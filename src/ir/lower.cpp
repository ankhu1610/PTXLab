#include "ptxlab/ir/lower.h"

namespace ptxlab::ir {

std::unique_ptr<Module> Lowering::lower(const Program& program) {
    module_ = std::make_unique<Module>();
    for (const auto& kernel : program.kernels()) {
        lowerKernel(*kernel);
    }
    return std::move(module_);
}

IRType Lowering::toIRType(const Type& type) const {
    if (type.isPtr()) {
        return type.primitive == PrimitiveType::I32 ? IRType::ptrI32() : IRType::ptrF32();
    }
    if (type.isInt()) return IRType::i32();
    if (type.isFloat()) return IRType::f32();
    if (type.isBool()) return IRType::pred();
    return IRType::voidType();
}

void Lowering::enterScope() {
    scopes_.emplace_back();
}

void Lowering::exitScope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

void Lowering::setVar(const std::string& name, IROperand operand) {
    if (scopes_.empty()) {
        enterScope();
    }
    scopes_.back()[name] = operand;
}

void Lowering::assignVar(const std::string& name, IROperand operand) {
    IROperand varReg = lookupVar(name);
    builder_.createCopy(varReg, operand);
}

IROperand Lowering::lookupVar(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto varIt = it->find(name);
        if (varIt != it->end()) {
            return varIt->second;
        }
    }
    return IROperand::reg(-1, IRType::voidType());
}

void Lowering::lowerKernel(const KernelDecl& kernel) {
    currentFunc_ = module_->createFunction(kernel.name());
    builder_.setFunction(currentFunc_);
    scopes_.clear();
    enterScope();

    for (size_t i = 0; i < kernel.params().size(); ++i) {
        const auto& param = kernel.params()[i];
        IRType irT = toIRType(param.type());
        IROperand reg = builder_.createParam(static_cast<int>(i), irT, param.location());
        currentFunc_->addParam(param.name(), irT, reg.regId);
        setVar(param.name(), reg);
    }

    if (kernel.body()) {
        lowerStmt(*kernel.body());
    }

    // Ensure entry or current block has a terminator
    if (auto* cur = builder_.getInsertPoint()) {
        if (!cur->hasTerminator()) {
            builder_.createRet(kernel.location());
        }
    }

    exitScope();
    currentFunc_->rebuildCFG();
}

void Lowering::lowerStmt(const Stmt& stmt) {
    auto* curBB = builder_.getInsertPoint();
    if (curBB && curBB->hasTerminator()) {
        return; // Dead code after terminator
    }

    if (const auto* comp = dynamic_cast<const CompoundStmt*>(&stmt)) {
        enterScope();
        for (const auto& s : comp->statements()) {
            if (s) {
                lowerStmt(*s);
            }
        }
        exitScope();
        return;
    }

    if (const auto* decl = dynamic_cast<const DeclStmt*>(&stmt)) {
        IRType varType = toIRType(decl->type());
        int regId = varType.isPred() ? currentFunc_->nextPredId() : currentFunc_->nextRegId();
        IROperand varReg = IROperand::reg(regId, varType);
        if (decl->init()) {
            IROperand val = lowerExpr(*decl->init());
            setVar(decl->name(), varReg);
            builder_.createCopy(varReg, val, decl->location());
        } else {
            setVar(decl->name(), varReg);
        }
        return;
    }

    if (const auto* assign = dynamic_cast<const AssignStmt*>(&stmt)) {
        IROperand val = lowerExpr(*assign->value());

        if (const auto* ident = dynamic_cast<const IdentifierExpr*>(assign->lvalue())) {
            assignVar(ident->name(), val);
        } else if (const auto* idx = dynamic_cast<const IndexExpr*>(assign->lvalue())) {
            IROperand base = lowerExpr(*idx->base());
            IROperand index = lowerExpr(*idx->index());
            IROperand addr = builder_.createElemAddr(base, index, assign->location());
            builder_.createStore(addr, val, assign->location());
        }
        return;
    }

    if (const auto* ifStmt = dynamic_cast<const IfStmt*>(&stmt)) {
        BasicBlock* thenBB = currentFunc_->createBlock("if_then");
        BasicBlock* elseBB = ifStmt->elseBranch() ? currentFunc_->createBlock("if_else") : nullptr;
        BasicBlock* joinBB = currentFunc_->createBlock("if_join");

        BasicBlock* falseTarget = elseBB ? elseBB : joinBB;

        lowerCond(*ifStmt->condition(), thenBB, falseTarget);

        // Then branch
        builder_.setInsertPoint(thenBB);
        lowerStmt(*ifStmt->thenBranch());
        if (auto* cur = builder_.getInsertPoint(); cur && !cur->hasTerminator()) {
            builder_.createBr(joinBB);
        }

        // Else branch
        if (elseBB) {
            builder_.setInsertPoint(elseBB);
            lowerStmt(*ifStmt->elseBranch());
            if (auto* cur = builder_.getInsertPoint(); cur && !cur->hasTerminator()) {
                builder_.createBr(joinBB);
            }
        }

        builder_.setInsertPoint(joinBB);
        return;
    }

    if (const auto* whileStmt = dynamic_cast<const WhileStmt*>(&stmt)) {
        BasicBlock* headBB = currentFunc_->createBlock("while_head");
        BasicBlock* bodyBB = currentFunc_->createBlock("while_body");
        BasicBlock* exitBB = currentFunc_->createBlock("while_exit");

        builder_.createBr(headBB);

        builder_.setInsertPoint(headBB);
        lowerCond(*whileStmt->condition(), bodyBB, exitBB);

        builder_.setInsertPoint(bodyBB);
        lowerStmt(*whileStmt->body());
        if (auto* cur = builder_.getInsertPoint(); cur && !cur->hasTerminator()) {
            builder_.createBr(headBB);
        }

        builder_.setInsertPoint(exitBB);
        return;
    }

    if (const auto* forStmt = dynamic_cast<const ForStmt*>(&stmt)) {
        enterScope();

        if (forStmt->init()) {
            lowerStmt(*forStmt->init());
        }

        BasicBlock* headBB = currentFunc_->createBlock("for_head");
        BasicBlock* bodyBB = currentFunc_->createBlock("for_body");
        BasicBlock* stepBB = currentFunc_->createBlock("for_step");
        BasicBlock* exitBB = currentFunc_->createBlock("for_exit");

        builder_.createBr(headBB);

        builder_.setInsertPoint(headBB);
        if (forStmt->condition()) {
            lowerCond(*forStmt->condition(), bodyBB, exitBB);
        } else {
            builder_.createBr(bodyBB);
        }

        builder_.setInsertPoint(bodyBB);
        lowerStmt(*forStmt->body());
        if (auto* cur = builder_.getInsertPoint(); cur && !cur->hasTerminator()) {
            builder_.createBr(stepBB);
        }

        builder_.setInsertPoint(stepBB);
        if (forStmt->step()) {
            lowerStmt(*forStmt->step());
        }
        if (auto* cur = builder_.getInsertPoint(); cur && !cur->hasTerminator()) {
            builder_.createBr(headBB);
        }

        builder_.setInsertPoint(exitBB);
        exitScope();
        return;
    }

    if (const auto* ret = dynamic_cast<const ReturnStmt*>(&stmt)) {
        builder_.createRet(ret->location());
        return;
    }
}

