#include "ptxlab/kc/sema.h"
#include <unordered_set>

namespace ptxlab {

Sema::Sema(const SourceManager& sourceMgr, DiagnosticEngine& diagEngine)
    : sm_(sourceMgr), diag_(diagEngine) {}

bool Sema::analyze(Program& program) {
    program.accept(*this);
    return !diag_.hasErrors();
}

void Sema::visit(Program& node) {
    declaredKernels_.clear();
    for (const auto& kernel : node.kernels()) {
        if (auto it = declaredKernels_.find(kernel->name()); it != declaredKernels_.end()) {
            diag_.error(kernel->location(), "redefinition of kernel '" + kernel->name() + "'");
        } else {
            declaredKernels_[kernel->name()] = kernel->location();
        }
        kernel->accept(*this);
    }
}

void Sema::visit(KernelDecl& node) {
    symbolTable_.enterScope();
    std::unordered_set<std::string> paramNames;

    for (const auto& param : node.params()) {
        if (param.type().isVoid()) {
            diag_.error(param.location(), "parameter cannot have void type");
            continue;
        }
        if (param.type().isPtr() && param.type().primitive == PrimitiveType::Bool) {
            diag_.error(param.location(), "pointer to bool is not supported");
            continue;
        }

        if (!paramNames.insert(param.name()).second) {
            diag_.error(param.location(), "redefinition of parameter '" + param.name() + "'");
        } else {
            symbolTable_.insert(Symbol{param.name(), param.type(), param.location(), true});
        }
    }

    if (node.body()) {
        node.body()->accept(*this);
    }

    symbolTable_.exitScope();
}

void Sema::visit(ParamDecl& node) {
    // Handled in KernelDecl
}

void Sema::visit(CompoundStmt& node) {
    symbolTable_.enterScope();
    for (const auto& stmt : node.statements()) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
    symbolTable_.exitScope();
}

void Sema::visit(DeclStmt& node) {
    if (node.type().isPtr()) {
        diag_.error(node.location(), "local variable cannot have pointer type '" + node.type().toString() + "'");
    }
    if (node.type().isVoid()) {
        diag_.error(node.location(), "variable cannot have void type");
    }

    if (node.init()) {
        node.init()->accept(*this);
        if (node.init()->type() != node.type() && !node.init()->type().isVoid()) {
            diag_.error(node.location(), "cannot initialize variable of type '" + node.type().toString() +
                                            "' with rvalue of type '" + node.init()->type().toString() + "'");
        }
    }

    if (symbolTable_.lookupCurrent(node.name())) {
        diag_.error(node.location(), "redefinition of variable '" + node.name() + "'");
    } else {
        symbolTable_.insert(Symbol{node.name(), node.type(), node.location(), false});
    }
}

void Sema::visit(AssignStmt& node) {
    auto* ident = dynamic_cast<IdentifierExpr*>(node.lvalue());
    auto* indexExpr = dynamic_cast<IndexExpr*>(node.lvalue());

    if (ident) {
        ident->accept(*this);
        if (ident->type().isPtr()) {
            diag_.error(node.lvalue()->location(), "cannot assign directly to pointer variable '" + ident->name() + "'");
        }
    } else if (indexExpr) {
        indexExpr->accept(*this);
    } else {
        diag_.error(node.lvalue()->location(), "expression is not assignable");
        if (node.lvalue()) {
            node.lvalue()->accept(*this);
        }
    }

    if (node.value()) {
        node.value()->accept(*this);
        if (node.lvalue() && node.lvalue()->type() != node.value()->type() &&
            !node.lvalue()->type().isVoid() && !node.value()->type().isVoid()) {
            diag_.error(node.location(), "assigning to '" + node.lvalue()->type().toString() +
                                            "' from incompatible type '" + node.value()->type().toString() + "'");
        }
    }
}

void Sema::visit(IfStmt& node) {
    if (node.condition()) {
        node.condition()->accept(*this);
        if (!node.condition()->type().isBool() && !node.condition()->type().isVoid()) {
            diag_.error(node.condition()->location(), "statement condition must have type 'bool' (got '" +
                                                         node.condition()->type().toString() + "')");
        }
    }

    if (node.thenBranch()) {
        node.thenBranch()->accept(*this);
    }
    if (node.elseBranch()) {
        node.elseBranch()->accept(*this);
    }
}

void Sema::visit(WhileStmt& node) {
    if (node.condition()) {
        node.condition()->accept(*this);
        if (!node.condition()->type().isBool() && !node.condition()->type().isVoid()) {
            diag_.error(node.condition()->location(), "statement condition must have type 'bool' (got '" +
                                                         node.condition()->type().toString() + "')");
        }
    }

    if (node.body()) {
        node.body()->accept(*this);
    }
}

void Sema::visit(ForStmt& node) {
    symbolTable_.enterScope();

    if (node.init()) {
        node.init()->accept(*this);
    }
    if (node.condition()) {
        node.condition()->accept(*this);
        if (!node.condition()->type().isBool() && !node.condition()->type().isVoid()) {
            diag_.error(node.condition()->location(), "statement condition must have type 'bool' (got '" +
                                                         node.condition()->type().toString() + "')");
        }
    }
    if (node.step()) {
        node.step()->accept(*this);
    }
    if (node.body()) {
        node.body()->accept(*this);
    }

    symbolTable_.exitScope();
}

void Sema::visit(ReturnStmt& node) {
    // Return statement in KC is strictly "return;" (void)
}

void Sema::visit(IntLiteralExpr& node) {
    node.setType(Type::getI32());
}

void Sema::visit(FloatLiteralExpr& node) {
    node.setType(Type::getF32());
}

void Sema::visit(BoolLiteralExpr& node) {
    node.setType(Type::getBool());
}

void Sema::visit(IdentifierExpr& node) {
    const auto* sym = symbolTable_.lookup(node.name());
    if (!sym) {
        diag_.error(node.location(), "use of undeclared identifier '" + node.name() + "'");
        node.setType(Type::getVoid());
    } else {
        node.setType(sym->type);
    }
}

void Sema::visit(UnaryExpr& node) {
    if (!node.operand()) {
        return;
    }

    node.operand()->accept(*this);
    Type opType = node.operand()->type();
    if (opType.isVoid()) {
        node.setType(Type::getVoid());
        return;
    }

    switch (node.op()) {
        case TokenKind::Minus:
            if (opType.isInt()) {
                node.setType(Type::getI32());
            } else if (opType.isFloat()) {
                node.setType(Type::getF32());
            } else {
                diag_.error(node.location(), "invalid argument type '" + opType.toString() + "' to unary minus");
                node.setType(Type::getVoid());
            }
            break;
        case TokenKind::Exclaim:
            if (opType.isBool()) {
                node.setType(Type::getBool());
            } else {
                diag_.error(node.location(), "invalid argument type '" + opType.toString() + "' to logical not");
                node.setType(Type::getVoid());
            }
            break;
        case TokenKind::Tilde:
            if (opType.isInt()) {
                node.setType(Type::getI32());
            } else {
                diag_.error(node.location(), "invalid argument type '" + opType.toString() + "' to bitwise not");
                node.setType(Type::getVoid());
            }
            break;
        default:
            diag_.error(node.location(), "unsupported unary operator");
            node.setType(Type::getVoid());
            break;
    }
}

void Sema::visit(BinaryExpr& node) {
    if (!node.lhs() || !node.rhs()) {
        return;
    }

    node.lhs()->accept(*this);
    node.rhs()->accept(*this);

    Type lhsType = node.lhs()->type();
    Type rhsType = node.rhs()->type();

    if (lhsType.isVoid() || rhsType.isVoid()) {
        node.setType(Type::getVoid());
        return;
    }

    switch (node.op()) {
        case TokenKind::Plus:
        case TokenKind::Minus:
        case TokenKind::Star:
        case TokenKind::Slash:
            if (lhsType == rhsType && (lhsType.isInt() || lhsType.isFloat())) {
                node.setType(lhsType);
            } else {
                diag_.error(node.location(), "invalid operands to binary expression ('" + lhsType.toString() +
                                                "' and '" + rhsType.toString() + "')");
                node.setType(Type::getVoid());
            }
            break;

        case TokenKind::Percent:
            if (lhsType.isInt() && rhsType.isInt()) {
                node.setType(Type::getI32());
            } else {
                diag_.error(node.location(), "invalid operands to modulo expression ('" + lhsType.toString() +
                                                "' and '" + rhsType.toString() + "')");
                node.setType(Type::getVoid());
            }
            break;

        case TokenKind::Amp:
        case TokenKind::Pipe:
        case TokenKind::Caret:
        case TokenKind::LessLess:
        case TokenKind::GreaterGreater:
            if (lhsType.isInt() && rhsType.isInt()) {
                node.setType(Type::getI32());
            } else {
                diag_.error(node.location(), "bitwise operator requires integer operands ('" +
                                                (lhsType.isInt() ? rhsType.toString() : lhsType.toString()) + "' given)");
                node.setType(Type::getVoid());
            }
            break;

        case TokenKind::EqualEqual:
        case TokenKind::ExclaimEqual:
            if (lhsType == rhsType && !lhsType.isPtr()) {
                node.setType(Type::getBool());
            } else {
                diag_.error(node.location(), "comparison between incompatible types ('" + lhsType.toString() +
                                                "' and '" + rhsType.toString() + "')");
                node.setType(Type::getVoid());
            }
            break;

        case TokenKind::Less:
        case TokenKind::LessEqual:
        case TokenKind::Greater:
        case TokenKind::GreaterEqual:
            if (lhsType == rhsType && (lhsType.isInt() || lhsType.isFloat())) {
                node.setType(Type::getBool());
            } else {
                diag_.error(node.location(), "comparison between incompatible types ('" + lhsType.toString() +
                                                "' and '" + rhsType.toString() + "')");
                node.setType(Type::getVoid());
            }
            break;

        case TokenKind::AmpAmp:
        case TokenKind::PipePipe:
            if (lhsType.isBool() && rhsType.isBool()) {
                node.setType(Type::getBool());
            } else {
                diag_.error(node.location(), "logical operator requires bool operands");
                node.setType(Type::getVoid());
            }
            break;

        default:
            diag_.error(node.location(), "unsupported binary operator");
            node.setType(Type::getVoid());
            break;
    }
}

void Sema::visit(IndexExpr& node) {
    if (!node.base() || !node.index()) {
        return;
    }

    node.base()->accept(*this);
    node.index()->accept(*this);

    if (!node.base()->type().isPtr()) {
        diag_.error(node.base()->location(), "subscripted value is not a pointer (type is '" +
                                                node.base()->type().toString() + "')");
        node.setType(Type::getVoid());
        return;
    }

    if (!node.index()->type().isInt()) {
        diag_.error(node.index()->location(), "array subscript is not an integer (type is '" +
                                                 node.index()->type().toString() + "')");
    }

    if (node.base()->type().primitive == PrimitiveType::I32) {
        node.setType(Type::getI32());
    } else if (node.base()->type().primitive == PrimitiveType::F32) {
        node.setType(Type::getF32());
    } else {
        node.setType(Type::getVoid());
    }
}

void Sema::visit(CallExpr& node) {
    // GPU built-ins
    if (node.callee() == "tid_x" || node.callee() == "ctaid_x" || node.callee() == "ntid_x") {
        if (!node.args().empty()) {
            diag_.error(node.location(), "builtin '" + node.callee() + "' expects 0 arguments");
        }
        node.setType(Type::getI32());
        return;
    }

    // Explicit conversions
    if (node.callee() == "i32") {
        if (node.args().size() != 1) {
            diag_.error(node.location(), "explicit conversion 'i32' expects 1 argument");
            node.setType(Type::getI32());
            return;
        }
        node.args()[0]->accept(*this);
        Type argType = node.args()[0]->type();
        if (!argType.isFloat() && !argType.isBool() && !argType.isVoid()) {
            diag_.error(node.location(), "cannot cast '" + argType.toString() + "' to 'i32'");
        }
        node.setType(Type::getI32());
        return;
    }

    if (node.callee() == "f32") {
        if (node.args().size() != 1) {
            diag_.error(node.location(), "explicit conversion 'f32' expects 1 argument");
            node.setType(Type::getF32());
            return;
        }
        node.args()[0]->accept(*this);
        Type argType = node.args()[0]->type();
        if (!argType.isInt() && !argType.isVoid()) {
            diag_.error(node.location(), "cannot cast '" + argType.toString() + "' to 'f32'");
        }
        node.setType(Type::getF32());
        return;
    }

    diag_.error(node.location(), "unknown function or builtin '" + node.callee() + "'");
    node.setType(Type::getVoid());
}

} // namespace ptxlab
