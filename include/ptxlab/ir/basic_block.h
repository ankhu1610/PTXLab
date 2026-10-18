#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "ptxlab/ir/instruction.h"

namespace ptxlab::ir {

class Function;

class BasicBlock {
public:
    explicit BasicBlock(std::string label, Function* parent = nullptr)
        : label_(std::move(label)), parent_(parent) {}

    [[nodiscard]] const std::string& label() const noexcept { return label_; }
    [[nodiscard]] Function* parent() const noexcept { return parent_; }
    void setParent(Function* parent) noexcept { parent_ = parent; }

    void appendInstruction(std::unique_ptr<Instruction> inst) {
        instructions_.push_back(std::move(inst));
    }

    [[nodiscard]] bool hasTerminator() const noexcept {
        return !instructions_.empty() && instructions_.back()->isTerminator();
    }

    [[nodiscard]] Instruction* terminator() const noexcept {
        if (hasTerminator()) {
            return instructions_.back().get();
        }
        return nullptr;
    }

    [[nodiscard]] const std::vector<std::unique_ptr<Instruction>>& instructions() const noexcept {
        return instructions_;
    }

    [[nodiscard]] const std::vector<BasicBlock*>& predecessors() const noexcept { return predecessors_; }
    [[nodiscard]] const std::vector<BasicBlock*>& successors() const noexcept { return successors_; }

    void clearEdges() noexcept {
        predecessors_.clear();
        successors_.clear();
    }

    void addSuccessor(BasicBlock* succ) {
        if (std::find(successors_.begin(), successors_.end(), succ) == successors_.end()) {
            successors_.push_back(succ);
            succ->addPredecessor(this);
        }
    }

    void addPredecessor(BasicBlock* pred) {
        if (std::find(predecessors_.begin(), predecessors_.end(), pred) == predecessors_.end()) {
            predecessors_.push_back(pred);
        }
    }

private:
    std::string label_;
    Function* parent_ = nullptr;
    std::vector<std::unique_ptr<Instruction>> instructions_;
    std::vector<BasicBlock*> predecessors_;
    std::vector<BasicBlock*> successors_;
};

} // namespace ptxlab::ir
