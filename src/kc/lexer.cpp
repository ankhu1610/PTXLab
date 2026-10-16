#include "ptxlab/kc/lexer.h"
#include <cctype>
#include <charconv>

namespace ptxlab {

Lexer::Lexer(const SourceManager& sourceMgr, DiagnosticEngine& diagEngine)
    : sm_(sourceMgr), diag_(diagEngine), source_(sourceMgr.getSource()) {
}

char Lexer::peek(size_t offset) const noexcept {
    if (cursor_ + offset >= source_.size()) {
        return '\0';
    }
    return source_[cursor_ + offset];
}

char Lexer::advance() noexcept {
    if (isAtEnd()) {
        return '\0';
    }
    return source_[cursor_++];
}

bool Lexer::match(char expected) noexcept {
    if (isAtEnd() || source_[cursor_] != expected) {
        return false;
    }
    ++cursor_;
    return true;
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            // Line comment: skip until '\n' or EOF
            advance(); // '/'
            advance(); // '/'
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
        } else if (c == '/' && peek(1) == '*') {
            // Block comment: skip until '*/' or EOF
            size_t startOffset = cursor_;
            advance(); // '/'
            advance(); // '*'
            bool terminated = false;
            while (!isAtEnd()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance(); // '*'
                    advance(); // '/'
                    terminated = true;
                    break;
                }
                advance();
            }
            if (!terminated) {
                diag_.error(sm_.getLocation(startOffset), "unterminated block comment");
                return;
            }
        } else {
            break;
        }
    }
}

Token Lexer::scanIdentifierOrKeyword(size_t startOffset) {
    while (!isAtEnd()) {
        char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            advance();
        } else {
            break;
        }
    }

    std::string_view text(source_.data() + startOffset, cursor_ - startOffset);
    SourceLocation loc = sm_.getLocation(startOffset);

    if (auto kw = lookupKeyword(text)) {
        return Token{*kw, loc, text, 0, 0.0f};
    }
    return Token{TokenKind::Identifier, loc, text, 0, 0.0f};
}

Token Lexer::scanNumber(size_t startOffset) {
    SourceLocation loc = sm_.getLocation(startOffset);

    // Check for hex literal: 0x or 0X
    if (source_[startOffset] == '0' && (peek() == 'x' || peek() == 'X')) {
        advance(); // consume 'x' or 'X'
        size_t hexStart = cursor_;
        while (!isAtEnd() && std::isxdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }

        if (cursor_ == hexStart) {
            diag_.error(loc, "hexadecimal literal has no digits");
            std::string_view text(source_.data() + startOffset, cursor_ - startOffset);
            return Token{TokenKind::Invalid, loc, text, 0, 0.0f};
        }

        std::string_view hexText(source_.data() + hexStart, cursor_ - hexStart);
        uint64_t val = 0;
        auto result = std::from_chars(hexText.data(), hexText.data() + hexText.size(), val, 16);
        if (result.ec == std::errc::result_out_of_range || val > 0xFFFFFFFFULL) {
            diag_.error(loc, "hexadecimal literal exceeds 32-bit range");
        }

        std::string_view fullText(source_.data() + startOffset, cursor_ - startOffset);
        return Token{TokenKind::IntLiteral, loc, fullText, static_cast<int32_t>(val), 0.0f};
    }

    // Decimal integer or float
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    char next = peek();
    if (next == '.' || next == 'e' || next == 'E') {
        return scanFloatTail(startOffset, false, false);
    }

    // It's a decimal integer
    std::string_view intText(source_.data() + startOffset, cursor_ - startOffset);
    uint64_t val = 0;
    auto result = std::from_chars(intText.data(), intText.data() + intText.size(), val, 10);
    if (result.ec == std::errc::result_out_of_range || val > 0xFFFFFFFFULL) {
        diag_.error(loc, "integer literal exceeds 32-bit range");
    }

    return Token{TokenKind::IntLiteral, loc, intText, static_cast<int32_t>(val), 0.0f};
}

