#pragma once

#include <optional>
#include <string_view>

namespace ptxlab {

enum class TokenKind {
    // Special
    Eof,
    Invalid,

    // Keywords
    KwKernel,
    KwI32,
    KwF32,
    KwBool,
    KwTrue,
    KwFalse,
    KwIf,
    KwElse,
    KwWhile,
    KwFor,
    KwReturn,

    // Literals and Identifier
    Identifier,
    IntLiteral,
    FloatLiteral,

    // Delimiters
    LParen,          // (
    RParen,          // )
    LBrace,          // {
    RBrace,          // }
    LBracket,        // [
    RBracket,        // ]
    Semicolon,       // ;
    Comma,           // ,

    // Arithmetic Operators
    Plus,            // +
    Minus,           // -
    Star,            // *
    Slash,           // /
    Percent,         // %

    // Bitwise Operators
    Amp,             // &
    Pipe,            // |
    Caret,           // ^
    Tilde,           // ~
    LessLess,        // <<
    GreaterGreater,  // >>

    // Relational Operators
    EqualEqual,      // ==
    ExclaimEqual,    // !=
    Less,            // <
    LessEqual,       // <=
    Greater,         // >
    GreaterEqual,    // >=

    // Logical Operators
    AmpAmp,          // &&
    PipePipe,        // ||
    Exclaim,         // !

    // Assignment
    Equal            // =
};

[[nodiscard]] constexpr const char* toString(TokenKind kind) noexcept {
    switch (kind) {
        case TokenKind::Eof:            return "EOF";
        case TokenKind::Invalid:        return "INVALID";
        case TokenKind::KwKernel:       return "kernel";
        case TokenKind::KwI32:          return "i32";
        case TokenKind::KwF32:          return "f32";
        case TokenKind::KwBool:         return "bool";
        case TokenKind::KwTrue:         return "true";
        case TokenKind::KwFalse:        return "false";
        case TokenKind::KwIf:           return "if";
        case TokenKind::KwElse:         return "else";
        case TokenKind::KwWhile:        return "while";
        case TokenKind::KwFor:          return "for";
        case TokenKind::KwReturn:       return "return";
        case TokenKind::Identifier:     return "IDENTIFIER";
        case TokenKind::IntLiteral:     return "INT_LIT";
        case TokenKind::FloatLiteral:   return "FLOAT_LIT";
        case TokenKind::LParen:         return "(";
        case TokenKind::RParen:         return ")";
        case TokenKind::LBrace:         return "{";
        case TokenKind::RBrace:         return "}";
        case TokenKind::LBracket:       return "[";
        case TokenKind::RBracket:       return "]";
        case TokenKind::Semicolon:      return ";";
        case TokenKind::Comma:          return ",";
        case TokenKind::Plus:           return "+";
        case TokenKind::Minus:          return "-";
        case TokenKind::Star:           return "*";
        case TokenKind::Slash:          return "/";
        case TokenKind::Percent:        return "%";
        case TokenKind::Amp:            return "&";
        case TokenKind::Pipe:           return "|";
        case TokenKind::Caret:          return "^";
        case TokenKind::Tilde:          return "~";
        case TokenKind::LessLess:       return "<<";
        case TokenKind::GreaterGreater: return ">>";
        case TokenKind::EqualEqual:     return "==";
        case TokenKind::ExclaimEqual:   return "!=";
        case TokenKind::Less:           return "<";
        case TokenKind::LessEqual:      return "<=";
        case TokenKind::Greater:        return ">";
        case TokenKind::GreaterEqual:   return ">=";
        case TokenKind::AmpAmp:         return "&&";
        case TokenKind::PipePipe:       return "||";
        case TokenKind::Exclaim:        return "!";
        case TokenKind::Equal:          return "=";
    }
    return "UNKNOWN";
}

[[nodiscard]] inline std::optional<TokenKind> lookupKeyword(std::string_view text) noexcept {
    if (text == "kernel") return TokenKind::KwKernel;
    if (text == "i32")    return TokenKind::KwI32;
    if (text == "f32")    return TokenKind::KwF32;
    if (text == "bool")   return TokenKind::KwBool;
    if (text == "true")   return TokenKind::KwTrue;
    if (text == "false")  return TokenKind::KwFalse;
    if (text == "if")     return TokenKind::KwIf;
    if (text == "else")   return TokenKind::KwElse;
    if (text == "while")  return TokenKind::KwWhile;
    if (text == "for")    return TokenKind::KwFor;
    if (text == "return") return TokenKind::KwReturn;
    return std::nullopt;
}

} // namespace ptxlab
