#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "ptxlab/ir/basic_block.h"

namespace ptxlab::ir {

struct IRParam {
    std::string name;
    IRType type;
    int regId = -1;
};

class Function {
public:
    explicit Function(std::string name)
        : name_(std::move(name)) {
        // Create standard entry block
        entryBlock_ = createBlock("entry");
    }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    BasicBlock* createBlock(const std::string& nameHint = "bb") {
        std::string uniqueLabel = nameHint;
        if (uniqueLabel.empty()) {
            uniqueLabel = "bb";
        }

        int count = blockLabelCounts_[uniqueLabel]++;
        if (count > 0 || uniqueLabel != "entry") {
            uniqueLabel += "_" + std::to_string(count);
        }

        auto bb = std::make_unique<BasicBlock>(uniqueLabel, this);
        BasicBlock* ptr = bb.get();
        blocks_.push_back(std::move(bb));
        return ptr;
    }

    [[nodiscard]] BasicBlock* entryBlock() const noexcept { return entryBlock_; }

    [[nodiscard]] BasicBlock* getBlock(std::string_view label) const {
        for (const auto& bb : blocks_) {
            if (bb->label() == label) {
                return bb.get();
            }
        }
        return nullptr;
    }

    [[nodiscard]] const std::vector<std::unique_ptr<BasicBlock>>& blocks() const noexcept {
        return blocks_;
    }

    void addParam(std::string name, IRType type, int regId) {
        params_.push_back(IRParam{std::move(name), type, regId});
    }

    [[nodiscard]] const std::vector<IRParam>& params() const noexcept { return params_; }

    [[nodiscard]] int nextRegId() noexcept { return nextRegId_++; }
    [[nodiscard]] int nextPredId() noexcept { return nextPredId_++; }

    void rebuildCFG();

private:
    std::string name_;
    std::vector<IRParam> params_;
    std::vector<std::unique_ptr<BasicBlock>> blocks_;
    BasicBlock* entryBlock_ = nullptr;
    std::unordered_map<std::string, int> blockLabelCounts_;
    int nextRegId_ = 0;
    int nextPredId_ = 0;
};

} // namespace ptxlab::ir
