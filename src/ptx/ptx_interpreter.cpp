#include "ptxlab/ptx/ptx_interpreter.h"
#include <iomanip>
#include <sstream>
#include "ptxlab/ptx/ptx_parser.h"
#include "ptxlab/ptx/ptx_validator.h"

namespace ptxlab::ptx {

static std::string toHex(uint64_t val) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << val;
    return ss.str();
}

PTXInterpreter::PTXInterpreter(BufferManager& buffers, uint64_t maxCyclesPerThread)
    : buffers_(buffers), maxCyclesPerThread_(maxCyclesPerThread) {}

void PTXInterpreter::setupBufferMemory(const std::unordered_map<std::string, KernelArg>& args) {
    memoryMap_.clear();
    uint64_t currentBase = 0x10000000ULL;

    for (const auto& [paramName, arg] : args) {
        if (arg.kind == KernelArg::Kind::Buffer) {
            BufferRegion region;
            region.name = arg.bufferName;
            region.baseAddr = currentBase;

            // Try float buffer first, then int buffer
            try {
                const auto& fbuf = buffers_.getF32(arg.bufferName);
                region.sizeBytes = fbuf.size() * sizeof(float);
                region.isFloat = true;
            } catch (const UBTrap&) {
                const auto& ibuf = buffers_.getI32(arg.bufferName);
                region.sizeBytes = ibuf.size() * sizeof(int32_t);
                region.isFloat = false;
            }

            memoryMap_.push_back(region);
            currentBase += 0x10000000ULL;
        }
    }
}

const PTXInterpreter::BufferRegion* PTXInterpreter::findBufferRegion(uint64_t addr) const {
    for (const auto& region : memoryMap_) {
        if (addr >= region.baseAddr && addr < region.baseAddr + region.sizeBytes) {
            return &region;
        }
    }
    return nullptr;
}

PTXExecResult PTXInterpreter::launch(std::string_view ptxSource,
                                     std::string_view kernelName,
                                     int gridDim,
                                     int blockDim,
                                     const std::unordered_map<std::string, KernelArg>& args) {
    std::string parseErr;
    auto module = PTXParser::parseString(ptxSource, &parseErr);
    if (!module) {
        return {false, "PTX parse failed: " + parseErr, 0};
    }

    PTXValidator validator;
    if (!validator.validate(*module)) {
        std::ostringstream ss;
        ss << "PTX validation failed:\n";
        for (const auto& err : validator.errors()) {
            ss << "  " << err << "\n";
        }
        return {false, ss.str(), 0};
    }

    return launch(*module, kernelName, gridDim, blockDim, args);
}

PTXExecResult PTXInterpreter::launch(const PTXModule& module,
                                     std::string_view kernelName,
                                     int gridDim,
                                     int blockDim,
                                     const std::unordered_map<std::string, KernelArg>& args) {
    const PTXKernel* kernel = module.findKernel(kernelName);
    if (!kernel) {
        if (!module.kernels().empty()) {
            kernel = module.kernels()[0].get();
        } else {
            return {false, "Kernel '" + std::string(kernelName) + "' not found in PTX module", 0};
        }
    }

    return launch(*kernel, gridDim, blockDim, args);
}

PTXExecResult PTXInterpreter::launch(const PTXKernel& kernel,
                                     int gridDim,
                                     int blockDim,
                                     const std::unordered_map<std::string, KernelArg>& args) {
    setupBufferMemory(args);

    // Pre-extract instructions and label indices
    std::unordered_map<std::string, size_t> labelIndices;
    std::vector<PTXInstruction> instructions;
    for (const auto& stmt : kernel.statements()) {
        if (stmt.kind == PTXStatement::Kind::Label) {
            labelIndices[stmt.labelName] = instructions.size();
        } else {
            instructions.push_back(stmt.inst);
        }
    }

    uint64_t totalCycles = 0;

    try {
        for (int ctaid = 0; ctaid < gridDim; ++ctaid) {
            for (int tid = 0; tid < blockDim; ++tid) {
                uint64_t threadCycles = 0;
                executeThread(kernel, ctaid, tid, blockDim, instructions, labelIndices, args, threadCycles);
                totalCycles += threadCycles;
            }
        }
    } catch (const UBTrap& trap) {
        return {false, trap.message(), totalCycles};
    } catch (const std::exception& ex) {
        return {false, ex.what(), totalCycles};
    }

    return {true, "", totalCycles};
}

