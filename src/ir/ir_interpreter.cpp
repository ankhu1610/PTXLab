#include "ptxlab/ir/ir_interpreter.h"
#include <climits>

namespace ptxlab::ir {

IRInterpreter::IRInterpreter(BufferManager& buffers, uint64_t maxSteps)
    : buffers_(buffers), maxSteps_(maxSteps) {}

void IRInterpreter::setDst(const Instruction& inst, Value val) {
    if (!inst.hasDst()) return;
    if (inst.dst().type.isPred()) {
        predicates_[inst.dst().regId] = val.b;
    } else {
        registers_[inst.dst().regId] = std::move(val);
    }
}

Value IRInterpreter::readOperand(const IROperand& op, SourceLocation loc) {
    if (op.isReg()) {
        if (op.type.isPred()) {
            auto it = predicates_.find(op.regId);
            if (it == predicates_.end()) {
                throw UBTrap("read of uninitialized predicate register " + op.toString(), loc);
            }
            return Value::makeBool(it->second);
        }
        auto it = registers_.find(op.regId);
        if (it == registers_.end()) {
            throw UBTrap("read of uninitialized virtual register " + op.toString(), loc);
        }
        return it->second;
    }
    if (op.kind == IROperand::Kind::ConstantInt) {
        return Value::makeI32(op.intVal);
    }
    if (op.kind == IROperand::Kind::ConstantFloat) {
        return Value::makeF32(op.floatVal);
    }
    return Value::makeBool(op.boolVal);
}

void IRInterpreter::launch1D(const Function& function,
                             int gridDim,
                             int blockDim,
                             const std::unordered_map<std::string, KernelArg>& args) {
    ntid_x_ = blockDim;

    for (int block = 0; block < gridDim; ++block) {
        ctaid_x_ = block;
        for (int thread = 0; thread < blockDim; ++thread) {
            tid_x_ = thread;
            stepCount_ = 0;
            registers_.clear();
            predicates_.clear();

            const BasicBlock* curBB = function.entryBlock();

            while (curBB) {
                const Instruction* terminator = nullptr;

                for (const auto& inst : curBB->instructions()) {
                    if (++stepCount_ > maxSteps_) {
                        throw UBTrap("step limit exceeded (infinite loop)", inst->location());
                    }

                    if (inst->isTerminator()) {
                        terminator = inst.get();
                        break;
                    }

                    // Normal instruction execution
                    switch (inst->opcode()) {
                        case Opcode::Const:
                        case Opcode::Copy:
                            setDst(*inst, readOperand(inst->operands()[0], inst->location()));
                            break;

                        case Opcode::Param: {
                            int idx = inst->operands()[0].intVal;
                            const auto& param = function.params()[idx];
                            auto it = args.find(param.name);
                            if (it == args.end()) {
                                throw UBTrap("missing argument for parameter '" + param.name + "'", inst->location());
                            }
                            const auto& arg = it->second;
                            if (param.type.isPtr()) {
                                setDst(*inst, Value::makePtr(arg.bufferName, 0));
                            } else if (param.type.isInt()) {
                                setDst(*inst, Value::makeI32(arg.i32Val));
                            } else if (param.type.isFloat()) {
                                setDst(*inst, Value::makeF32(arg.f32Val));
                            } else if (param.type.isPred()) {
                                setDst(*inst, Value::makeBool(arg.boolVal));
                            }
                            break;
                        }

                        case Opcode::Special:
                            switch (inst->specialKind()) {
                                case SpecialKind::TidX:
                                    setDst(*inst, Value::makeI32(tid_x_));
                                    break;
                                case SpecialKind::CtaidX:
                                    setDst(*inst, Value::makeI32(ctaid_x_));
                                    break;
                                case SpecialKind::NtidX:
                                    setDst(*inst, Value::makeI32(ntid_x_));
                                    break;
                            }
                            break;

                        // Integer binary
                        case Opcode::Add: {
                            uint32_t a = static_cast<uint32_t>(readOperand(inst->operands()[0], inst->location()).i32);
                            uint32_t b = static_cast<uint32_t>(readOperand(inst->operands()[1], inst->location()).i32);
                            setDst(*inst, Value::makeI32(static_cast<int32_t>(a + b)));
                            break;
                        }
                        case Opcode::Sub: {
                            uint32_t a = static_cast<uint32_t>(readOperand(inst->operands()[0], inst->location()).i32);
                            uint32_t b = static_cast<uint32_t>(readOperand(inst->operands()[1], inst->location()).i32);
                            setDst(*inst, Value::makeI32(static_cast<int32_t>(a - b)));
                            break;
                        }
                        case Opcode::Mul: {
                            uint32_t a = static_cast<uint32_t>(readOperand(inst->operands()[0], inst->location()).i32);
                            uint32_t b = static_cast<uint32_t>(readOperand(inst->operands()[1], inst->location()).i32);
                            setDst(*inst, Value::makeI32(static_cast<int32_t>(a * b)));
                            break;
                        }
                        case Opcode::Div: {
                            int32_t a = readOperand(inst->operands()[0], inst->location()).i32;
                            int32_t b = readOperand(inst->operands()[1], inst->location()).i32;
                            if (b == 0) {
                                throw UBTrap("division by zero", inst->location());
                            }
                            if (a == INT32_MIN && b == -1) {
                                throw UBTrap("signed integer division overflow", inst->location());
                            }
                            setDst(*inst, Value::makeI32(a / b));
                            break;
                        }
                        case Opcode::Rem: {
                            int32_t a = readOperand(inst->operands()[0], inst->location()).i32;
                            int32_t b = readOperand(inst->operands()[1], inst->location()).i32;
                            if (b == 0) {
                                throw UBTrap("modulo by zero", inst->location());
                            }
                            if (a == INT32_MIN && b == -1) {
                                throw UBTrap("signed integer division overflow", inst->location());
                            }
                            setDst(*inst, Value::makeI32(a % b));
                            break;
                        }
                        case Opcode::And: {
                            int32_t a = readOperand(inst->operands()[0], inst->location()).i32;
                            int32_t b = readOperand(inst->operands()[1], inst->location()).i32;
                            setDst(*inst, Value::makeI32(a & b));
                            break;
                        }
                        case Opcode::Or: {
                            int32_t a = readOperand(inst->operands()[0], inst->location()).i32;
                            int32_t b = readOperand(inst->operands()[1], inst->location()).i32;
                            setDst(*inst, Value::makeI32(a | b));
                            break;
                        }
                        case Opcode::Xor: {
                            int32_t a = readOperand(inst->operands()[0], inst->location()).i32;
                            int32_t b = readOperand(inst->operands()[1], inst->location()).i32;
                            setDst(*inst, Value::makeI32(a ^ b));
                            break;
                        }
                        case Opcode::Shl: {
                            uint32_t a = static_cast<uint32_t>(readOperand(inst->operands()[0], inst->location()).i32);
                            int32_t b = readOperand(inst->operands()[1], inst->location()).i32;
                            if (b < 0 || b >= 32) {
                                throw UBTrap("shift count out of range [0, 31]: " + std::to_string(b), inst->location());
                            }
                            setDst(*inst, Value::makeI32(static_cast<int32_t>(a << b)));
                            break;
                        }
                        case Opcode::Shr: {
                            int32_t a = readOperand(inst->operands()[0], inst->location()).i32;
                            int32_t b = readOperand(inst->operands()[1], inst->location()).i32;
                            if (b < 0 || b >= 32) {
                                throw UBTrap("shift count out of range [0, 31]: " + std::to_string(b), inst->location());
                            }
                            setDst(*inst, Value::makeI32(a >> b));
                            break;
                        }

                        // Float binary
                        case Opcode::FAdd: {
                            float a = readOperand(inst->operands()[0], inst->location()).f32;
                            float b = readOperand(inst->operands()[1], inst->location()).f32;
                            setDst(*inst, Value::makeF32(a + b));
                            break;
                        }
                        case Opcode::FSub: {
                            float a = readOperand(inst->operands()[0], inst->location()).f32;
                            float b = readOperand(inst->operands()[1], inst->location()).f32;
                            setDst(*inst, Value::makeF32(a - b));
                            break;
                        }
                        case Opcode::FMul: {
                            float a = readOperand(inst->operands()[0], inst->location()).f32;
                            float b = readOperand(inst->operands()[1], inst->location()).f32;
                            setDst(*inst, Value::makeF32(a * b));
                            break;
                        }
                        case Opcode::FDiv: {
                            float a = readOperand(inst->operands()[0], inst->location()).f32;
                            float b = readOperand(inst->operands()[1], inst->location()).f32;
                            setDst(*inst, Value::makeF32(a / b));
                            break;
                        }

                        // Unary
                        case Opcode::Neg: {
                            uint32_t a = static_cast<uint32_t>(readOperand(inst->operands()[0], inst->location()).i32);
                            setDst(*inst, Value::makeI32(static_cast<int32_t>(0u - a)));
                            break;
                        }
                        case Opcode::FNeg: {
                            float a = readOperand(inst->operands()[0], inst->location()).f32;
                            setDst(*inst, Value::makeF32(-a));
                            break;
                        }
                        case Opcode::Not: {
                            Value a = readOperand(inst->operands()[0], inst->location());
                            if (a.kind == Value::Kind::Bool) {
                                setDst(*inst, Value::makeBool(!a.b));
                            } else {
                                setDst(*inst, Value::makeI32(~a.i32));
                            }
                            break;
                        }

                        // Comparisons
                        case Opcode::CmpEq: {
                            Value a = readOperand(inst->operands()[0], inst->location());
                            Value b = readOperand(inst->operands()[1], inst->location());
                            bool res = (a.kind == Value::Kind::F32) ? (a.f32 == b.f32) : (a.i32 == b.i32);
                            setDst(*inst, Value::makeBool(res));
                            break;
                        }
                        case Opcode::CmpNe: {
                            Value a = readOperand(inst->operands()[0], inst->location());
                            Value b = readOperand(inst->operands()[1], inst->location());
                            bool res = (a.kind == Value::Kind::F32) ? (a.f32 != b.f32) : (a.i32 != b.i32);
                            setDst(*inst, Value::makeBool(res));
                            break;
                        }
                        case Opcode::CmpLt: {
                            Value a = readOperand(inst->operands()[0], inst->location());
                            Value b = readOperand(inst->operands()[1], inst->location());
                            bool res = (a.kind == Value::Kind::F32) ? (a.f32 < b.f32) : (a.i32 < b.i32);
                            setDst(*inst, Value::makeBool(res));
                            break;
                        }
                        case Opcode::CmpLe: {
                            Value a = readOperand(inst->operands()[0], inst->location());
                            Value b = readOperand(inst->operands()[1], inst->location());
                            bool res = (a.kind == Value::Kind::F32) ? (a.f32 <= b.f32) : (a.i32 <= b.i32);
                            setDst(*inst, Value::makeBool(res));
                            break;
                        }
                        case Opcode::CmpGt: {
                            Value a = readOperand(inst->operands()[0], inst->location());
                            Value b = readOperand(inst->operands()[1], inst->location());
                            bool res = (a.kind == Value::Kind::F32) ? (a.f32 > b.f32) : (a.i32 > b.i32);
                            setDst(*inst, Value::makeBool(res));
                            break;
                        }
                        case Opcode::CmpGe: {
                            Value a = readOperand(inst->operands()[0], inst->location());
                            Value b = readOperand(inst->operands()[1], inst->location());
                            bool res = (a.kind == Value::Kind::F32) ? (a.f32 >= b.f32) : (a.i32 >= b.i32);
                            setDst(*inst, Value::makeBool(res));
                            break;
                        }

                        // Conversions
                        case Opcode::CvtI32ToF32: {
                            int32_t a = readOperand(inst->operands()[0], inst->location()).i32;
                            setDst(*inst, Value::makeF32(static_cast<float>(a)));
                            break;
                        }
                        case Opcode::CvtF32ToI32: {
                            float a = readOperand(inst->operands()[0], inst->location()).f32;
                            if (std::isnan(a) || a < -2147483648.0f || a > 2147483647.0f) {
                                throw UBTrap("float to int conversion out of range or NaN: " + std::to_string(a), inst->location());
                            }
                            setDst(*inst, Value::makeI32(static_cast<int32_t>(a)));
                            break;
                        }

                        // Memory
                        case Opcode::ElemAddr: {
                            Value base = readOperand(inst->operands()[0], inst->location());
                            int32_t idx = readOperand(inst->operands()[1], inst->location()).i32;
                            setDst(*inst, Value::makePtr(base.ptrBuffer, base.ptrOffset + idx));
                            break;
                        }
                        case Opcode::Load: {
                            Value addr = readOperand(inst->operands()[0], inst->location());
                            if (inst->dst().type.isInt()) {
                                setDst(*inst, Value::makeI32(buffers_.readI32(addr.ptrBuffer, addr.ptrOffset, inst->location())));
                            } else {
                                setDst(*inst, Value::makeF32(buffers_.readF32(addr.ptrBuffer, addr.ptrOffset, inst->location())));
                            }
                            break;
                        }
                        case Opcode::Store: {
                            Value addr = readOperand(inst->operands()[0], inst->location());
                            Value val = readOperand(inst->operands()[1], inst->location());
                            if (val.kind == Value::Kind::I32) {
                                buffers_.writeI32(addr.ptrBuffer, addr.ptrOffset, val.i32, inst->location());
                            } else {
                                buffers_.writeF32(addr.ptrBuffer, addr.ptrOffset, val.f32, inst->location());
                            }
                            break;
                        }

                        default:
                            break;
                    }
                }

                if (!terminator) {
                    throw UBTrap("unterminated basic block execution: '" + curBB->label() + "'");
                }

                // Handle terminator
                if (terminator->opcode() == Opcode::Ret) {
                    curBB = nullptr;
                } else if (terminator->opcode() == Opcode::Br) {
                    curBB = function.getBlock(terminator->targetTrue());
                } else if (terminator->opcode() == Opcode::CondBr) {
                    Value cond = readOperand(terminator->operands()[0], terminator->location());
                    if (cond.b) {
                        curBB = function.getBlock(terminator->targetTrue());
                    } else {
                        curBB = function.getBlock(terminator->targetFalse());
                    }
                }
            }
        }
    }
}

} // namespace ptxlab::ir
