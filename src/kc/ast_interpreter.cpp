#include "ptxlab/kc/ast_interpreter.h"
#include <climits>

namespace ptxlab {

ASTInterpreter::ASTInterpreter(BufferManager& buffers, uint64_t maxSteps)
    : buffers_(buffers), maxSteps_(maxSteps) {
    enterScope();
}

void ASTInterpreter::enterScope() {
    scopes_.emplace_back();
}

void ASTInterpreter::exitScope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

void ASTInterpreter::setVar(const std::string& name, Value val) {
    if (scopes_.empty()) {
        enterScope();
    }
    scopes_.back()[name] = std::move(val);
}

void ASTInterpreter::assignVar(const std::string& name, Value val, SourceLocation loc) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto varIt = it->find(name);
        if (varIt != it->end()) {
            varIt->second = std::move(val);
            return;
        }
    }
    throw UBTrap("assignment to undeclared variable '" + name + "'", loc);
}

Value ASTInterpreter::getVar(const std::string& name, SourceLocation loc) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto varIt = it->find(name);
        if (varIt != it->end()) {
            return varIt->second;
        }
    }
    throw UBTrap("read of undeclared variable '" + name + "'", loc);
}

void ASTInterpreter::step(SourceLocation loc) {
    if (++stepCount_ > maxSteps_) {
        throw UBTrap("step limit exceeded (infinite loop)", loc);
    }
}

Value ASTInterpreter::evaluate(Expr& expr) {
    expr.accept(*this);
    return lastExprVal_;
}

void ASTInterpreter::execute(Stmt& stmt) {
    if (hasReturned_) {
        return;
    }
    step(stmt.location());
    stmt.accept(*this);
}

