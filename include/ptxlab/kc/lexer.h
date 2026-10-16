#pragma once

#include <cstddef>
#include <string_view>
#include <vector>
#include "ptxlab/kc/token.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

namespace ptxlab {

class Lexer {
public:
    Lexer(const SourceManager& sourceMgr, DiagnosticEngine& diagEngine);

    [[nodiscard]] Token nextToken();
    [[nodiscard]] std::vector<Token> tokenizeAll();

    void reset() noexcept { cursor_ = 0; }

private:
    [[nodiscard]] bool isAtEnd() const noexcept { return cursor_ >= source_.size(); }
    [[nodiscard]] char peek(size_t offset = 0) const noexcept;
    char advance() noexcept;
    bool match(char expected) noexcept;

    void skipWhitespaceAndComments();
    Token scanIdentifierOrKeyword(size_t startOffset);
    Token scanNumber(size_t startOffset);
    Token scanFloatTail(size_t startOffset, bool hasDot, bool hasExp);

    const SourceManager& sm_;
    DiagnosticEngine& diag_;
    std::string_view source_;
    size_t cursor_ = 0;
};

} // namespace ptxlab
