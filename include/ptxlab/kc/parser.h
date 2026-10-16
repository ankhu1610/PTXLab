#pragma once

#include <memory>
#include <string_view>
#include <vector>
#include "ptxlab/kc/ast.h"
#include "ptxlab/kc/lexer.h"
#include "ptxlab/kc/token.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

namespace ptxlab {

class Parser {
public:
    Parser(Lexer& lexer, const SourceManager& sourceMgr, DiagnosticEngine& diagEngine);

    [[nodiscard]] std::unique_ptr<Program> parseProgram();

private:
    // Token inspection & advance
    Token advance();
    [[nodiscard]] bool check(TokenKind kind) const noexcept;
    bool match(TokenKind kind) noexcept;
    template <typename... Kinds>
    bool matchOneOf(TokenKind k1, Kinds... rest) noexcept {
        if (check(k1)) {
            advance();
            return true;
        }
        if constexpr (sizeof...(rest) > 0) {
            return matchOneOf(rest...);
        }
        return false;
    }
    Token consume(TokenKind kind, std::string_view errorMessage);
    [[nodiscard]] bool isAtEnd() const noexcept;

    // Error recovery
    void synchronize();

    // Declarations & Kernels
    std::unique_ptr<KernelDecl> parseKernel();
    ParamDecl parseParam();
    Type parseType();

    // Statements
    std::unique_ptr<CompoundStmt> parseBlock();
    std::unique_ptr<Stmt> parseStmt();
    std::unique_ptr<DeclStmt> parseDeclStmt();
    std::unique_ptr<AssignStmt> parseAssignStmt();
    std::unique_ptr<AssignStmt> parseAssignNoTerm();
    std::unique_ptr<IfStmt> parseIf();
    std::unique_ptr<WhileStmt> parseWhile();
    std::unique_ptr<ForStmt> parseFor();
    std::unique_ptr<ReturnStmt> parseReturn();

    // Expressions (Precedence climbing)
    std::unique_ptr<Expr> parseExpr();
    std::unique_ptr<Expr> parseLogicalOr();
    std::unique_ptr<Expr> parseLogicalAnd();
    std::unique_ptr<Expr> parseBitwiseOr();
    std::unique_ptr<Expr> parseBitwiseXor();
    std::unique_ptr<Expr> parseBitwiseAnd();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseRelational();
    std::unique_ptr<Expr> parseShift();
    std::unique_ptr<Expr> parseAdditive();
    std::unique_ptr<Expr> parseMultiplicative();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePostfix();
    std::unique_ptr<Expr> parsePrimary();

    Lexer& lexer_;
    const SourceManager& sm_;
    DiagnosticEngine& diag_;
    Token current_;
    Token previous_;
};

} // namespace ptxlab
