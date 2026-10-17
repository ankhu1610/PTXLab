#include "ptxlab/kc/parser.h"

namespace ptxlab {

Parser::Parser(Lexer& lexer, const SourceManager& sourceMgr, DiagnosticEngine& diagEngine)
    : lexer_(lexer), sm_(sourceMgr), diag_(diagEngine) {
    advance(); // Initialize current_
}

Token Parser::advance() {
    previous_ = current_;
    current_ = lexer_.nextToken();
    return previous_;
}

bool Parser::check(TokenKind kind) const noexcept {
    return current_.is(kind);
}

bool Parser::match(TokenKind kind) noexcept {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenKind kind, std::string_view errorMessage) {
    if (check(kind)) {
        return advance();
    }
    diag_.error(current_.location, std::string(errorMessage));
    return current_;
}

bool Parser::isAtEnd() const noexcept {
    return current_.is(TokenKind::Eof);
}

void Parser::synchronize() {
    while (!isAtEnd()) {
        if (previous_.is(TokenKind::Semicolon)) {
            return;
        }

        switch (current_.kind) {
            case TokenKind::KwKernel:
            case TokenKind::KwIf:
            case TokenKind::KwWhile:
            case TokenKind::KwFor:
            case TokenKind::KwReturn:
            case TokenKind::KwI32:
            case TokenKind::KwF32:
            case TokenKind::KwBool:
            case TokenKind::RBrace:
                return;
            default:
                advance();
                break;
        }
    }
}

std::unique_ptr<Program> Parser::parseProgram() {
    SourceLocation loc = current_.location;
    std::vector<std::unique_ptr<KernelDecl>> kernels;

    while (!isAtEnd()) {
        if (check(TokenKind::KwKernel)) {
            auto kernel = parseKernel();
            if (kernel) {
                kernels.push_back(std::move(kernel));
            }
        } else {
            diag_.error(current_.location, "expected 'kernel' declaration");
            advance();
            synchronize();
        }
    }

    return std::make_unique<Program>(loc, std::move(kernels));
}

std::unique_ptr<KernelDecl> Parser::parseKernel() {
    SourceLocation loc = current_.location;
    consume(TokenKind::KwKernel, "expected 'kernel'");
    Token nameTok = consume(TokenKind::Identifier, "expected kernel name");

    consume(TokenKind::LParen, "expected '(' after kernel name");
    std::vector<ParamDecl> params;
    if (!check(TokenKind::RParen) && !isAtEnd()) {
        while (true) {
            params.push_back(parseParam());
            if (!match(TokenKind::Comma)) {
                break;
            }
        }
    }
    consume(TokenKind::RParen, "expected ')' after parameter list");

    auto body = parseBlock();
    return std::make_unique<KernelDecl>(loc, std::string(nameTok.text),
                                        std::move(params), std::move(body));
}

ParamDecl Parser::parseParam() {
    SourceLocation loc = current_.location;
    Type type = parseType();
    Token nameTok = consume(TokenKind::Identifier, "expected parameter name");
    return ParamDecl(loc, type, std::string(nameTok.text));
}

Type Parser::parseType() {
    PrimitiveType prim = PrimitiveType::Void;
    if (match(TokenKind::KwI32)) {
        prim = PrimitiveType::I32;
    } else if (match(TokenKind::KwF32)) {
        prim = PrimitiveType::F32;
    } else if (match(TokenKind::KwBool)) {
        prim = PrimitiveType::Bool;
    } else {
        diag_.error(current_.location, "expected type ('i32', 'f32', or 'bool')");
    }

    bool isPtr = match(TokenKind::Star);
    return Type{prim, isPtr};
}

std::unique_ptr<CompoundStmt> Parser::parseBlock() {
    SourceLocation loc = current_.location;
    consume(TokenKind::LBrace, "expected '{'");

    std::vector<std::unique_ptr<Stmt>> stmts;
    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        auto s = parseStmt();
        if (s) {
            stmts.push_back(std::move(s));
        }
    }

    consume(TokenKind::RBrace, "expected '}' at end of block");
    return std::make_unique<CompoundStmt>(loc, std::move(stmts));
}

std::unique_ptr<Stmt> Parser::parseStmt() {
    if (check(TokenKind::LBrace)) {
        return parseBlock();
    }
    if (check(TokenKind::KwIf)) {
        return parseIf();
    }
    if (check(TokenKind::KwWhile)) {
        return parseWhile();
    }
    if (check(TokenKind::KwFor)) {
        return parseFor();
    }
    if (check(TokenKind::KwReturn)) {
        return parseReturn();
    }
    if (check(TokenKind::KwI32) || check(TokenKind::KwF32) || check(TokenKind::KwBool)) {
        return parseDeclStmt();
    }
    return parseAssignStmt();
}

