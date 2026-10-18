#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include "ptxlab/kc/ast.h"
#include "ptxlab/ir/ir_builder.h"
#include "ptxlab/ir/module.h"

namespace ptxlab::ir {

class Lowering {
public:
    Lowering() = default;

    /// Lowers an entire KC Program AST into an IR Module.
    [[nodiscard]] std::unique_ptr<Module> lower(const Program& program);

private:
    void lowerKernel(const KernelDecl& kernel);
    void lowerStmt(const Stmt& stmt);
    IROperand lowerExpr(const Expr& expr);
    void lowerCond(const Expr& cond, BasicBlock* thenTarget, BasicBlock* elseTarget);

    void enterScope();
    void exitScope();
    void setVar(const std::string& name, IROperand operand);
    void assignVar(const std::string& name, IROperand operand);
    [[nodiscard]] IROperand lookupVar(const std::string& name) const;

    IRType toIRType(const Type& type) const;

    IRBuilder builder_;
    std::unique_ptr<Module> module_;
    Function* currentFunc_ = nullptr;
    std::vector<std::unordered_map<std::string, IROperand>> scopes_;
};

} // namespace ptxlab::ir
