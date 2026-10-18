#include "ptxlab/ir/ir_verifier.h"
#include <unordered_set>

namespace ptxlab::ir {

bool IRVerifier::verify(const Module& module) {
    clear();
    for (const auto& func : module.functions()) {
        verify(*func);
    }
    return !hasErrors();
}

bool IRVerifier::verify(const Function& function) {
    if (function.blocks().empty()) {
        error("function '" + function.name() + "' has no basic blocks");
        return false;
    }

    // 1. Entry block check: must exist and have 0 predecessors
    const auto* entry = function.entryBlock();
    if (!entry) {
        error("function '" + function.name() + "' has no entry block");
    } else if (!entry->predecessors().empty()) {
        error("entry block '" + entry->label() + "' must have zero predecessors (has " +
              std::to_string(entry->predecessors().size()) + ")", entry->label());
    }

    // 2. Verify all basic blocks
    for (const auto& bb : function.blocks()) {
        verifyBlock(*bb, function);
    }

    // 3. Verify CFG edge symmetry
    for (const auto& bb : function.blocks()) {
        for (const auto* succ : bb->successors()) {
            bool foundPred = false;
            for (const auto* pred : succ->predecessors()) {
                if (pred == bb.get()) {
                    foundPred = true;
                    break;
                }
            }
            if (!foundPred) {
                error("asymmetric CFG edge: block '" + bb->label() + "' has successor '" +
                      succ->label() + "', but '" + succ->label() + "' does not list '" +
                      bb->label() + "' as predecessor", bb->label());
            }
        }
    }

    return !hasErrors();
}

void IRVerifier::verifyBlock(const BasicBlock& block, const Function& func) {
    if (block.instructions().empty()) {
        error("block '" + block.label() + "' is empty and has no terminator", block.label());
        return;
    }

    // 1. Check terminator placement: exactly one terminator at the very end
    for (size_t i = 0; i < block.instructions().size(); ++i) {
        const auto& inst = block.instructions()[i];
        bool isLast = (i == block.instructions().size() - 1);

        if (inst->isTerminator()) {
            if (!isLast) {
                error("terminator found before the end of block '" + block.label() +
                      "': " + inst->toString(), block.label(), inst->location());
            }
        } else {
            if (isLast) {
                error("block '" + block.label() + "' does not end with a terminator",
                      block.label(), inst->location());
            }
        }

        verifyInstruction(*inst, block, func);
    }
}

void IRVerifier::verifyInstruction(const Instruction& inst, const BasicBlock& block, const Function& func) {
    // 1. Check branch targets
    if (inst.opcode() == Opcode::Br) {
        if (!func.getBlock(inst.targetTrue())) {
            error("branch target '" + inst.targetTrue() + "' does not exist in function",
                  block.label(), inst.location());
        }
    } else if (inst.opcode() == Opcode::CondBr) {
        if (!inst.operands().empty() && !inst.operands()[0].type.isPred()) {
            error("condbr condition must be of type 'pred', got '" +
                  inst.operands()[0].type.toString() + "'", block.label(), inst.location());
        }
        if (!func.getBlock(inst.targetTrue())) {
            error("condbr true target '" + inst.targetTrue() + "' does not exist in function",
                  block.label(), inst.location());
        }
        if (!func.getBlock(inst.targetFalse())) {
            error("condbr false target '" + inst.targetFalse() + "' does not exist in function",
                  block.label(), inst.location());
        }
    }

    // 2. Type consistency
    switch (inst.opcode()) {
        case Opcode::Add:
        case Opcode::Sub:
        case Opcode::Mul:
        case Opcode::Div:
        case Opcode::Rem:
        case Opcode::And:
        case Opcode::Or:
        case Opcode::Xor:
        case Opcode::Shl:
        case Opcode::Shr:
            if (inst.operands().size() < 2 || !inst.operands()[0].type.isInt() || !inst.operands()[1].type.isInt()) {
                error("integer binary op '" + inst.toString() + "' requires i32 operands", block.label(), inst.location());
            }
            if (!inst.hasDst() || !inst.dst().type.isInt()) {
                error("integer binary op '" + inst.toString() + "' destination must be i32", block.label(), inst.location());
            }
            break;

        case Opcode::FAdd:
        case Opcode::FSub:
        case Opcode::FMul:
        case Opcode::FDiv:
            if (inst.operands().size() < 2 || !inst.operands()[0].type.isFloat() || !inst.operands()[1].type.isFloat()) {
                error("float binary op '" + inst.toString() + "' requires f32 operands", block.label(), inst.location());
            }
            if (!inst.hasDst() || !inst.dst().type.isFloat()) {
                error("float binary op '" + inst.toString() + "' destination must be f32", block.label(), inst.location());
            }
            break;

        case Opcode::CmpEq:
        case Opcode::CmpNe:
        case Opcode::CmpLt:
        case Opcode::CmpLe:
        case Opcode::CmpGt:
        case Opcode::CmpGe:
            if (inst.operands().size() < 2 || inst.operands()[0].type != inst.operands()[1].type) {
                error("cmp op '" + inst.toString() + "' requires matching operand types", block.label(), inst.location());
            }
            if (!inst.hasDst() || !inst.dst().type.isPred()) {
                error("cmp op '" + inst.toString() + "' destination must be pred", block.label(), inst.location());
            }
            break;

        case Opcode::ElemAddr:
            if (inst.operands().size() < 2 || !inst.operands()[0].type.isPtr() || !inst.operands()[1].type.isInt()) {
                error("elemaddr requires pointer base and integer index", block.label(), inst.location());
            }
            if (!inst.hasDst() || !inst.dst().type.isPtr()) {
                error("elemaddr destination must be a pointer", block.label(), inst.location());
            }
            break;

        case Opcode::Load:
            if (inst.operands().empty() || !inst.operands()[0].type.isPtr()) {
                error("load requires pointer address operand", block.label(), inst.location());
            }
            break;

        case Opcode::Store:
            if (inst.operands().size() < 2 || !inst.operands()[0].type.isPtr()) {
                error("store requires pointer address as first operand", block.label(), inst.location());
            }
            break;

        case Opcode::Copy:
            if (!inst.hasDst()) {
                error("copy instruction requires destination register", block.label(), inst.location());
            }
            if (inst.operands().empty()) {
                error("copy instruction requires source operand", block.label(), inst.location());
            } else if (inst.dst().type != inst.operands()[0].type) {
                error("copy type mismatch: dst is " + inst.dst().type.toString() +
                      ", src is " + inst.operands()[0].type.toString(), block.label(), inst.location());
            }
            break;

        case Opcode::Const:
            if (!inst.hasDst()) {
                error("const instruction requires destination register", block.label(), inst.location());
            }
            if (inst.operands().empty()) {
                error("const instruction requires value operand", block.label(), inst.location());
            } else if (inst.dst().type != inst.operands()[0].type) {
                error("const type mismatch: dst is " + inst.dst().type.toString() +
                      ", src is " + inst.operands()[0].type.toString(), block.label(), inst.location());
            }
            break;

        default:
            break;
    }
}

} // namespace ptxlab::ir