void PTXInterpreter::executeThread(const PTXKernel& kernel,
                                   int ctaid,
                                   int tid,
                                   int blockDim,
                                   const std::vector<PTXInstruction>& instructions,
                                   const std::unordered_map<std::string, size_t>& labelIndices,
                                   const std::unordered_map<std::string, KernelArg>& args,
                                   uint64_t& threadCycles) {
    ThreadContext ctx;
    ctx.tidX = tid;
    ctx.ctaidX = ctaid;
    ctx.ntidX = blockDim;

    size_t pc = 0;
    while (pc < instructions.size()) {
        if (++threadCycles > maxCyclesPerThread_) {
            throw UBTrap("PTX interpreter execution cycle limit exceeded (infinite loop detected)");
        }

        const auto& inst = instructions[pc];

        // Predication mask check
        if (inst.predicate) {
            bool predVal = false;
            auto it = ctx.pRegs.find(inst.predicate->regName);
            if (it != ctx.pRegs.end()) {
                predVal = it->second;
            }
            if (inst.predicate->negated) {
                predVal = !predVal;
            }
            if (!predVal) {
                // Predicate false -> skip instruction
                pc++;
                continue;
            }
        }

        switch (inst.opcode) {
            case PTXOpcode::Ret: {
                return;
            }

            case PTXOpcode::Bra: {
                const std::string& target = inst.operands[0].labelName;
                auto it = labelIndices.find(target);
                if (it == labelIndices.end()) {
                    throw UBTrap("Jump to undefined label '" + target + "'");
                }
                pc = it->second;
                continue; // Do not pc++
            }

            case PTXOpcode::Mov: {
                if (inst.typeMod == PTXType::F32) {
                    writeF32(ctx, inst.operands[0], readF32(ctx, inst.operands[1]));
                } else if (inst.typeMod == PTXType::Pred) {
                    writePred(ctx, inst.operands[0], readPred(ctx, inst.operands[1]));
                } else if (inst.typeMod == PTXType::U64 || inst.typeMod == PTXType::S64 || inst.typeMod == PTXType::B64) {
                    writeU64(ctx, inst.operands[0], readU64(ctx, inst.operands[1]));
                } else {
                    writeU32(ctx, inst.operands[0], readU32(ctx, inst.operands[1]));
                }
                break;
            }

            case PTXOpcode::LdParam: {
                std::string pName = inst.operands[1].memBase;
                std::string argKey = pName;
                if (argKey.rfind("_param_", 0) == 0) {
                    argKey = argKey.substr(7);
                }

                auto it = args.find(argKey);
                if (it == args.end()) {
                    // Try looking for direct match
                    it = args.find(pName);
                }

                if (inst.typeMod == PTXType::U64 || inst.typeMod == PTXType::S64 || inst.typeMod == PTXType::B64) {
                    // Pointer argument
                    uint64_t addr = 0;
                    for (const auto& reg : memoryMap_) {
                        if (reg.name == argKey || reg.name == pName) {
                            addr = reg.baseAddr;
                            break;
                        }
                    }
                    writeU64(ctx, inst.operands[0], addr);
                } else if (inst.typeMod == PTXType::F32) {
                    float val = (it != args.end()) ? it->second.f32Val : 0.0f;
                    writeF32(ctx, inst.operands[0], val);
                } else if (inst.typeMod == PTXType::Pred) {
                    bool val = (it != args.end()) ? it->second.boolVal : false;
                    writePred(ctx, inst.operands[0], val);
                } else {
                    int32_t val = (it != args.end()) ? it->second.i32Val : 0;
                    writeS32(ctx, inst.operands[0], val);
                }
                break;
            }

            case PTXOpcode::Cvta: {
                writeU64(ctx, inst.operands[0], readU64(ctx, inst.operands[1]));
                break;
            }

            case PTXOpcode::Cvt: {
                if (inst.typeMod == PTXType::F32 && inst.srcTypeMod == PTXType::S32) {
                    int32_t srcVal = readS32(ctx, inst.operands[1]);
                    writeF32(ctx, inst.operands[0], static_cast<float>(srcVal));
                } else if (inst.typeMod == PTXType::S32 && inst.srcTypeMod == PTXType::F32) {
                    float srcVal = readF32(ctx, inst.operands[1]);
                    writeS32(ctx, inst.operands[0], static_cast<int32_t>(srcVal));
                }
                break;
            }

            case PTXOpcode::Add: {
                if (inst.typeMod == PTXType::F32) {
                    writeF32(ctx, inst.operands[0], readF32(ctx, inst.operands[1]) + readF32(ctx, inst.operands[2]));
                } else if (inst.typeMod == PTXType::S64 || inst.typeMod == PTXType::U64 || inst.typeMod == PTXType::B64) {
                    writeU64(ctx, inst.operands[0], readU64(ctx, inst.operands[1]) + readU64(ctx, inst.operands[2]));
                } else {
                    uint32_t a = readU32(ctx, inst.operands[1]);
                    uint32_t b = readU32(ctx, inst.operands[2]);
                    writeU32(ctx, inst.operands[0], a + b);
                }
                break;
            }

            case PTXOpcode::Sub: {
                if (inst.typeMod == PTXType::F32) {
                    writeF32(ctx, inst.operands[0], readF32(ctx, inst.operands[1]) - readF32(ctx, inst.operands[2]));
                } else {
                    uint32_t a = readU32(ctx, inst.operands[1]);
                    uint32_t b = readU32(ctx, inst.operands[2]);
                    writeU32(ctx, inst.operands[0], a - b);
                }
                break;
            }

            case PTXOpcode::Mul: {
                if (inst.typeMod == PTXType::F32) {
                    writeF32(ctx, inst.operands[0], readF32(ctx, inst.operands[1]) * readF32(ctx, inst.operands[2]));
                } else {
                    uint32_t a = readU32(ctx, inst.operands[1]);
                    uint32_t b = readU32(ctx, inst.operands[2]);
                    writeU32(ctx, inst.operands[0], a * b);
                }
                break;
            }

            case PTXOpcode::MulWide: {
                int64_t a = static_cast<int64_t>(readS32(ctx, inst.operands[1]));
                int64_t b = static_cast<int64_t>(readS32(ctx, inst.operands[2]));
                writeU64(ctx, inst.operands[0], static_cast<uint64_t>(a * b));
                break;
            }

            case PTXOpcode::Div: {
                if (inst.typeMod == PTXType::F32) {
                    writeF32(ctx, inst.operands[0], readF32(ctx, inst.operands[1]) / readF32(ctx, inst.operands[2]));
                } else {
                    int32_t a = readS32(ctx, inst.operands[1]);
                    int32_t b = readS32(ctx, inst.operands[2]);
                    if (b == 0) throw UBTrap("Integer division by zero in PTX interpreter");
                    if (a == INT32_MIN && b == -1) throw UBTrap("Signed integer division overflow in PTX interpreter");
                    writeS32(ctx, inst.operands[0], a / b);
                }
                break;
            }

            case PTXOpcode::Rem: {
                int32_t a = readS32(ctx, inst.operands[1]);
                int32_t b = readS32(ctx, inst.operands[2]);
                if (b == 0) throw UBTrap("Integer remainder by zero in PTX interpreter");
                if (a == INT32_MIN && b == -1) {
                    writeS32(ctx, inst.operands[0], 0);
                } else {
                    writeS32(ctx, inst.operands[0], a % b);
                }
                break;
            }

            case PTXOpcode::And: {
                writeU32(ctx, inst.operands[0], readU32(ctx, inst.operands[1]) & readU32(ctx, inst.operands[2]));
                break;
            }

            case PTXOpcode::Or: {
                writeU32(ctx, inst.operands[0], readU32(ctx, inst.operands[1]) | readU32(ctx, inst.operands[2]));
                break;
            }

            case PTXOpcode::Xor: {
                writeU32(ctx, inst.operands[0], readU32(ctx, inst.operands[1]) ^ readU32(ctx, inst.operands[2]));
                break;
            }

            case PTXOpcode::Shl: {
                uint32_t a = readU32(ctx, inst.operands[1]);
                uint32_t b = readU32(ctx, inst.operands[2]) & 31u;
                writeU32(ctx, inst.operands[0], a << b);
                break;
            }

            case PTXOpcode::Shr: {
                int32_t a = readS32(ctx, inst.operands[1]);
                uint32_t b = readU32(ctx, inst.operands[2]) & 31u;
                writeS32(ctx, inst.operands[0], a >> b);
                break;
            }

            case PTXOpcode::Neg: {
                if (inst.typeMod == PTXType::F32) {
                    writeF32(ctx, inst.operands[0], -readF32(ctx, inst.operands[1]));
                } else {
                    int32_t a = readS32(ctx, inst.operands[1]);
                    writeS32(ctx, inst.operands[0], static_cast<int32_t>(0u - static_cast<uint32_t>(a)));
                }
                break;
            }

            case PTXOpcode::Not: {
                if (inst.typeMod == PTXType::Pred) {
                    writePred(ctx, inst.operands[0], !readPred(ctx, inst.operands[1]));
                } else {
                    writeU32(ctx, inst.operands[0], ~readU32(ctx, inst.operands[1]));
                }
                break;
            }

            case PTXOpcode::Setp: {
                bool cond = false;
                if (inst.typeMod == PTXType::F32) {
                    float a = readF32(ctx, inst.operands[1]);
                    float b = readF32(ctx, inst.operands[2]);
                    switch (inst.cmpCond) {
                        case PTXCmpCondition::Eq: cond = (a == b); break;
                        case PTXCmpCondition::Ne: cond = (a != b); break;
                        case PTXCmpCondition::Lt: cond = (a < b); break;
                        case PTXCmpCondition::Le: cond = (a <= b); break;
                        case PTXCmpCondition::Gt: cond = (a > b); break;
                        case PTXCmpCondition::Ge: cond = (a >= b); break;
                    }
                } else {
                    int32_t a = readS32(ctx, inst.operands[1]);
                    int32_t b = readS32(ctx, inst.operands[2]);
                    switch (inst.cmpCond) {
                        case PTXCmpCondition::Eq: cond = (a == b); break;
                        case PTXCmpCondition::Ne: cond = (a != b); break;
                        case PTXCmpCondition::Lt: cond = (a < b); break;
                        case PTXCmpCondition::Le: cond = (a <= b); break;
                        case PTXCmpCondition::Gt: cond = (a > b); break;
                        case PTXCmpCondition::Ge: cond = (a >= b); break;
                    }
                }
                writePred(ctx, inst.operands[0], cond);
                break;
            }

            case PTXOpcode::LdGlobal: {
                uint64_t baseAddr = readU64(ctx, PTXOperand::reg(inst.operands[1].memBase));
                uint64_t addr = baseAddr + static_cast<uint64_t>(inst.operands[1].memOffset);
                const BufferRegion* region = findBufferRegion(addr);
                if (!region) {
                    throw UBTrap("Invalid global memory load address: 0x" + toHex(addr));
                }
                uint64_t byteOffset = addr - region->baseAddr;
                int32_t elemIdx = static_cast<int32_t>(byteOffset / 4);

                if (inst.typeMod == PTXType::F32) {
                    float val = buffers_.readF32(region->name, elemIdx);
                    writeF32(ctx, inst.operands[0], val);
                } else {
                    int32_t val = buffers_.readI32(region->name, elemIdx);
                    writeS32(ctx, inst.operands[0], val);
                }
                break;
            }

            case PTXOpcode::StGlobal: {
                uint64_t baseAddr = readU64(ctx, PTXOperand::reg(inst.operands[0].memBase));
                uint64_t addr = baseAddr + static_cast<uint64_t>(inst.operands[0].memOffset);
                const BufferRegion* region = findBufferRegion(addr);
                if (!region) {
                    throw UBTrap("Invalid global memory store address: 0x" + toHex(addr));
                }
                uint64_t byteOffset = addr - region->baseAddr;
                int32_t elemIdx = static_cast<int32_t>(byteOffset / 4);

                if (inst.typeMod == PTXType::F32) {
                    float val = readF32(ctx, inst.operands[1]);
                    buffers_.writeF32(region->name, elemIdx, val);
                } else {
                    int32_t val = readS32(ctx, inst.operands[1]);
                    buffers_.writeI32(region->name, elemIdx, val);
                }
                break;
            }
        }

        pc++;
    }
}

