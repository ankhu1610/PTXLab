#include "ptxlab/kc/ast_printer.h"
#include <sstream>

namespace ptxlab {

void ASTPrinter::indent() {
    for (int i = 0; i < indentLevel_; ++i) {
        os_ << "  ";
    }
}

void ASTPrinter::print(ASTNode& node) {
    node.accept(*this);
}

std::string ASTPrinter::dump(ASTNode& node) {
    std::ostringstream oss;
    ASTPrinter printer(oss);
    printer.print(node);
    return oss.str();
}

void ASTPrinter::visit(IntLiteralExpr& node) {
    indent();
    os_ << "IntLiteral " << node.value() << "\n";
}

void ASTPrinter::visit(FloatLiteralExpr& node) {
    indent();
    os_ << "FloatLiteral " << node.value() << "\n";
}

void ASTPrinter::visit(BoolLiteralExpr& node) {
    indent();
    os_ << "BoolLiteral " << (node.value() ? "true" : "false") << "\n";
}

void ASTPrinter::visit(IdentifierExpr& node) {
    indent();
    os_ << "Identifier '" << node.name() << "'\n";
}

void ASTPrinter::visit(UnaryExpr& node) {
    indent();
    os_ << "UnaryExpr '" << toString(node.op()) << "'\n";
    ++indentLevel_;
    if (node.operand()) {
        node.operand()->accept(*this);
    }
    --indentLevel_;
}

void ASTPrinter::visit(BinaryExpr& node) {
    indent();
    os_ << "BinaryExpr '" << toString(node.op()) << "'\n";
    ++indentLevel_;
    if (node.lhs()) {
        node.lhs()->accept(*this);
    }
    if (node.rhs()) {
        node.rhs()->accept(*this);
    }
    --indentLevel_;
}

void ASTPrinter::visit(IndexExpr& node) {
    indent();
    os_ << "IndexExpr\n";
    ++indentLevel_;
    if (node.base()) {
        node.base()->accept(*this);
    }
    if (node.index()) {
        node.index()->accept(*this);
    }
    --indentLevel_;
}

void ASTPrinter::visit(CallExpr& node) {
    indent();
    os_ << "CallExpr '" << node.callee() << "'\n";
    ++indentLevel_;
    for (const auto& arg : node.args()) {
        if (arg) {
            arg->accept(*this);
        }
    }
    --indentLevel_;
}

void ASTPrinter::visit(CompoundStmt& node) {
    indent();
    os_ << "CompoundStmt\n";
    ++indentLevel_;
    for (const auto& stmt : node.statements()) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
    --indentLevel_;
}

void ASTPrinter::visit(DeclStmt& node) {
    indent();
    os_ << "DeclStmt '" << node.name() << "' type=" << node.type().toString() << "\n";
    ++indentLevel_;
    if (node.init()) {
        node.init()->accept(*this);
    }
    --indentLevel_;
}

void ASTPrinter::visit(AssignStmt& node) {
    indent();
    os_ << "AssignStmt\n";
    ++indentLevel_;
    if (node.lvalue()) {
        node.lvalue()->accept(*this);
    }
    if (node.value()) {
        node.value()->accept(*this);
    }
    --indentLevel_;
}

void ASTPrinter::visit(IfStmt& node) {
    indent();
    os_ << "IfStmt\n";
    ++indentLevel_;
    indent();
    os_ << "Condition:\n";
    ++indentLevel_;
    if (node.condition()) {
        node.condition()->accept(*this);
    }
    --indentLevel_;

    indent();
    os_ << "Then:\n";
    ++indentLevel_;
    if (node.thenBranch()) {
        node.thenBranch()->accept(*this);
    }
    --indentLevel_;

    if (node.elseBranch()) {
        indent();
        os_ << "Else:\n";
        ++indentLevel_;
        node.elseBranch()->accept(*this);
        --indentLevel_;
    }
    --indentLevel_;
}

void ASTPrinter::visit(WhileStmt& node) {
    indent();
    os_ << "WhileStmt\n";
    ++indentLevel_;
    indent();
    os_ << "Condition:\n";
    ++indentLevel_;
    if (node.condition()) {
        node.condition()->accept(*this);
    }
    --indentLevel_;

    indent();
    os_ << "Body:\n";
    ++indentLevel_;
    if (node.body()) {
        node.body()->accept(*this);
    }
    --indentLevel_;
    --indentLevel_;
}

void ASTPrinter::visit(ForStmt& node) {
    indent();
    os_ << "ForStmt\n";
    ++indentLevel_;
    indent();
    os_ << "Init:\n";
    ++indentLevel_;
    if (node.init()) {
        node.init()->accept(*this);
    }
    --indentLevel_;

    indent();
    os_ << "Condition:\n";
    ++indentLevel_;
    if (node.condition()) {
        node.condition()->accept(*this);
    }
    --indentLevel_;

    indent();
    os_ << "Step:\n";
    ++indentLevel_;
    if (node.step()) {
        node.step()->accept(*this);
    }
    --indentLevel_;

    indent();
    os_ << "Body:\n";
    ++indentLevel_;
    if (node.body()) {
        node.body()->accept(*this);
    }
    --indentLevel_;
    --indentLevel_;
}

void ASTPrinter::visit(ReturnStmt& /*node*/) {
    indent();
    os_ << "ReturnStmt\n";
}

void ASTPrinter::visit(ParamDecl& node) {
    indent();
    os_ << "ParamDecl '" << node.name() << "' type=" << node.type().toString() << "\n";
}

void ASTPrinter::visit(KernelDecl& node) {
    indent();
    os_ << "KernelDecl '" << node.name() << "'\n";
    ++indentLevel_;
    for (const auto& param : node.params()) {
        const_cast<ParamDecl&>(param).accept(*this);
    }
    if (node.body()) {
        node.body()->accept(*this);
    }
    --indentLevel_;
}

void ASTPrinter::visit(Program& node) {
    indent();
    os_ << "Program\n";
    ++indentLevel_;
    for (const auto& kernel : node.kernels()) {
        if (kernel) {
            kernel->accept(*this);
        }
    }
    --indentLevel_;
}

} // namespace ptxlab
