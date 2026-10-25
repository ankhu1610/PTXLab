#include "ptxlab/ptx/ptx_validator.h"
#include <charconv>

namespace ptxlab::ptx {

PTXValidator::PTXValidator(DiagnosticEngine* diag) : diag_(diag) {}

void PTXValidator::error(std::string msg, int line) {
    errors_.push_back(msg);
    if (diag_) {
        diag_->error(SourceLocation{static_cast<uint32_t>(line), 1}, std::move(msg));
    }
}

bool PTXValidator::validate(const PTXModule& module) {
    bool ok = true;
    if (!validateHeader(module)) {
        ok = false;
    }

    if (module.kernels().empty()) {
        error("PTX module must contain at least one kernel entry", 1);
        ok = false;
    }

    for (const auto& kernel : module.kernels()) {
        if (!validateKernel(*kernel)) {
            ok = false;
        }
    }

    return ok && errors_.empty();
}

bool PTXValidator::validateHeader(const PTXModule& module) {
    bool ok = true;

    if (module.version().empty()) {
        error("Missing required directive .version", 1);
        ok = false;
    } else {
        // Must be version >= 7.0
        float ver = 0.0f;
        auto res = std::from_chars(module.version().data(), module.version().data() + module.version().size(), ver);
        if (res.ec != std::errc() || ver < 7.0f) {
            error("PTX .version must be >= 7.0 (found " + module.version() + ")", 1);
            ok = false;
        }
    }

    if (module.target().empty()) {
        error("Missing required directive .target", 1);
        ok = false;
    } else if (module.target().rfind("sm_", 0) != 0 && module.target().rfind("compute_", 0) != 0) {
        error("PTX .target must specify a valid architecture like sm_75 or sm_120 (found " + module.target() + ")", 1);
        ok = false;
    }

    if (module.addressSize() == 0) {
        error("Missing required directive .address_size", 1);
        ok = false;
    } else if (module.addressSize() != 64) {
        error("Unsupported .address_size: " + std::to_string(module.addressSize()) + " (only 64 is supported)", 1);
        ok = false;
    }

    return ok;
}

static bool is32BitIntType(PTXType t) {
    return t == PTXType::U32 || t == PTXType::S32 || t == PTXType::B32;
}

static bool is64BitType(PTXType t) {
    return t == PTXType::U64 || t == PTXType::S64 || t == PTXType::B64;
}

bool PTXValidator::validateKernel(const PTXKernel& kernel) {
    bool ok = true;

    // Collect parameters
    std::unordered_set<std::string> paramNames;
    for (const auto& param : kernel.params()) {
        if (param.name.empty()) {
            error("Kernel parameter has empty name", param.line);
            ok = false;
        } else if (!paramNames.insert(param.name).second) {
            error("Duplicate parameter name '" + param.name + "' in kernel '" + kernel.name() + "'", param.line);
            ok = false;
        }
        if (param.type == PTXType::Unknown) {
            error("Invalid type for parameter '" + param.name + "'", param.line);
            ok = false;
        }
    }

    // Collect register declarations and populate symbol table
    std::unordered_map<std::string, PTXType> declaredRegs;
    for (const auto& regDecl : kernel.regDecls()) {
        if (regDecl.isRange) {
            for (int i = 0; i < regDecl.count; ++i) {
                std::string fullReg = regDecl.nameOrPrefix + std::to_string(i);
                if (!declaredRegs.emplace(fullReg, regDecl.type).second) {
                    error("Redeclaration of register '" + fullReg + "'", regDecl.line);
                    ok = false;
                }
            }
        } else {
            if (!declaredRegs.emplace(regDecl.nameOrPrefix, regDecl.type).second) {
                error("Redeclaration of register '" + regDecl.nameOrPrefix + "'", regDecl.line);
                ok = false;
            }
        }
    }

    // Collect labels and check uniqueness
    std::unordered_set<std::string> labels;
    for (const auto& stmt : kernel.statements()) {
        if (stmt.kind == PTXStatement::Kind::Label) {
            if (!labels.insert(stmt.labelName).second) {
                error("Duplicate label definition '" + stmt.labelName + "' in kernel '" + kernel.name() + "'", stmt.line);
                ok = false;
            }
        }
    }

    // Validate instructions
    for (const auto& stmt : kernel.statements()) {
        if (stmt.kind == PTXStatement::Kind::Instruction) {
            if (!validateInstruction(kernel, stmt.inst, declaredRegs, labels, paramNames)) {
                ok = false;
            }
        }
    }

    return ok;
}

bool PTXValidator::validateInstruction(const PTXKernel& kernel,
                                       const PTXInstruction& inst,
                                       const std::unordered_map<std::string, PTXType>& declaredRegs,
                                       const std::unordered_set<std::string>& labels,
                                       const std::unordered_set<std::string>& params) {
    bool ok = true;

    // Check predicate register if present
    if (inst.predicate) {
        auto it = declaredRegs.find(inst.predicate->regName);
        if (it == declaredRegs.end()) {
            error("Undeclared predicate register '" + inst.predicate->regName + "'", inst.line);
            ok = false;
        } else if (it->second != PTXType::Pred) {
            error("Predicate mask must be a predicate register (.pred), got " + std::string(ptxTypeToString(it->second)), inst.line);
            ok = false;
        }
    }

    auto checkReg = [&](const std::string& regName, PTXType expectedType, std::string_view role) -> bool {
        auto it = declaredRegs.find(regName);
        if (it == declaredRegs.end()) {
            error("Undeclared register '" + regName + "' used as " + std::string(role), inst.line);
            return false;
        }
        PTXType actual = it->second;
        if (expectedType == PTXType::S32 || expectedType == PTXType::U32 || expectedType == PTXType::B32) {
            if (!is32BitIntType(actual)) {
                error("Register '" + regName + "' used as 32-bit int, but declared as " + std::string(ptxTypeToString(actual)), inst.line);
                return false;
            }
        } else if (expectedType == PTXType::F32) {
            if (actual != PTXType::F32) {
                error("Register '" + regName + "' used as f32, but declared as " + std::string(ptxTypeToString(actual)), inst.line);
                return false;
            }
        } else if (expectedType == PTXType::U64 || expectedType == PTXType::S64 || expectedType == PTXType::B64) {
            if (!is64BitType(actual)) {
                error("Register '" + regName + "' used as 64-bit int/ptr, but declared as " + std::string(ptxTypeToString(actual)), inst.line);
                return false;
            }
        } else if (expectedType == PTXType::Pred) {
            if (actual != PTXType::Pred) {
                error("Register '" + regName + "' used as pred, but declared as " + std::string(ptxTypeToString(actual)), inst.line);
                return false;
            }
        }
        return true;
    };

    switch (inst.opcode) {
        case PTXOpcode::Ret: {
            if (!inst.operands.empty()) {
                error("ret instruction takes 0 operands", inst.line);
                ok = false;
            }
            break;
        }

        case PTXOpcode::Bra: {
            if (inst.operands.size() != 1) {
                error("bra instruction takes exactly 1 operand (target label)", inst.line);
                ok = false;
            } else if (inst.operands[0].kind != PTXOperandKind::LabelRef) {
                error("bra target must be a label identifier", inst.line);
                ok = false;
            } else {
                const std::string& target = inst.operands[0].labelName;
                if (labels.find(target) == labels.end()) {
                    error("Undefined branch target label '" + target + "'", inst.line);
                    ok = false;
                }
            }
            break;
        }

        case PTXOpcode::Mov: {
            if (inst.operands.size() != 2) {
                error("mov instruction takes exactly 2 operands (dst, src)", inst.line);
                return false;
            }
            if (inst.operands[0].kind != PTXOperandKind::Register) {
                error("Destination of mov must be a register", inst.line);
                ok = false;
            } else {
                if (!checkReg(inst.operands[0].regName, inst.typeMod, "destination")) ok = false;
            }

            if (inst.operands[1].kind == PTXOperandKind::Register) {
                if (!checkReg(inst.operands[1].regName, inst.typeMod, "source")) ok = false;
            } else if (inst.operands[1].kind == PTXOperandKind::SpecialReg) {
                if (!is32BitIntType(inst.typeMod)) {
                    error("Special registers must be read into 32-bit integer registers", inst.line);
                    ok = false;
                }
            }
            break;
        }

        case PTXOpcode::LdParam: {
            if (inst.operands.size() != 2) {
                error("ld.param takes exactly 2 operands (dst, [param])", inst.line);
                return false;
            }
            if (inst.operands[0].kind != PTXOperandKind::Register) {
                error("Destination of ld.param must be a register", inst.line);
                ok = false;
            } else {
                if (!checkReg(inst.operands[0].regName, inst.typeMod, "destination")) ok = false;
            }

            if (inst.operands[1].kind != PTXOperandKind::MemoryRef) {
                error("Source of ld.param must be parameter memory reference [name]", inst.line);
                ok = false;
            } else {
                const std::string& pName = inst.operands[1].memBase;
                if (params.find(pName) == params.end()) {
                    error("ld.param references undeclared parameter '" + pName + "'", inst.line);
                    ok = false;
                }
            }
            break;
        }

        case PTXOpcode::Cvta: {
            if (inst.operands.size() != 2) {
                error("cvta.to.global.u64 takes exactly 2 operands (dst, src)", inst.line);
                return false;
            }
            if (!checkReg(inst.operands[0].regName, PTXType::U64, "destination")) ok = false;
            if (!checkReg(inst.operands[1].regName, PTXType::U64, "source")) ok = false;
            break;
        }

        case PTXOpcode::Cvt: {
            if (inst.operands.size() != 2) {
                error("cvt instruction takes exactly 2 operands (dst, src)", inst.line);
                return false;
            }
            if (!checkReg(inst.operands[0].regName, inst.typeMod, "destination")) ok = false;
            if (inst.operands[1].kind == PTXOperandKind::Register) {
                if (!checkReg(inst.operands[1].regName, inst.srcTypeMod, "source")) ok = false;
            }
            break;
        }

        case PTXOpcode::Add:
        case PTXOpcode::Sub:
        case PTXOpcode::Mul:
        case PTXOpcode::Div:
        case PTXOpcode::Rem:
        case PTXOpcode::And:
        case PTXOpcode::Or:
        case PTXOpcode::Xor:
        case PTXOpcode::Shl:
        case PTXOpcode::Shr: {
            if (inst.operands.size() != 3) {
                error("Binary arithmetic instruction takes exactly 3 operands (dst, src1, src2)", inst.line);
                return false;
            }
            if (inst.operands[0].kind != PTXOperandKind::Register) {
                error("Destination of binary operation must be a register", inst.line);
                ok = false;
            } else {
                if (!checkReg(inst.operands[0].regName, inst.typeMod, "destination")) ok = false;
            }

            for (size_t opIdx = 1; opIdx <= 2; ++opIdx) {
                if (inst.operands[opIdx].kind == PTXOperandKind::Register) {
                    if (!checkReg(inst.operands[opIdx].regName, inst.typeMod, "source")) ok = false;
                }
            }
            break;
        }

        case PTXOpcode::MulWide: {
            if (inst.operands.size() != 3) {
                error("mul.wide.s32 takes exactly 3 operands (dst_rd, src_r, imm)", inst.line);
                return false;
            }
            if (!checkReg(inst.operands[0].regName, PTXType::S64, "destination")) ok = false;
            if (inst.operands[1].kind == PTXOperandKind::Register) {
                if (!checkReg(inst.operands[1].regName, PTXType::S32, "source")) ok = false;
            }
            break;
        }

        case PTXOpcode::Neg:
        case PTXOpcode::Not: {
            if (inst.operands.size() != 2) {
                error("Unary instruction takes exactly 2 operands (dst, src)", inst.line);
                return false;
            }
            if (!checkReg(inst.operands[0].regName, inst.typeMod, "destination")) ok = false;
            if (inst.operands[1].kind == PTXOperandKind::Register) {
                if (!checkReg(inst.operands[1].regName, inst.typeMod, "source")) ok = false;
            }
            break;
        }

        case PTXOpcode::Setp: {
            if (inst.operands.size() != 3) {
                error("setp instruction takes exactly 3 operands (dst_pred, src1, src2)", inst.line);
                return false;
            }
            if (!checkReg(inst.operands[0].regName, PTXType::Pred, "predicate destination")) ok = false;
            for (size_t opIdx = 1; opIdx <= 2; ++opIdx) {
                if (inst.operands[opIdx].kind == PTXOperandKind::Register) {
                    if (!checkReg(inst.operands[opIdx].regName, inst.typeMod, "source operand")) ok = false;
                }
            }
            break;
        }

        case PTXOpcode::LdGlobal: {
            if (inst.operands.size() != 2) {
                error("ld.global takes exactly 2 operands (dst, [addr])", inst.line);
                return false;
            }
            if (!checkReg(inst.operands[0].regName, inst.typeMod, "destination")) ok = false;
            if (inst.operands[1].kind != PTXOperandKind::MemoryRef) {
                error("ld.global source must be a memory reference [reg]", inst.line);
                ok = false;
            } else {
                if (!checkReg(inst.operands[1].memBase, PTXType::U64, "address register")) ok = false;
            }
            break;
        }

        case PTXOpcode::StGlobal: {
            if (inst.operands.size() != 2) {
                error("st.global takes exactly 2 operands ([addr], src)", inst.line);
                return false;
            }
            if (inst.operands[0].kind != PTXOperandKind::MemoryRef) {
                error("st.global destination must be a memory reference [reg]", inst.line);
                ok = false;
            } else {
                if (!checkReg(inst.operands[0].memBase, PTXType::U64, "address register")) ok = false;
            }
            if (inst.operands[1].kind == PTXOperandKind::Register) {
                if (!checkReg(inst.operands[1].regName, inst.typeMod, "source")) ok = false;
            }
            break;
        }
    }

    return ok;
}

} // namespace ptxlab::ptx
