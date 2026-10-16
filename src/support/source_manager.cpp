#include "ptxlab/support/source_manager.h"
#include <algorithm>

namespace ptxlab {

SourceManager::SourceManager(std::string filename, std::string sourceCode)
    : filename_(std::move(filename)), source_(std::move(sourceCode)) {
    computeLineOffsets();
}

void SourceManager::setSource(std::string filename, std::string sourceCode) {
    filename_ = std::move(filename);
    source_ = std::move(sourceCode);
    computeLineOffsets();
}

void SourceManager::computeLineOffsets() {
    lineOffsets_.clear();
    lineOffsets_.push_back(0); // Line 1 always starts at index 0

    for (size_t i = 0; i < source_.size(); ++i) {
        if (source_[i] == '\n') {
            lineOffsets_.push_back(i + 1);
        }
    }
}

SourceLocation SourceManager::getLocation(size_t byteOffset) const {
    if (lineOffsets_.empty()) {
        return {0, 0};
    }

    if (byteOffset > source_.size()) {
        byteOffset = source_.size();
    }

    auto it = std::upper_bound(lineOffsets_.begin(), lineOffsets_.end(), byteOffset);
    auto lineIdx = static_cast<size_t>(std::distance(lineOffsets_.begin(), it) - 1);

    uint32_t line = static_cast<uint32_t>(lineIdx + 1);
    size_t lineStart = lineOffsets_[lineIdx];
    uint32_t column = static_cast<uint32_t>(byteOffset - lineStart + 1);

    return {line, column};
}

std::string_view SourceManager::getLineContent(uint32_t line) const {
    if (line == 0 || line > lineOffsets_.size()) {
        return {};
    }

    size_t start = lineOffsets_[line - 1];
    size_t end = (line < lineOffsets_.size()) ? lineOffsets_[line] : source_.size();

    // Strip trailing '\n' and '\r' for clean printing
    if (end > start && source_[end - 1] == '\n') {
        --end;
    }
    if (end > start && source_[end - 1] == '\r') {
        --end;
    }

    if (start >= source_.size()) {
        return {};
    }

    return std::string_view(source_.data() + start, end - start);
}

} // namespace ptxlab
