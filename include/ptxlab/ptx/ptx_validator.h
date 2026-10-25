#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ptxlab/ptx/ptx_ast.h"
#include "ptxlab/support/diagnostic.h"

namespace ptxlab::ptx {

class PTXValidator {
public:
    explicit PTXValidator(DiagnosticEngine* diag = nullptr);

    bool validate(const PTXModule& module);

    bool hasErrors() const { return !errors_.empty(); }
    const std::vector<std::string>& errors() const { return errors_; }

private:
    void error(std::string msg, int line = 0);

    bool validateHeader(const PTXModule& module);
    bool validateKernel(const PTXKernel& kernel);
    bool validateInstruction(const PTXKernel& kernel,
                             const PTXInstruction& inst,
                             const std::unordered_map<std::string, PTXType>& declaredRegs,
                             const std::unordered_set<std::string>& labels,
                             const std::unordered_set<std::string>& params);

    DiagnosticEngine* diag_{nullptr};
    std::vector<std::string> errors_;
};

} // namespace ptxlab::ptx
