#include "ptxlab/ir/ir_builder.h"

namespace ptxlab::ir {

void IRBuilder::emit(std::unique_ptr<Instruction> inst) {
    if (insertBlock_) {
        insertBlock_->appendInstruction(std::move(inst));
    }
}

IROperand IRBuilder::nextReg(IRType type) {
    if (!function_) {
        return IROperand::reg(0, type);
    }
    if (type.isPred()) {
        return IROperand::reg(function_->nextPredId(), type);
    }
    return IROperand::reg(function_->nextRegId(), type);
}

IROperand IRBuilder::createConstInt(int32_t val, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::Const, loc);
    inst->setDst(dst);
    inst->addOperand(IROperand::constInt(val));
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createConstFloat(float val, SourceLocation loc) {
    auto dst = nextReg(IRType::f32());
    auto inst = std::make_unique<Instruction>(Opcode::Const, loc);
    inst->setDst(dst);
    inst->addOperand(IROperand::constFloat(val));
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createConstBool(bool val, SourceLocation loc) {
    auto dst = nextReg(IRType::pred());
    auto inst = std::make_unique<Instruction>(Opcode::Const, loc);
    inst->setDst(dst);
    inst->addOperand(IROperand::constBool(val));
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createCopy(IROperand src, SourceLocation loc) {
    auto dst = nextReg(src.type);
    auto inst = std::make_unique<Instruction>(Opcode::Copy, loc);
    inst->setDst(dst);
    inst->addOperand(src);
    emit(std::move(inst));
    return dst;
}

void IRBuilder::createCopy(IROperand dst, IROperand src, SourceLocation loc) {
    auto inst = std::make_unique<Instruction>(Opcode::Copy, loc);
    inst->setDst(dst);
    inst->addOperand(src);
    emit(std::move(inst));
}

IROperand IRBuilder::createParam(int index, IRType type, SourceLocation loc) {
    auto dst = nextReg(type);
    auto inst = std::make_unique<Instruction>(Opcode::Param, loc);
    inst->setDst(dst);
    inst->addOperand(IROperand::constInt(index));
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createSpecial(SpecialKind kind, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::Special, loc);
    inst->setDst(dst);
    inst->setSpecialKind(kind);
    emit(std::move(inst));
    return dst;
}

// Integer Binary
IROperand IRBuilder::createAdd(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::Add, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createSub(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::Sub, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createMul(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::Mul, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createDiv(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::Div, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createRem(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::Rem, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createAnd(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::And, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createOr(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::Or, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createXor(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::Xor, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createShl(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::Shl, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createShr(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::Shr, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

// Float Binary
IROperand IRBuilder::createFAdd(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::f32());
    auto inst = std::make_unique<Instruction>(Opcode::FAdd, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createFSub(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::f32());
    auto inst = std::make_unique<Instruction>(Opcode::FSub, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createFMul(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::f32());
    auto inst = std::make_unique<Instruction>(Opcode::FMul, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createFDiv(IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::f32());
    auto inst = std::make_unique<Instruction>(Opcode::FDiv, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

// Unary
IROperand IRBuilder::createNeg(IROperand a, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::Neg, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createFNeg(IROperand a, SourceLocation loc) {
    auto dst = nextReg(IRType::f32());
    auto inst = std::make_unique<Instruction>(Opcode::FNeg, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createNot(IROperand a, SourceLocation loc) {
    auto dst = nextReg(a.type);
    auto inst = std::make_unique<Instruction>(Opcode::Not, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createCmp(Opcode cmpOp, IROperand a, IROperand b, SourceLocation loc) {
    auto dst = nextReg(IRType::pred());
    auto inst = std::make_unique<Instruction>(cmpOp, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    inst->addOperand(b);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createCvtI32ToF32(IROperand a, SourceLocation loc) {
    auto dst = nextReg(IRType::f32());
    auto inst = std::make_unique<Instruction>(Opcode::CvtI32ToF32, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createCvtF32ToI32(IROperand a, SourceLocation loc) {
    auto dst = nextReg(IRType::i32());
    auto inst = std::make_unique<Instruction>(Opcode::CvtF32ToI32, loc);
    inst->setDst(dst);
    inst->addOperand(a);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createElemAddr(IROperand base, IROperand idx, SourceLocation loc) {
    auto dst = nextReg(base.type); // Pointer type is preserved
    auto inst = std::make_unique<Instruction>(Opcode::ElemAddr, loc);
    inst->setDst(dst);
    inst->addOperand(base);
    inst->addOperand(idx);
    emit(std::move(inst));
    return dst;
}

IROperand IRBuilder::createLoad(IROperand addr, IRType elemType, SourceLocation loc) {
    auto dst = nextReg(elemType);
    auto inst = std::make_unique<Instruction>(Opcode::Load, loc);
    inst->setDst(dst);
    inst->addOperand(addr);
    emit(std::move(inst));
    return dst;
}

void IRBuilder::createStore(IROperand addr, IROperand val, SourceLocation loc) {
    auto inst = std::make_unique<Instruction>(Opcode::Store, loc);
    inst->addOperand(addr);
    inst->addOperand(val);
    emit(std::move(inst));
}

void IRBuilder::createBr(BasicBlock* target, SourceLocation loc) {
    if (!target) return;
    auto inst = std::make_unique<Instruction>(Opcode::Br, loc);
    inst->setBranchTargets(target->label());
    emit(std::move(inst));
    if (insertBlock_) {
        insertBlock_->addSuccessor(target);
    }
}

void IRBuilder::createCondBr(IROperand pred, BasicBlock* thenTarget, BasicBlock* elseTarget, SourceLocation loc) {
    if (!thenTarget || !elseTarget) return;
    auto inst = std::make_unique<Instruction>(Opcode::CondBr, loc);
    inst->addOperand(pred);
    inst->setBranchTargets(thenTarget->label(), elseTarget->label());
    emit(std::move(inst));
    if (insertBlock_) {
        insertBlock_->addSuccessor(thenTarget);
        insertBlock_->addSuccessor(elseTarget);
    }
}

void IRBuilder::createRet(SourceLocation loc) {
    auto inst = std::make_unique<Instruction>(Opcode::Ret, loc);
    emit(std::move(inst));
}

} // namespace ptxlab::ir
