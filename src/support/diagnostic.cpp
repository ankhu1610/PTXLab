#include "ptxlab/support/diagnostic.h"
#include <sstream>

namespace ptxlab {

void DiagnosticEngine::report(DiagnosticLevel level, SourceLocation loc, std::string message) {
    if (level == DiagnosticLevel::Error) {
        ++errorCount_;
    } else if (level == DiagnosticLevel::Warning) {
        ++warningCount_;
    }
    diagnostics_.push_back(Diagnostic{level, loc, std::move(message)});
}

std::string DiagnosticEngine::format(const Diagnostic& diag, const SourceManager& sm) const {
    std::ostringstream oss;
    
    std::string_view filename = sm.getFilename();
    if (filename.empty()) {
        filename = "<input>";
    }

    if (diag.location.isValid()) {
        oss << filename << ":" << diag.location.line << ":" << diag.location.column << ": ";
    } else {
        oss << filename << ": ";
    }

    oss << toString(diag.level) << ": " << diag.message << "\n";

    if (diag.location.isValid()) {
        std::string_view lineContent = sm.getLineContent(diag.location.line);
        if (!lineContent.empty()) {
            oss << "  " << lineContent << "\n";
            oss << "  ";
            for (uint32_t c = 1; c < diag.location.column; ++c) {
                // If the source character was a tab, preserve it
                size_t charIdx = c - 1;
                if (charIdx < lineContent.size() && lineContent[charIdx] == '\t') {
                    oss << '\t';
                } else {
                    oss << ' ';
                }
            }
            oss << "^\n";
        }
    }

    return oss.str();
}

} // namespace ptxlab
