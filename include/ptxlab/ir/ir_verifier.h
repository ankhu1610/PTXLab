#pragma once

#include <string>
#include <vector>
#include "ptxlab/ir/function.h"
#include "ptxlab/ir/module.h"

namespace ptxlab::ir {

struct VerificationError {
    std::string message;
    std::string blockLabel;
    SourceLocation location;
};

class IRVerifier {
public:
    IRVerifier() = default;

    /// Verifies an entire Module. Returns true if valid, false if errors found.
    bool verify(const Module& module);

    /// Verifies a single Function.
    bool verify(const Function& function);

    [[nodiscard]] const std::vector<VerificationError>& errors() const noexcept { return errors_; }
    [[nodiscard]] bool hasErrors() const noexcept { return !errors_.empty(); }

    void clear() noexcept { errors_.clear(); }

private:
    void error(std::string message, std::string blockLabel = "", SourceLocation loc = {}) {
        errors_.push_back(VerificationError{std::move(message), std::move(blockLabel), loc});
    }

    void verifyBlock(const BasicBlock& block, const Function& func);
    void verifyInstruction(const Instruction& inst, const BasicBlock& block, const Function& func);

    std::vector<VerificationError> errors_;
};

} // namespace ptxlab::ir
