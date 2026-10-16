#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "ptxlab/support/source_location.h"
#include "ptxlab/support/source_manager.h"

namespace ptxlab {

enum class DiagnosticLevel {
    Note,
    Warning,
    Error
};

[[nodiscard]] constexpr const char* toString(DiagnosticLevel level) noexcept {
    switch (level) {
        case DiagnosticLevel::Note:    return "note";
        case DiagnosticLevel::Warning: return "warning";
        case DiagnosticLevel::Error:   return "error";
    }
    return "unknown";
}

struct Diagnostic {
    DiagnosticLevel level;
    SourceLocation location;
    std::string message;
};

class DiagnosticEngine {
public:
    DiagnosticEngine() = default;

    void report(DiagnosticLevel level, SourceLocation loc, std::string message);
    void error(SourceLocation loc, std::string message) {
        report(DiagnosticLevel::Error, loc, std::move(message));
    }
    void warning(SourceLocation loc, std::string message) {
        report(DiagnosticLevel::Warning, loc, std::move(message));
    }
    void note(SourceLocation loc, std::string message) {
        report(DiagnosticLevel::Note, loc, std::move(message));
    }

    [[nodiscard]] bool hasErrors() const noexcept { return errorCount_ > 0; }
    [[nodiscard]] size_t errorCount() const noexcept { return errorCount_; }
    [[nodiscard]] size_t warningCount() const noexcept { return warningCount_; }
    [[nodiscard]] const std::vector<Diagnostic>& getDiagnostics() const noexcept { return diagnostics_; }

    void clear() noexcept {
        diagnostics_.clear();
        errorCount_ = 0;
        warningCount_ = 0;
    }

    [[nodiscard]] std::string format(const Diagnostic& diag, const SourceManager& sm) const;

private:
    size_t errorCount_ = 0;
    size_t warningCount_ = 0;
    std::vector<Diagnostic> diagnostics_;
};

} // namespace ptxlab
