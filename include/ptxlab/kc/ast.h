#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "ptxlab/kc/token_kind.h"
#include "ptxlab/kc/type.h"
#include "ptxlab/support/source_location.h"

namespace ptxlab {

// Forward declarations
class ASTVisitor;

class Expr;
class IntLiteralExpr;
class FloatLiteralExpr;
class BoolLiteralExpr;
class IdentifierExpr;
class UnaryExpr;
class BinaryExpr;
class IndexExpr;
class CallExpr;

class Stmt;
class CompoundStmt;
class DeclStmt;
class AssignStmt;
class IfStmt;
class WhileStmt;
class ForStmt;
class ReturnStmt;

class ParamDecl;
class KernelDecl;
class Program;

// ---------------------------------------------------------------------------
// Visitor Interface
// ---------------------------------------------------------------------------
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    virtual void visit(IntLiteralExpr& node) = 0;
    virtual void visit(FloatLiteralExpr& node) = 0;
    virtual void visit(BoolLiteralExpr& node) = 0;
    virtual void visit(IdentifierExpr& node) = 0;
    virtual void visit(UnaryExpr& node) = 0;
    virtual void visit(BinaryExpr& node) = 0;
    virtual void visit(IndexExpr& node) = 0;
    virtual void visit(CallExpr& node) = 0;

    virtual void visit(CompoundStmt& node) = 0;
    virtual void visit(DeclStmt& node) = 0;
    virtual void visit(AssignStmt& node) = 0;
    virtual void visit(IfStmt& node) = 0;
    virtual void visit(WhileStmt& node) = 0;
    virtual void visit(ForStmt& node) = 0;
    virtual void visit(ReturnStmt& node) = 0;

    virtual void visit(ParamDecl& node) = 0;
    virtual void visit(KernelDecl& node) = 0;
    virtual void visit(Program& node) = 0;
};

// ---------------------------------------------------------------------------
// Base AST Node
// ---------------------------------------------------------------------------
class ASTNode {
public:
    explicit ASTNode(SourceLocation loc) : loc_(loc) {}
    virtual ~ASTNode() = default;

    [[nodiscard]] SourceLocation location() const noexcept { return loc_; }
    virtual void accept(ASTVisitor& visitor) = 0;

private:
    SourceLocation loc_;
};

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------
class Expr : public ASTNode {
public:
    using ASTNode::ASTNode;

    [[nodiscard]] const Type& type() const noexcept { return type_; }
    void setType(const Type& t) noexcept { type_ = t; }

private:
    Type type_{PrimitiveType::Void, false};
};

class IntLiteralExpr : public Expr {
public:
    IntLiteralExpr(SourceLocation loc, int32_t value)
        : Expr(loc), value_(value) {
        setType(Type::getI32());
    }

    [[nodiscard]] int32_t value() const noexcept { return value_; }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    int32_t value_;
};

class FloatLiteralExpr : public Expr {
public:
    FloatLiteralExpr(SourceLocation loc, float value)
        : Expr(loc), value_(value) {
        setType(Type::getF32());
    }

    [[nodiscard]] float value() const noexcept { return value_; }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    float value_;
};

class BoolLiteralExpr : public Expr {
public:
    BoolLiteralExpr(SourceLocation loc, bool value)
        : Expr(loc), value_(value) {
        setType(Type::getBool());
    }

    [[nodiscard]] bool value() const noexcept { return value_; }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    bool value_;
};

class IdentifierExpr : public Expr {
public:
    IdentifierExpr(SourceLocation loc, std::string name)
        : Expr(loc), name_(std::move(name)) {}

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string name_;
};

class UnaryExpr : public Expr {
public:
    UnaryExpr(SourceLocation loc, TokenKind op, std::unique_ptr<Expr> operand)
        : Expr(loc), op_(op), operand_(std::move(operand)) {}

    [[nodiscard]] TokenKind op() const noexcept { return op_; }
    [[nodiscard]] Expr* operand() const noexcept { return operand_.get(); }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    TokenKind op_;
    std::unique_ptr<Expr> operand_;
};

class BinaryExpr : public Expr {
public:
    BinaryExpr(SourceLocation loc, TokenKind op, std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs)
        : Expr(loc), op_(op), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

    [[nodiscard]] TokenKind op() const noexcept { return op_; }
    [[nodiscard]] Expr* lhs() const noexcept { return lhs_.get(); }
    [[nodiscard]] Expr* rhs() const noexcept { return rhs_.get(); }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    TokenKind op_;
    std::unique_ptr<Expr> lhs_;
    std::unique_ptr<Expr> rhs_;
};

class IndexExpr : public Expr {
public:
    IndexExpr(SourceLocation loc, std::unique_ptr<Expr> base, std::unique_ptr<Expr> index)
        : Expr(loc), base_(std::move(base)), index_(std::move(index)) {}

    [[nodiscard]] Expr* base() const noexcept { return base_.get(); }
    [[nodiscard]] Expr* index() const noexcept { return index_.get(); }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<Expr> base_;
    std::unique_ptr<Expr> index_;
};

class CallExpr : public Expr {
public:
    CallExpr(SourceLocation loc, std::string callee, std::vector<std::unique_ptr<Expr>> args)
        : Expr(loc), callee_(std::move(callee)), args_(std::move(args)) {}