std::unique_ptr<DeclStmt> Parser::parseDeclStmt() {
    SourceLocation loc = current_.location;
    Type type = parseType();
    Token nameTok = consume(TokenKind::Identifier, "expected variable name");
    consume(TokenKind::Equal, "expected '=': variable declaration requires an initializer");
    auto init = parseExpr();
    consume(TokenKind::Semicolon, "expected ';' after variable declaration");
    return std::make_unique<DeclStmt>(loc, type, std::string(nameTok.text), std::move(init));
}

std::unique_ptr<AssignStmt> Parser::parseAssignStmt() {
    auto assign = parseAssignNoTerm();
    consume(TokenKind::Semicolon, "expected ';' after assignment");
    return assign;
}

std::unique_ptr<AssignStmt> Parser::parseAssignNoTerm() {
    SourceLocation loc = current_.location;
    Token nameTok = consume(TokenKind::Identifier, "expected identifier in assignment");
    std::unique_ptr<Expr> lvalue = std::make_unique<IdentifierExpr>(nameTok.location, std::string(nameTok.text));

    if (match(TokenKind::LBracket)) {
        auto index = parseExpr();
        consume(TokenKind::RBracket, "expected ']' after array index");
        lvalue = std::make_unique<IndexExpr>(loc, std::move(lvalue), std::move(index));
    }

    consume(TokenKind::Equal, "expected '=' in assignment");
    auto val = parseExpr();
    return std::make_unique<AssignStmt>(loc, std::move(lvalue), std::move(val));
}

std::unique_ptr<IfStmt> Parser::parseIf() {
    SourceLocation loc = current_.location;
    consume(TokenKind::KwIf, "expected 'if'");
    consume(TokenKind::LParen, "expected '(' after 'if'");
    auto cond = parseExpr();
    consume(TokenKind::RParen, "expected ')' after if condition");

    auto thenBranch = parseStmt();
    std::unique_ptr<Stmt> elseBranch;
    if (match(TokenKind::KwElse)) {
        elseBranch = parseStmt();
    }
    return std::make_unique<IfStmt>(loc, std::move(cond), std::move(thenBranch), std::move(elseBranch));
}

std::unique_ptr<WhileStmt> Parser::parseWhile() {
    SourceLocation loc = current_.location;
    consume(TokenKind::KwWhile, "expected 'while'");
    consume(TokenKind::LParen, "expected '(' after 'while'");
    auto cond = parseExpr();
    consume(TokenKind::RParen, "expected ')' after while condition");
    auto body = parseStmt();
    return std::make_unique<WhileStmt>(loc, std::move(cond), std::move(body));
}

std::unique_ptr<ForStmt> Parser::parseFor() {
    SourceLocation loc = current_.location;
    consume(TokenKind::KwFor, "expected 'for'");
    consume(TokenKind::LParen, "expected '(' after 'for'");

    std::unique_ptr<Stmt> init;
    if (check(TokenKind::KwI32) || check(TokenKind::KwF32) || check(TokenKind::KwBool)) {
        init = parseDeclStmt();
    } else {
        init = parseAssignStmt();
    }

    auto cond = parseExpr();
    consume(TokenKind::Semicolon, "expected ';' after for loop condition");

    auto step = parseAssignNoTerm();
    consume(TokenKind::RParen, "expected ')' after for clauses");

    auto body = parseStmt();
    return std::make_unique<ForStmt>(loc, std::move(init), std::move(cond), std::move(step), std::move(body));
}

std::unique_ptr<ReturnStmt> Parser::parseReturn() {
    SourceLocation loc = current_.location;
    consume(TokenKind::KwReturn, "expected 'return'");
    consume(TokenKind::Semicolon, "expected ';' after 'return'");
    return std::make_unique<ReturnStmt>(loc);
}

// ---------------------------------------------------------------------------
// Expression Precedence Hierarchy (Precedence Climbing)
// ---------------------------------------------------------------------------
std::unique_ptr<Expr> Parser::parseExpr() {
    return parseLogicalOr();
}

