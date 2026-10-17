#pragma once

#include "ptxlab/kc/ast.h"
#include "ptxlab/kc/symbol_table.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

namespace ptxlab {

class Sema : public ASTVisitor {
public:
    Sema(const SourceManager& sourceMgr, DiagnosticEngine& diagEngine);

    /// Runs semantic analysis on the entire AST program.
    /// Returns true if no semantic errors were found.
    bool analyze(Program& program);

    // Visitor methods
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
    const SourceManager& sm_;
    DiagnosticEngine& diag_;
    SymbolTable symbolTable_;
    std::unordered_map<std::string, SourceLocation> declaredKernels_;
};

} // namespace ptxlab
