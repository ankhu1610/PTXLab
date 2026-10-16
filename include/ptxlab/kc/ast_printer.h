#pragma once

#include <ostream>
#include <string>
#include "ptxlab/kc/ast.h"

namespace ptxlab {

class ASTPrinter : public ASTVisitor {
public:
    explicit ASTPrinter(std::ostream& os) : os_(os) {}

    void print(ASTNode& node);
    [[nodiscard]] static std::string dump(ASTNode& node);

    void visit(IntLiteralExpr& node) override;
    void visit(FloatLiteralExpr& node) override;
    void visit(BoolLiteralExpr& node) override;
    void visit(IdentifierExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(BinaryExpr& node) override;
    void visit(IndexExpr& node) override;
    void visit(CallExpr& node) override;

    void visit(CompoundStmt& node) override;
    void visit(DeclStmt& node) override;
    void visit(AssignStmt& node) override;
    void visit(IfStmt& node) override;
    void visit(WhileStmt& node) override;
    void visit(ForStmt& node) override;
    void visit(ReturnStmt& node) override;

    void visit(ParamDecl& node) override;
    void visit(KernelDecl& node) override;
    void visit(Program& node) override;

private:
    void indent();

    std::ostream& os_;
    int indentLevel_ = 0;
};

} // namespace ptxlab
