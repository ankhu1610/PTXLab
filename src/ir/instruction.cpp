#include "ptxlab/ir/instruction.h"

namespace ptxlab::ir {

std::string Instruction::toString() const {
    std::string s;

    // Destination
    if (dst_) {
        s += dst_->toString() + " = ";
    }

    switch (opcode_) {
        case Opcode::Const:
            s += "const " + operands_[0].type.toString() + " " + operands_[0].toString();
            break;
        case Opcode::Copy:
            s += "copy " + operands_[0].toString();
            break;
        case Opcode::Param:
            s += "param " + operands_[0].toString();
            break;
        case Opcode::Special:
            s += "special ";
            switch (specialKind_) {
                case SpecialKind::TidX:   s += "tid_x"; break;
                case SpecialKind::CtaidX: s += "ctaid_x"; break;
                case SpecialKind::NtidX:  s += "ntid_x"; break;
            }
            break;

        case Opcode::Add:  s += "add " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::Sub:  s += "sub " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::Mul:  s += "mul " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::Div:  s += "div " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::Rem:  s += "rem " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::And:  s += "and " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::Or:   s += "or " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::Xor:  s += "xor " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::Shl:  s += "shl " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::Shr:  s += "shr " + operands_[0].toString() + ", " + operands_[1].toString(); break;

        case Opcode::FAdd: s += "fadd " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::FSub: s += "fsub " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::FMul: s += "fmul " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::FDiv: s += "fdiv " + operands_[0].toString() + ", " + operands_[1].toString(); break;

        case Opcode::Neg:  s += "neg " + operands_[0].toString(); break;
        case Opcode::FNeg: s += "fneg " + operands_[0].toString(); break;
        case Opcode::Not:  s += "not " + operands_[0].toString(); break;

        case Opcode::CmpEq: s += "cmp.eq " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::CmpNe: s += "cmp.ne " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::CmpLt: s += "cmp.lt " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::CmpLe: s += "cmp.le " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::CmpGt: s += "cmp.gt " + operands_[0].toString() + ", " + operands_[1].toString(); break;
        case Opcode::CmpGe: s += "cmp.ge " + operands_[0].toString() + ", " + operands_[1].toString(); break;

        case Opcode::CvtI32ToF32: s += "cvt.i32_to_f32 " + operands_[0].toString(); break;
        case Opcode::CvtF32ToI32: s += "cvt.f32_to_i32 " + operands_[0].toString(); break;

        case Opcode::ElemAddr:
            s += "elemaddr " + operands_[0].toString() + ", " + operands_[1].toString();
            break;
        case Opcode::Load:
            s += "load " + operands_[0].toString();
            break;
        case Opcode::Store:
            s += "store " + operands_[0].toString() + ", " + operands_[1].toString();
            break;

        case Opcode::Br:
            s += "br " + targetTrue_;
            break;
        case Opcode::CondBr:
            s += "condbr " + operands_[0].toString() + ", " + targetTrue_ + ", " + targetFalse_;
            break;
        case Opcode::Ret:
            s += "ret";
            break;
    }

    return s;
}

} // namespace ptxlab::ir