uint32_t PTXInterpreter::readU32(const ThreadContext& ctx, const PTXOperand& op) const {
    if (op.kind == PTXOperandKind::Register) {
        auto it = ctx.rRegs.find(op.regName);
        if (it != ctx.rRegs.end()) return it->second;
        return 0;
    }
    if (op.kind == PTXOperandKind::SpecialReg) {
        if (op.specialReg == PTXSpecialReg::TidX) return static_cast<uint32_t>(ctx.tidX);
        if (op.specialReg == PTXSpecialReg::CtaidX) return static_cast<uint32_t>(ctx.ctaidX);
        if (op.specialReg == PTXSpecialReg::NtidX) return static_cast<uint32_t>(ctx.ntidX);
    }
    if (op.kind == PTXOperandKind::ImmediateInt) {
        return static_cast<uint32_t>(op.intVal);
    }
    return 0;
}

int32_t PTXInterpreter::readS32(const ThreadContext& ctx, const PTXOperand& op) const {
    return static_cast<int32_t>(readU32(ctx, op));
}

float PTXInterpreter::readF32(const ThreadContext& ctx, const PTXOperand& op) const {
    if (op.kind == PTXOperandKind::Register) {
        auto it = ctx.fRegs.find(op.regName);
        if (it != ctx.fRegs.end()) return it->second;
        return 0.0f;
    }
    if (op.kind == PTXOperandKind::ImmediateFloat) {
        return op.floatVal;
    }
    return 0.0f;
}