void Lowering::lowerCond(const Expr& cond, BasicBlock* thenTarget, BasicBlock* elseTarget) {
    if (const auto* bin = dynamic_cast<const BinaryExpr*>(&cond)) {
        if (bin->op() == TokenKind::AmpAmp) {
            BasicBlock* rhsBB = currentFunc_->createBlock("and_rhs");
            lowerCond(*bin->lhs(), rhsBB, elseTarget);
            builder_.setInsertPoint(rhsBB);
            lowerCond(*bin->rhs(), thenTarget, elseTarget);
            return;
        }
        if (bin->op() == TokenKind::PipePipe) {
            BasicBlock* rhsBB = currentFunc_->createBlock("or_rhs");
            lowerCond(*bin->lhs(), thenTarget, rhsBB);
            builder_.setInsertPoint(rhsBB);
            lowerCond(*bin->rhs(), thenTarget, elseTarget);
            return;
        }

        IROperand lhs = lowerExpr(*bin->lhs());
        IROperand rhs = lowerExpr(*bin->rhs());
        Opcode cmpOp = Opcode::CmpEq;

        switch (bin->op()) {
            case TokenKind::EqualEqual:   cmpOp = Opcode::CmpEq; break;
            case TokenKind::ExclaimEqual: cmpOp = Opcode::CmpNe; break;
            case TokenKind::Less:         cmpOp = Opcode::CmpLt; break;
            case TokenKind::LessEqual:    cmpOp = Opcode::CmpLe; break;
            case TokenKind::Greater:      cmpOp = Opcode::CmpGt; break;
            case TokenKind::GreaterEqual: cmpOp = Opcode::CmpGe; break;
            default: break;
        }

        IROperand pred = builder_.createCmp(cmpOp, lhs, rhs, bin->location());
        builder_.createCondBr(pred, thenTarget, elseTarget, bin->location());
        return;
    }

    if (const auto* un = dynamic_cast<const UnaryExpr*>(&cond)) {
        if (un->op() == TokenKind::Exclaim) {
            // Invert branches for logical NOT
            lowerCond(*un->operand(), elseTarget, thenTarget);
            return;
        }
    }

    IROperand pred = lowerExpr(cond);
    builder_.createCondBr(pred, thenTarget, elseTarget, cond.location());
}