Token Lexer::scanFloatTail(size_t startOffset, bool hasDot, bool hasExp) {
    SourceLocation loc = sm_.getLocation(startOffset);

    if (!hasDot && peek() == '.') {
        advance(); // consume '.'
        hasDot = true;
    }

    if (hasDot) {
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    if (!hasExp && (peek() == 'e' || peek() == 'E')) {
        advance(); // consume 'e' or 'E'
        hasExp = true;
        if (peek() == '+' || peek() == '-') {
            advance();
        }
        size_t expDigitsStart = cursor_;
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
        if (cursor_ == expDigitsStart) {
            diag_.error(loc, "floating-point exponent requires at least one digit");
            std::string_view text(source_.data() + startOffset, cursor_ - startOffset);
            return Token{TokenKind::Invalid, loc, text, 0, 0.0f};
        }
    }

    // Optional 'f' or 'F' suffix
    bool hasSuffix = false;
    if (peek() == 'f' || peek() == 'F') {
        advance();
        hasSuffix = true;
    }

    std::string_view fullText(source_.data() + startOffset, cursor_ - startOffset);
    std::string_view parseText = fullText;
    if (hasSuffix) {
        parseText = parseText.substr(0, parseText.size() - 1);
    }

    float val = 0.0f;
    auto result = std::from_chars(parseText.data(), parseText.data() + parseText.size(), val);
    if (result.ec == std::errc::result_out_of_range) {
        diag_.error(loc, "floating-point literal out of range");
    }

    return Token{TokenKind::FloatLiteral, loc, fullText, 0, val};
}

Token Lexer::nextToken() {
    skipWhitespaceAndComments();

    if (isAtEnd()) {
        return Token{TokenKind::Eof, sm_.getLocation(cursor_), "", 0, 0.0f};
    }

    size_t startOffset = cursor_;
    char c = advance();
    SourceLocation loc = sm_.getLocation(startOffset);

    // Identifiers or keywords
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return scanIdentifierOrKeyword(startOffset);
    }

    // Number literals
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return scanNumber(startOffset);
    }

    // Float starting with dot (.5, .123)
    if (c == '.' && std::isdigit(static_cast<unsigned char>(peek()))) {
        return scanFloatTail(startOffset, true, false);
    }

    // Delimiters and operators
    switch (c) {
        case '(': return Token{TokenKind::LParen, loc, "(", 0, 0.0f};
        case ')': return Token{TokenKind::RParen, loc, ")", 0, 0.0f};
        case '{': return Token{TokenKind::LBrace, loc, "{", 0, 0.0f};
        case '}': return Token{TokenKind::RBrace, loc, "}", 0, 0.0f};
        case '[': return Token{TokenKind::LBracket, loc, "[", 0, 0.0f};
        case ']': return Token{TokenKind::RBracket, loc, "]", 0, 0.0f};
        case ';': return Token{TokenKind::Semicolon, loc, ";", 0, 0.0f};
        case ',': return Token{TokenKind::Comma, loc, ",", 0, 0.0f};

        case '+': return Token{TokenKind::Plus, loc, "+", 0, 0.0f};
        case '-': return Token{TokenKind::Minus, loc, "-", 0, 0.0f};
        case '*': return Token{TokenKind::Star, loc, "*", 0, 0.0f};
        case '/': return Token{TokenKind::Slash, loc, "/", 0, 0.0f};
        case '%': return Token{TokenKind::Percent, loc, "%", 0, 0.0f};

        case '^': return Token{TokenKind::Caret, loc, "^", 0, 0.0f};
        case '~': return Token{TokenKind::Tilde, loc, "~", 0, 0.0f};

        case '&':
            if (match('&')) {
                return Token{TokenKind::AmpAmp, loc, "&&", 0, 0.0f};
            }
            return Token{TokenKind::Amp, loc, "&", 0, 0.0f};

        case '|':
            if (match('|')) {
                return Token{TokenKind::PipePipe, loc, "||", 0, 0.0f};
            }
            return Token{TokenKind::Pipe, loc, "|", 0, 0.0f};

        case '!':
            if (match('=')) {
                return Token{TokenKind::ExclaimEqual, loc, "!=", 0, 0.0f};
            }
            return Token{TokenKind::Exclaim, loc, "!", 0, 0.0f};

        case '=':
            if (match('=')) {
                return Token{TokenKind::EqualEqual, loc, "==", 0, 0.0f};
            }
            return Token{TokenKind::Equal, loc, "=", 0, 0.0f};

        case '<':
            if (match('=')) {
                return Token{TokenKind::LessEqual, loc, "<=", 0, 0.0f};
            }
            if (match('<')) {
                return Token{TokenKind::LessLess, loc, "<<", 0, 0.0f};
            }
            return Token{TokenKind::Less, loc, "<", 0, 0.0f};

        case '>':
            if (match('=')) {
                return Token{TokenKind::GreaterEqual, loc, ">=", 0, 0.0f};
            }
            if (match('>')) {
                return Token{TokenKind::GreaterGreater, loc, ">>", 0, 0.0f};
            }
            return Token{TokenKind::Greater, loc, ">", 0, 0.0f};

        default: {
            std::string msg = "unexpected character '";
            msg += c;
            msg += "'";
            diag_.error(loc, std::move(msg));
            std::string_view text(source_.data() + startOffset, 1);
            return Token{TokenKind::Invalid, loc, text, 0, 0.0f};
        }
    }
}

std::vector<Token> Lexer::tokenizeAll() {
    std::vector<Token> tokens;
    while (true) {
        Token tok = nextToken();
        tokens.push_back(tok);
        if (tok.is(TokenKind::Eof)) {
            break;
        }
    }
    return tokens;
}

} // namespace ptxlab
