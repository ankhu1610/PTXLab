#pragma once

#include <ostream>
#include <string>
#include "ptxlab/ir/basic_block.h"
#include "ptxlab/ir/function.h"
#include "ptxlab/ir/instruction.h"
#include "ptxlab/ir/module.h"

namespace ptxlab::ptx {

struct PTXOptions {
    std::string target = "sm_75";
    std::string version = "7.0";
    int addressSize = 64;
};

class PTXEmitter {
public:
    explicit PTXEmitter(PTXOptions options = {})
        : options_(std::move(options)) {}

    [[nodiscard]] std::string emit(const ir::Module& module);
    [[nodiscard]] std::string emit(const ir::Function& function);

private:
    void emitHeader(std::ostream& os) const;
    void emitFunction(const ir::Function& function, std::ostream& os);
    void emitBlock(const ir::Function& func, const ir::BasicBlock& block, std::ostream& os);
    void emitInstruction(const ir::Function& func, const ir::Instruction& inst, std::ostream& os);
    [[nodiscard]] std::string formatOperand(const ir::IROperand& op) const;

    PTXOptions options_;
};

} // namespace ptxlab::ptx