IROperand Lowering::lowerExpr(const Expr& expr) {
    if (const auto* lit = dynamic_cast<const IntLiteralExpr*>(&expr)) {
        return builder_.createConstInt(lit->value(), lit->location());
    }
    if (const auto* lit = dynamic_cast<const FloatLiteralExpr*>(&expr)) {
        return builder_.createConstFloat(lit->value(), lit->location());
    }
    if (const auto* lit = dynamic_cast<const BoolLiteralExpr*>(&expr)) {
        return builder_.createConstBool(lit->value(), lit->location());
    }
    if (const auto* id = dynamic_cast<const IdentifierExpr*>(&expr)) {
        return lookupVar(id->name());
    }

    if (const auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        IROperand op = lowerExpr(*un->operand());
        switch (un->op()) {
            case TokenKind::Minus:
                return op.type.isFloat() ? builder_.createFNeg(op, un->location())
                                         : builder_.createNeg(op, un->location());
            case TokenKind::Exclaim:
            case TokenKind::Tilde:
                return builder_.createNot(op, un->location());
            default:
                break;
        }
    }

    if (const auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        // Handle short-circuiting in value context
        if (bin->op() == TokenKind::AmpAmp || bin->op() == TokenKind::PipePipe) {
            BasicBlock* trueBB = currentFunc_->createBlock("sc_true");
            BasicBlock* falseBB = currentFunc_->createBlock("sc_false");
            BasicBlock* joinBB = currentFunc_->createBlock("sc_join");

            lowerCond(expr, trueBB, falseBB);

            IROperand res = IROperand::reg(currentFunc_->nextPredId(), IRType::pred());

            builder_.setInsertPoint(trueBB);
            IROperand trueVal = builder_.createConstBool(true, bin->location());
            builder_.createCopy(res, trueVal, bin->location());
            builder_.createBr(joinBB);

            builder_.setInsertPoint(falseBB);
            IROperand falseVal = builder_.createConstBool(false, bin->location());
            builder_.createCopy(res, falseVal, bin->location());
            builder_.createBr(joinBB);

            builder_.setInsertPoint(joinBB);
            return res;
        }

        IROperand lhs = lowerExpr(*bin->lhs());
        IROperand rhs = lowerExpr(*bin->rhs());

        switch (bin->op()) {
            case TokenKind::Plus:
                return lhs.type.isFloat() ? builder_.createFAdd(lhs, rhs, bin->location())
                                          : builder_.createAdd(lhs, rhs, bin->location());
            case TokenKind::Minus:
                return lhs.type.isFloat() ? builder_.createFSub(lhs, rhs, bin->location())
                                          : builder_.createSub(lhs, rhs, bin->location());
            case TokenKind::Star:
                return lhs.type.isFloat() ? builder_.createFMul(lhs, rhs, bin->location())
                                          : builder_.createMul(lhs, rhs, bin->location());
            case TokenKind::Slash:
                return lhs.type.isFloat() ? builder_.createFDiv(lhs, rhs, bin->location())
                                          : builder_.createDiv(lhs, rhs, bin->location());
            case TokenKind::Percent:
                return builder_.createRem(lhs, rhs, bin->location());

            case TokenKind::Amp:
                return builder_.createAnd(lhs, rhs, bin->location());
            case TokenKind::Pipe:
                return builder_.createOr(lhs, rhs, bin->location());
            case TokenKind::Caret:
                return builder_.createXor(lhs, rhs, bin->location());
            case TokenKind::LessLess:
                return builder_.createShl(lhs, rhs, bin->location());
            case TokenKind::GreaterGreater:
                return builder_.createShr(lhs, rhs, bin->location());

            case TokenKind::EqualEqual:
                return builder_.createCmp(Opcode::CmpEq, lhs, rhs, bin->location());
            case TokenKind::ExclaimEqual:
                return builder_.createCmp(Opcode::CmpNe, lhs, rhs, bin->location());
            case TokenKind::Less:
                return builder_.createCmp(Opcode::CmpLt, lhs, rhs, bin->location());
            case TokenKind::LessEqual:
                return builder_.createCmp(Opcode::CmpLe, lhs, rhs, bin->location());
            case TokenKind::Greater:
                return builder_.createCmp(Opcode::CmpGt, lhs, rhs, bin->location());
            case TokenKind::GreaterEqual:
                return builder_.createCmp(Opcode::CmpGe, lhs, rhs, bin->location());

            default:
                break;
        }
    }

    if (const auto* idx = dynamic_cast<const IndexExpr*>(&expr)) {
        IROperand base = lowerExpr(*idx->base());
        IROperand index = lowerExpr(*idx->index());
        IROperand addr = builder_.createElemAddr(base, index, idx->location());
        IRType elemType = idx->type().isInt() ? IRType::i32() : IRType::f32();
        return builder_.createLoad(addr, elemType, idx->location());
    }

    if (const auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        if (call->callee() == "tid_x") {
            return builder_.createSpecial(SpecialKind::TidX, call->location());
        }
        if (call->callee() == "ctaid_x") {
            return builder_.createSpecial(SpecialKind::CtaidX, call->location());
        }
        if (call->callee() == "ntid_x") {
            return builder_.createSpecial(SpecialKind::NtidX, call->location());
        }
        if (call->callee() == "i32") {
            IROperand arg = lowerExpr(*call->args()[0]);
            return builder_.createCvtF32ToI32(arg, call->location());
        }
        if (call->callee() == "f32") {
            IROperand arg = lowerExpr(*call->args()[0]);
            return builder_.createCvtI32ToF32(arg, call->location());
        }
    }

    return IROperand::reg(-1, IRType::voidType());
}

} // namespace ptxlab::ir