void ASTInterpreter::launch1D(KernelDecl& kernel,
                             int gridDim,
                             int blockDim,
                             const std::unordered_map<std::string, KernelArg>& args) {
    ntid_x_ = blockDim;

    for (int block = 0; block < gridDim; ++block) {
        ctaid_x_ = block;
        for (int thread = 0; thread < blockDim; ++thread) {
            tid_x_ = thread;
            hasReturned_ = false;
            stepCount_ = 0;
            scopes_.clear();
            enterScope(); // Kernel scope

            // Bind kernel parameters
            for (const auto& param : kernel.params()) {
                auto argIt = args.find(param.name());
                if (argIt == args.end()) {
                    throw UBTrap("missing kernel argument for parameter '" + param.name() + "'", kernel.location());
                }
                const auto& arg = argIt->second;
                if (param.type().isPtr()) {
                    if (arg.kind != KernelArg::Kind::Buffer) {
                        throw UBTrap("expected buffer for pointer parameter '" + param.name() + "'", kernel.location());
                    }
                    setVar(param.name(), Value::makePtr(arg.bufferName));
                } else if (param.type().isInt()) {
                    if (arg.kind != KernelArg::Kind::I32) {
                        throw UBTrap("type mismatch for i32 parameter '" + param.name() + "'", kernel.location());
                    }
                    setVar(param.name(), Value::makeI32(arg.i32Val));
                } else if (param.type().isFloat()) {
                    if (arg.kind != KernelArg::Kind::F32) {
                        throw UBTrap("type mismatch for f32 parameter '" + param.name() + "'", kernel.location());
                    }
                    setVar(param.name(), Value::makeF32(arg.f32Val));
                } else if (param.type().isBool()) {
                    if (arg.kind != KernelArg::Kind::Bool) {
                        throw UBTrap("type mismatch for bool parameter '" + param.name() + "'", kernel.location());
                    }
                    setVar(param.name(), Value::makeBool(arg.boolVal));
                }
            }

            if (kernel.body()) {
                kernel.body()->accept(*this);
            }

            exitScope();
        }
    }
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

void ASTInterpreter::visit(IntLiteralExpr& node) {
    lastExprVal_ = Value::makeI32(node.value());
}

void ASTInterpreter::visit(FloatLiteralExpr& node) {
    lastExprVal_ = Value::makeF32(node.value());
}

void ASTInterpreter::visit(BoolLiteralExpr& node) {
    lastExprVal_ = Value::makeBool(node.value());
}

void ASTInterpreter::visit(IdentifierExpr& node) {
    lastExprVal_ = getVar(node.name(), node.location());
}

void ASTInterpreter::visit(UnaryExpr& node) {
    Value val = evaluate(*node.operand());
    switch (node.op()) {
        case TokenKind::Minus:
            if (val.kind == Value::Kind::I32) {
                // Wrap in two's complement
                uint32_t u = static_cast<uint32_t>(val.i32);
                lastExprVal_ = Value::makeI32(static_cast<int32_t>(0u - u));
            } else if (val.kind == Value::Kind::F32) {
                lastExprVal_ = Value::makeF32(-val.f32);
            }
            break;
        case TokenKind::Exclaim:
            lastExprVal_ = Value::makeBool(!val.b);
            break;
        case TokenKind::Tilde:
            lastExprVal_ = Value::makeI32(~val.i32);
            break;
        default:
            throw UBTrap("unsupported unary operator", node.location());
    }
}

void ASTInterpreter::visit(BinaryExpr& node) {
    // Short-circuiting for logical operators
    if (node.op() == TokenKind::AmpAmp) {
        Value lhs = evaluate(*node.lhs());
        if (!lhs.b) {
            lastExprVal_ = Value::makeBool(false);
            return;
        }
        Value rhs = evaluate(*node.rhs());
        lastExprVal_ = Value::makeBool(rhs.b);
        return;
    }
    if (node.op() == TokenKind::PipePipe) {
        Value lhs = evaluate(*node.lhs());
        if (lhs.b) {
            lastExprVal_ = Value::makeBool(true);
            return;
        }
        Value rhs = evaluate(*node.rhs());
        lastExprVal_ = Value::makeBool(rhs.b);
        return;
    }

    Value lhs = evaluate(*node.lhs());
    Value rhs = evaluate(*node.rhs());

    if (lhs.kind == Value::Kind::I32 && rhs.kind == Value::Kind::I32) {
        uint32_t uA = static_cast<uint32_t>(lhs.i32);
        uint32_t uB = static_cast<uint32_t>(rhs.i32);

        switch (node.op()) {
            case TokenKind::Plus:
                lastExprVal_ = Value::makeI32(static_cast<int32_t>(uA + uB));
                return;
            case TokenKind::Minus:
                lastExprVal_ = Value::makeI32(static_cast<int32_t>(uA - uB));
                return;
            case TokenKind::Star:
                lastExprVal_ = Value::makeI32(static_cast<int32_t>(uA * uB));
                return;
            case TokenKind::Slash:
                if (rhs.i32 == 0) {
                    throw UBTrap("division by zero", node.location());
                }
                if (lhs.i32 == INT32_MIN && rhs.i32 == -1) {
                    throw UBTrap("signed integer division overflow", node.location());
                }
                lastExprVal_ = Value::makeI32(lhs.i32 / rhs.i32);
                return;
            case TokenKind::Percent:
                if (rhs.i32 == 0) {
                    throw UBTrap("modulo by zero", node.location());
                }
                if (lhs.i32 == INT32_MIN && rhs.i32 == -1) {
                    throw UBTrap("signed integer division overflow", node.location());
                }
                lastExprVal_ = Value::makeI32(lhs.i32 % rhs.i32);
                return;
            case TokenKind::Amp:
                lastExprVal_ = Value::makeI32(lhs.i32 & rhs.i32);
                return;
            case TokenKind::Pipe:
                lastExprVal_ = Value::makeI32(lhs.i32 | rhs.i32);
                return;
            case TokenKind::Caret:
                lastExprVal_ = Value::makeI32(lhs.i32 ^ rhs.i32);
                return;
            case TokenKind::LessLess:
                if (rhs.i32 < 0 || rhs.i32 >= 32) {
                    throw UBTrap("shift count out of range [0, 31]: " + std::to_string(rhs.i32), node.location());
                }
                lastExprVal_ = Value::makeI32(static_cast<int32_t>(uA << rhs.i32));
                return;
            case TokenKind::GreaterGreater:
                if (rhs.i32 < 0 || rhs.i32 >= 32) {
                    throw UBTrap("shift count out of range [0, 31]: " + std::to_string(rhs.i32), node.location());
                }
                lastExprVal_ = Value::makeI32(lhs.i32 >> rhs.i32);
                return;
            case TokenKind::EqualEqual:
                lastExprVal_ = Value::makeBool(lhs.i32 == rhs.i32);
                return;
            case TokenKind::ExclaimEqual:
                lastExprVal_ = Value::makeBool(lhs.i32 != rhs.i32);
                return;
            case TokenKind::Less:
                lastExprVal_ = Value::makeBool(lhs.i32 < rhs.i32);
                return;
            case TokenKind::LessEqual:
                lastExprVal_ = Value::makeBool(lhs.i32 <= rhs.i32);
                return;
            case TokenKind::Greater:
                lastExprVal_ = Value::makeBool(lhs.i32 > rhs.i32);
                return;
            case TokenKind::GreaterEqual:
                lastExprVal_ = Value::makeBool(lhs.i32 >= rhs.i32);
                return;
            default:
                break;
        }
    } else if (lhs.kind == Value::Kind::F32 && rhs.kind == Value::Kind::F32) {
        switch (node.op()) {
            case TokenKind::Plus:
                lastExprVal_ = Value::makeF32(lhs.f32 + rhs.f32);
                return;
            case TokenKind::Minus:
                lastExprVal_ = Value::makeF32(lhs.f32 - rhs.f32);
                return;
            case TokenKind::Star:
                lastExprVal_ = Value::makeF32(lhs.f32 * rhs.f32);
                return;
            case TokenKind::Slash:
                lastExprVal_ = Value::makeF32(lhs.f32 / rhs.f32);
                return;
            case TokenKind::EqualEqual:
                lastExprVal_ = Value::makeBool(lhs.f32 == rhs.f32);
                return;
            case TokenKind::ExclaimEqual:
                lastExprVal_ = Value::makeBool(lhs.f32 != rhs.f32);
                return;
            case TokenKind::Less:
                lastExprVal_ = Value::makeBool(lhs.f32 < rhs.f32);
                return;
            case TokenKind::LessEqual:
                lastExprVal_ = Value::makeBool(lhs.f32 <= rhs.f32);
                return;
            case TokenKind::Greater:
                lastExprVal_ = Value::makeBool(lhs.f32 > rhs.f32);
                return;
            case TokenKind::GreaterEqual:
                lastExprVal_ = Value::makeBool(lhs.f32 >= rhs.f32);
                return;
            default:
                break;
        }
    } else if (lhs.kind == Value::Kind::Bool && rhs.kind == Value::Kind::Bool) {
        switch (node.op()) {
            case TokenKind::EqualEqual:
                lastExprVal_ = Value::makeBool(lhs.b == rhs.b);
                return;
            case TokenKind::ExclaimEqual:
                lastExprVal_ = Value::makeBool(lhs.b != rhs.b);
                return;
            default:
                break;
        }
    }

    throw UBTrap("unsupported binary operation", node.location());
}

void ASTInterpreter::visit(IndexExpr& node) {
    Value baseVal = evaluate(*node.base());
    Value idxVal = evaluate(*node.index());

    if (baseVal.kind != Value::Kind::Ptr) {
        throw UBTrap("subscript on non-pointer value", node.location());
    }

    if (node.type().isInt()) {
        lastExprVal_ = Value::makeI32(buffers_.readI32(baseVal.ptrBuffer, idxVal.i32, node.location()));
    } else if (node.type().isFloat()) {
        lastExprVal_ = Value::makeF32(buffers_.readF32(baseVal.ptrBuffer, idxVal.i32, node.location()));
    } else {
        throw UBTrap("unsupported pointer element type", node.location());
    }
}

void ASTInterpreter::visit(CallExpr& node) {
    if (node.callee() == "tid_x") {
        lastExprVal_ = Value::makeI32(tid_x_);
        return;
    }
    if (node.callee() == "ctaid_x") {
        lastExprVal_ = Value::makeI32(ctaid_x_);
        return;
    }
    if (node.callee() == "ntid_x") {
        lastExprVal_ = Value::makeI32(ntid_x_);
        return;
    }

    if (node.callee() == "i32") {
        Value arg = evaluate(*node.args()[0]);
        if (arg.kind == Value::Kind::F32) {
            if (std::isnan(arg.f32) || arg.f32 < -2147483648.0f || arg.f32 > 2147483647.0f) {
                throw UBTrap("float to int conversion out of range or NaN: " + std::to_string(arg.f32), node.location());
            }
            lastExprVal_ = Value::makeI32(static_cast<int32_t>(arg.f32));
        } else if (arg.kind == Value::Kind::Bool) {
            lastExprVal_ = Value::makeI32(arg.b ? 1 : 0);
        } else {
            lastExprVal_ = arg;
        }
        return;
    }

    if (node.callee() == "f32") {
        Value arg = evaluate(*node.args()[0]);
        if (arg.kind == Value::Kind::I32) {
            lastExprVal_ = Value::makeF32(static_cast<float>(arg.i32));
        } else {
            lastExprVal_ = arg;
        }
        return;
    }

    throw UBTrap("unknown function or intrinsic '" + node.callee() + "'", node.location());
}

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

void ASTInterpreter::visit(CompoundStmt& node) {
    enterScope();
    for (const auto& stmt : node.statements()) {
        if (hasReturned_) break;
        if (stmt) {
            execute(*stmt);
        }
    }
    exitScope();
}

void ASTInterpreter::visit(DeclStmt& node) {
    Value initVal = evaluate(*node.init());
    setVar(node.name(), initVal);
}

void ASTInterpreter::visit(AssignStmt& node) {
    Value val = evaluate(*node.value());

    if (auto* ident = dynamic_cast<IdentifierExpr*>(node.lvalue())) {
        assignVar(ident->name(), val, node.location());
    } else if (auto* idx = dynamic_cast<IndexExpr*>(node.lvalue())) {
        Value baseVal = evaluate(*idx->base());
        Value idxVal = evaluate(*idx->index());

        if (baseVal.kind != Value::Kind::Ptr) {
            throw UBTrap("subscript on non-pointer value", node.location());
        }

        if (val.kind == Value::Kind::I32) {
            buffers_.writeI32(baseVal.ptrBuffer, idxVal.i32, val.i32, node.location());
        } else if (val.kind == Value::Kind::F32) {
            buffers_.writeF32(baseVal.ptrBuffer, idxVal.i32, val.f32, node.location());
        } else {
            throw UBTrap("unsupported buffer write type", node.location());
        }
    } else {
        throw UBTrap("invalid lvalue in assignment", node.location());
    }
}

void ASTInterpreter::visit(IfStmt& node) {
    Value cond = evaluate(*node.condition());
    if (cond.b) {
        if (node.thenBranch()) {
            execute(*node.thenBranch());
        }
    } else {
        if (node.elseBranch()) {
            execute(*node.elseBranch());
        }
    }
}

void ASTInterpreter::visit(WhileStmt& node) {
    while (true) {
        if (hasReturned_) break;
        step(node.location());

        Value cond = evaluate(*node.condition());
        if (!cond.b) {
            break;
        }
        if (node.body()) {
            execute(*node.body());
        }
    }
}

void ASTInterpreter::visit(ForStmt& node) {
    enterScope();

    if (node.init()) {
        execute(*node.init());
    }

    while (true) {
        if (hasReturned_) break;
        step(node.location());

        if (node.condition()) {
            Value cond = evaluate(*node.condition());
            if (!cond.b) {
                break;
            }
        }

        if (node.body()) {
            execute(*node.body());
        }

        if (hasReturned_) break;

        if (node.step()) {
            execute(*node.step());
        }
    }

    exitScope();
}

void ASTInterpreter::visit(ReturnStmt& node) {
    hasReturned_ = true;
}

void ASTInterpreter::visit(ParamDecl& node) {}
void ASTInterpreter::visit(KernelDecl& node) {}
void ASTInterpreter::visit(Program& node) {}

} // namespace ptxlab
