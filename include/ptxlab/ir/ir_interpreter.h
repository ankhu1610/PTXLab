#pragma once

#include <unordered_map>
#include "ptxlab/kc/ast_interpreter.h"
#include "ptxlab/ir/function.h"

namespace ptxlab::ir {

class IRInterpreter {
public:
    explicit IRInterpreter(BufferManager& buffers, uint64_t maxSteps = 1'000'000);

    /// Executes a kernel function across a 1D grid
    void launch1D(const Function& function,
                  int gridDim,
                  int blockDim,
                  const std::unordered_map<std::string, KernelArg>& args);

private:
    Value readOperand(const IROperand& op, SourceLocation loc = {});
    void executeInstruction(const Instruction& inst);

    void setDst(const Instruction& inst, Value val);

    BufferManager& buffers_;
    uint64_t maxSteps_;
    uint64_t stepCount_ = 0;

    int32_t tid_x_ = 0;
    int32_t ctaid_x_ = 0;
    int32_t ntid_x_ = 1;

    std::unordered_map<int, Value> registers_;
    std::unordered_map<int, bool> predicates_;
};

} // namespace ptxlab::ir
