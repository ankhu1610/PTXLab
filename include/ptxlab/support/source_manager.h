#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "ptxlab/support/source_location.h"

namespace ptxlab {

class SourceManager {
public:
    SourceManager() = default;
    SourceManager(std::string filename, std::string sourceCode);

    void setSource(std::string filename, std::string sourceCode);

    [[nodiscard]] SourceLocation getLocation(size_t byteOffset) const;
    [[nodiscard]] std::string_view getLineContent(uint32_t line) const;
    [[nodiscard]] std::string_view getFilename() const noexcept { return filename_; }
    [[nodiscard]] std::string_view getSource() const noexcept { return source_; }
    [[nodiscard]] size_t getLineCount() const noexcept { return lineOffsets_.empty() ? 0 : lineOffsets_.size(); }

private:
    void computeLineOffsets();

    std::string filename_;
    std::string source_;
    std::vector<size_t> lineOffsets_; // 0-based byte offset where each 1-based line begins
};

} // namespace ptxlab