uint64_t PTXInterpreter::readU64(const ThreadContext& ctx, const PTXOperand& op) const {
    if (op.kind == PTXOperandKind::Register) {
        auto it = ctx.rdRegs.find(op.regName);
        if (it != ctx.rdRegs.end()) return it->second;
        return 0;
    }
    if (op.kind == PTXOperandKind::ImmediateInt) {
        return static_cast<uint64_t>(op.intVal);
    }
    return 0;
}

bool PTXInterpreter::readPred(const ThreadContext& ctx, const PTXOperand& op) const {
    if (op.kind == PTXOperandKind::Register) {
        auto it = ctx.pRegs.find(op.regName);
        if (it != ctx.pRegs.end()) return it->second;
        return false;
    }
    if (op.kind == PTXOperandKind::ImmediatePred) {
        return op.boolVal;
    }
    if (op.kind == PTXOperandKind::ImmediateInt) {
        return op.intVal != 0;
    }
    return false;
}

void PTXInterpreter::writeU32(ThreadContext& ctx, const PTXOperand& op, uint32_t val) {
    if (op.kind == PTXOperandKind::Register) {
        ctx.rRegs[op.regName] = val;
    }
}

void PTXInterpreter::writeS32(ThreadContext& ctx, const PTXOperand& op, int32_t val) {
    writeU32(ctx, op, static_cast<uint32_t>(val));
}

void PTXInterpreter::writeF32(ThreadContext& ctx, const PTXOperand& op, float val) {
    if (op.kind == PTXOperandKind::Register) {
        ctx.fRegs[op.regName] = val;
    }
}

void PTXInterpreter::writeU64(ThreadContext& ctx, const PTXOperand& op, uint64_t val) {
    if (op.kind == PTXOperandKind::Register) {
        ctx.rdRegs[op.regName] = val;
    }
}

void PTXInterpreter::writePred(ThreadContext& ctx, const PTXOperand& op, bool val) {
    if (op.kind == PTXOperandKind::Register) {
        ctx.pRegs[op.regName] = val;
    }
}

} // namespace ptxlab::ptx