    [[nodiscard]] const std::string& callee() const noexcept { return callee_; }
    [[nodiscard]] const std::vector<std::unique_ptr<Expr>>& args() const noexcept { return args_; }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string callee_;
    std::vector<std::unique_ptr<Expr>> args_;
};

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------
class Stmt : public ASTNode {
public:
    using ASTNode::ASTNode;
};

class CompoundStmt : public Stmt {
public:
    CompoundStmt(SourceLocation loc, std::vector<std::unique_ptr<Stmt>> statements)
        : Stmt(loc), statements_(std::move(statements)) {}

    [[nodiscard]] const std::vector<std::unique_ptr<Stmt>>& statements() const noexcept {
        return statements_;
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::vector<std::unique_ptr<Stmt>> statements_;
};

class DeclStmt : public Stmt {
public:
    DeclStmt(SourceLocation loc, Type type, std::string name, std::unique_ptr<Expr> init)
        : Stmt(loc), type_(type), name_(std::move(name)), init_(std::move(init)) {}

    [[nodiscard]] Type type() const noexcept { return type_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] Expr* init() const noexcept { return init_.get(); }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    Type type_;
    std::string name_;
    std::unique_ptr<Expr> init_;
};

class AssignStmt : public Stmt {
public:
    AssignStmt(SourceLocation loc, std::unique_ptr<Expr> lvalue, std::unique_ptr<Expr> value)
        : Stmt(loc), lvalue_(std::move(lvalue)), value_(std::move(value)) {}

    [[nodiscard]] Expr* lvalue() const noexcept { return lvalue_.get(); }
    [[nodiscard]] Expr* value() const noexcept { return value_.get(); }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<Expr> lvalue_;
    std::unique_ptr<Expr> value_;
};

class IfStmt : public Stmt {
public:
    IfStmt(SourceLocation loc, std::unique_ptr<Expr> condition,
           std::unique_ptr<Stmt> thenBranch, std::unique_ptr<Stmt> elseBranch)
        : Stmt(loc), condition_(std::move(condition)),
          thenBranch_(std::move(thenBranch)), elseBranch_(std::move(elseBranch)) {}

    [[nodiscard]] Expr* condition() const noexcept { return condition_.get(); }
    [[nodiscard]] Stmt* thenBranch() const noexcept { return thenBranch_.get(); }
    [[nodiscard]] Stmt* elseBranch() const noexcept { return elseBranch_.get(); }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<Expr> condition_;
    std::unique_ptr<Stmt> thenBranch_;
    std::unique_ptr<Stmt> elseBranch_;
};

class WhileStmt : public Stmt {
public:
    WhileStmt(SourceLocation loc, std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> body)
        : Stmt(loc), condition_(std::move(condition)), body_(std::move(body)) {}

    [[nodiscard]] Expr* condition() const noexcept { return condition_.get(); }
    [[nodiscard]] Stmt* body() const noexcept { return body_.get(); }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<Expr> condition_;
    std::unique_ptr<Stmt> body_;
};

class ForStmt : public Stmt {
public:
    ForStmt(SourceLocation loc, std::unique_ptr<Stmt> init, std::unique_ptr<Expr> condition,
            std::unique_ptr<Stmt> step, std::unique_ptr<Stmt> body)
        : Stmt(loc), init_(std::move(init)), condition_(std::move(condition)),
          step_(std::move(step)), body_(std::move(body)) {}

    [[nodiscard]] Stmt* init() const noexcept { return init_.get(); }
    [[nodiscard]] Expr* condition() const noexcept { return condition_.get(); }
    [[nodiscard]] Stmt* step() const noexcept { return step_.get(); }
    [[nodiscard]] Stmt* body() const noexcept { return body_.get(); }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::unique_ptr<Stmt> init_;
    std::unique_ptr<Expr> condition_;
    std::unique_ptr<Stmt> step_;
    std::unique_ptr<Stmt> body_;
};

class ReturnStmt : public Stmt {
public:
    explicit ReturnStmt(SourceLocation loc) : Stmt(loc) {}
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

// ---------------------------------------------------------------------------
// Parameters, Kernels, and Program
// ---------------------------------------------------------------------------
class ParamDecl : public ASTNode {
public:
    ParamDecl(SourceLocation loc, Type type, std::string name)
        : ASTNode(loc), type_(type), name_(std::move(name)) {}

    [[nodiscard]] Type type() const noexcept { return type_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    Type type_;
    std::string name_;
};

class KernelDecl : public ASTNode {
public:
    KernelDecl(SourceLocation loc, std::string name,
               std::vector<ParamDecl> params, std::unique_ptr<CompoundStmt> body)
        : ASTNode(loc), name_(std::move(name)),
          params_(std::move(params)), body_(std::move(body)) {}

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const std::vector<ParamDecl>& params() const noexcept { return params_; }
    [[nodiscard]] CompoundStmt* body() const noexcept { return body_.get(); }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string name_;
    std::vector<ParamDecl> params_;
    std::unique_ptr<CompoundStmt> body_;
};

class Program : public ASTNode {
public:
    Program(SourceLocation loc, std::vector<std::unique_ptr<KernelDecl>> kernels)
        : ASTNode(loc), kernels_(std::move(kernels)) {}

    [[nodiscard]] const std::vector<std::unique_ptr<KernelDecl>>& kernels() const noexcept {
        return kernels_;
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::vector<std::unique_ptr<KernelDecl>> kernels_;
};

} // namespace ptxlab