std::unique_ptr<Expr> Parser::parseLogicalOr() {
    auto lhs = parseLogicalAnd();
    while (match(TokenKind::PipePipe)) {
        Token op = previous_;
        auto rhs = parseLogicalAnd();
        lhs = std::make_unique<BinaryExpr>(op.location, op.kind, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseLogicalAnd() {
    auto lhs = parseBitwiseOr();
    while (match(TokenKind::AmpAmp)) {
        Token op = previous_;
        auto rhs = parseBitwiseOr();
        lhs = std::make_unique<BinaryExpr>(op.location, op.kind, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseBitwiseOr() {
    auto lhs = parseBitwiseXor();
    while (match(TokenKind::Pipe)) {
        Token op = previous_;
        auto rhs = parseBitwiseXor();
        lhs = std::make_unique<BinaryExpr>(op.location, op.kind, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseBitwiseXor() {
    auto lhs = parseBitwiseAnd();
    while (match(TokenKind::Caret)) {
        Token op = previous_;
        auto rhs = parseBitwiseAnd();
        lhs = std::make_unique<BinaryExpr>(op.location, op.kind, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseBitwiseAnd() {
    auto lhs = parseEquality();
    while (match(TokenKind::Amp)) {
        Token op = previous_;
        auto rhs = parseEquality();
        lhs = std::make_unique<BinaryExpr>(op.location, op.kind, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseEquality() {
    auto lhs = parseRelational();
    while (matchOneOf(TokenKind::EqualEqual, TokenKind::ExclaimEqual)) {
        Token op = previous_;
        auto rhs = parseRelational();
        lhs = std::make_unique<BinaryExpr>(op.location, op.kind, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseRelational() {
    auto lhs = parseShift();
    while (matchOneOf(TokenKind::Less, TokenKind::LessEqual, TokenKind::Greater, TokenKind::GreaterEqual)) {
        Token op = previous_;
        auto rhs = parseShift();
        lhs = std::make_unique<BinaryExpr>(op.location, op.kind, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseShift() {
    auto lhs = parseAdditive();
    while (matchOneOf(TokenKind::LessLess, TokenKind::GreaterGreater)) {
        Token op = previous_;
        auto rhs = parseAdditive();
        lhs = std::make_unique<BinaryExpr>(op.location, op.kind, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseAdditive() {
    auto lhs = parseMultiplicative();
    while (matchOneOf(TokenKind::Plus, TokenKind::Minus)) {
        Token op = previous_;
        auto rhs = parseMultiplicative();
        lhs = std::make_unique<BinaryExpr>(op.location, op.kind, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseMultiplicative() {
    auto lhs = parseUnary();
    while (matchOneOf(TokenKind::Star, TokenKind::Slash, TokenKind::Percent)) {
        Token op = previous_;
        auto rhs = parseUnary();
        lhs = std::make_unique<BinaryExpr>(op.location, op.kind, std::move(lhs), std::move(rhs));
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    if (matchOneOf(TokenKind::Minus, TokenKind::Exclaim, TokenKind::Tilde)) {
        Token op = previous_;
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(op.location, op.kind, std::move(operand));
    }
    return parsePostfix();
}

std::unique_ptr<Expr> Parser::parsePostfix() {
    auto expr = parsePrimary();
    while (match(TokenKind::LBracket)) {
        SourceLocation loc = previous_.location;
        auto index = parseExpr();
        consume(TokenKind::RBracket, "expected ']' after index expression");
        expr = std::make_unique<IndexExpr>(loc, std::move(expr), std::move(index));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    if (match(TokenKind::IntLiteral)) {
        return std::make_unique<IntLiteralExpr>(previous_.location, previous_.intVal);
    }
    if (match(TokenKind::FloatLiteral)) {
        return std::make_unique<FloatLiteralExpr>(previous_.location, previous_.floatVal);
    }
    if (match(TokenKind::KwTrue)) {
        return std::make_unique<BoolLiteralExpr>(previous_.location, true);
    }
    if (match(TokenKind::KwFalse)) {
        return std::make_unique<BoolLiteralExpr>(previous_.location, false);
    }
    if (matchOneOf(TokenKind::Identifier, TokenKind::KwI32, TokenKind::KwF32, TokenKind::KwBool)) {
        Token idTok = previous_;
        if (match(TokenKind::LParen)) {
            // Function call / builtin / type cast
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenKind::RParen) && !isAtEnd()) {
                while (true) {
                    args.push_back(parseExpr());
                    if (!match(TokenKind::Comma)) {
                        break;
                    }
                }
            }
            consume(TokenKind::RParen, "expected ')' after argument list");
            return std::make_unique<CallExpr>(idTok.location, std::string(idTok.text), std::move(args));
        }
        if (idTok.is(TokenKind::Identifier)) {
            return std::make_unique<IdentifierExpr>(idTok.location, std::string(idTok.text));
        }
        diag_.error(idTok.location, "expected '(' after type conversion keyword");
        return nullptr;
    }
    if (match(TokenKind::LParen)) {
        auto expr = parseExpr();
        consume(TokenKind::RParen, "expected ')' after parenthesized expression");
        return expr;
    }

    diag_.error(current_.location, "expected expression");
    advance();
    return nullptr;
}

} // namespace ptxlab
