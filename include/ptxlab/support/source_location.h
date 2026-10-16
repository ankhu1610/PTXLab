#pragma once

#include <cstdint>
#include <string>

namespace ptxlab {

struct SourceLocation {
    uint32_t line = 0;   // 1-based line number (0 = invalid/unknown)
    uint32_t column = 0; // 1-based column number (0 = invalid/unknown)

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return line != 0 && column != 0;
    }

    constexpr bool operator==(const SourceLocation& other) const noexcept = default;
};

struct SourceRange {
    SourceLocation begin;
    SourceLocation end;

    [[nodiscard]] constexpr bool isValid() const noexcept {
        return begin.isValid() && end.isValid();
    }

    constexpr bool operator==(const SourceRange& other) const noexcept = default;
};

} // namespace ptxlab
