#pragma once

#include <ostream>
#include <string>
#include "ptxlab/ir/module.h"

namespace ptxlab::ir {

class IRPrinter {
public:
    static void print(const Module& module, std::ostream& os);
    static void print(const Function& function, std::ostream& os);
    static void print(const BasicBlock& block, std::ostream& os);

    [[nodiscard]] static std::string dump(const Module& module);
    [[nodiscard]] static std::string dump(const Function& function);
};

} // namespace ptxlab::ir
