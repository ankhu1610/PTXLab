#include "ptxlab/ir/function.h"

namespace ptxlab::ir {

void Function::rebuildCFG() {
    for (auto& bb : blocks_) {
        bb->clearEdges();
    }

    for (auto& bb : blocks_) {
        if (auto* term = bb->terminator()) {
            if (term->opcode() == Opcode::Br) {
                if (auto* target = getBlock(term->targetTrue())) {
                    bb->addSuccessor(target);
                }
            } else if (term->opcode() == Opcode::CondBr) {
                if (auto* tTarget = getBlock(term->targetTrue())) {
                    bb->addSuccessor(tTarget);
                }
                if (auto* fTarget = getBlock(term->targetFalse())) {
                    bb->addSuccessor(fTarget);
                }
            }
        }
    }
}

} // namespace ptxlab::ir
