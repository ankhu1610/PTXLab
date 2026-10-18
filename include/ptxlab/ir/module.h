#pragma once

#include <memory>
#include <string>
#include <vector>
#include "ptxlab/ir/function.h"

namespace ptxlab::ir {

class Module {
public:
    Module() = default;

    Function* createFunction(std::string name) {
        auto func = std::make_unique<Function>(std::move(name));
        Function* ptr = func.get();
        functions_.push_back(std::move(func));
        return ptr;
    }

    [[nodiscard]] const std::vector<std::unique_ptr<Function>>& functions() const noexcept {
        return functions_;
    }

private:
    std::vector<std::unique_ptr<Function>> functions_;
};

} // namespace ptxlab::ir
