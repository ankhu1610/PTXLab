#include "ptxlab/ir/ir_printer.h"
#include <sstream>

namespace ptxlab::ir {

void IRPrinter::print(const Module& module, std::ostream& os) {
    for (size_t i = 0; i < module.functions().size(); ++i) {
        if (i > 0) {
            os << "\n";
        }
        print(*module.functions()[i], os);
    }
}

void IRPrinter::print(const Function& function, std::ostream& os) {
    os << "kernel " << function.name() << "(";
    for (size_t i = 0; i < function.params().size(); ++i) {
        if (i > 0) {
            os << ", ";
        }
        const auto& p = function.params()[i];
        os << p.type.toString() << " %" << p.name;
    }
    os << ") {\n";

    for (const auto& bb : function.blocks()) {
        print(*bb, os);
    }

    os << "}\n";
}

void IRPrinter::print(const BasicBlock& block, std::ostream& os) {
    os << block.label() << ":\n";
    for (const auto& inst : block.instructions()) {
        os << "    " << inst->toString() << "\n";
    }
}

std::string IRPrinter::dump(const Module& module) {
    std::ostringstream oss;
    print(module, oss);
    return oss.str();
}

std::string IRPrinter::dump(const Function& function) {
    std::ostringstream oss;
    print(function, oss);
    return oss.str();
}

} // namespace ptxlab::ir
