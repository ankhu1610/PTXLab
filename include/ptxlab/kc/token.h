#pragma once

#include <cstdint>
#include <string_view>
#include "ptxlab/kc/token_kind.h"
#include "ptxlab/support/source_location.h"

namespace ptxlab {

struct Token {
    TokenKind kind = TokenKind::Invalid;
    SourceLocation location;
    std::string_view text;
    int32_t intVal = 0;
    float floatVal = 0.0f;

    [[nodiscard]] constexpr bool is(TokenKind k) const noexcept {
        return kind == k;
    }

    [[nodiscard]] constexpr bool isNot(TokenKind k) const noexcept {
        return kind != k;
    }

    template <typename... Kinds>
    [[nodiscard]] constexpr bool isOneOf(TokenKind k1, Kinds... rest) const noexcept {
        return (is(k1) || ... || is(rest));
    }
};

} // namespace ptxlab
