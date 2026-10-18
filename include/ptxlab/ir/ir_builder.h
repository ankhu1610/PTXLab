#pragma once

#include <memory>
#include <string>
#include "ptxlab/ir/basic_block.h"
#include "ptxlab/ir/function.h"
#include "ptxlab/ir/instruction.h"

namespace ptxlab::ir {

class IRBuilder {
public:
    IRBuilder() = default;
    explicit IRBuilder(Function* func) : function_(func) {
        if (func) {
            insertBlock_ = func->entryBlock();
        }
    }

    void setFunction(Function* func) noexcept {
        function_ = func;
        if (func) {
            insertBlock_ = func->entryBlock();
        } else {
            insertBlock_ = nullptr;
        }
    }

    [[nodiscard]] Function* getFunction() const noexcept { return function_; }

    void setInsertPoint(BasicBlock* bb) noexcept { insertBlock_ = bb; }
    [[nodiscard]] BasicBlock* getInsertPoint() const noexcept { return insertBlock_; }

    // Constants & Special
    IROperand createConstInt(int32_t val, SourceLocation loc = {});
    IROperand createConstFloat(float val, SourceLocation loc = {});
    IROperand createConstBool(bool val, SourceLocation loc = {});
    IROperand createCopy(IROperand src, SourceLocation loc = {});
    void createCopy(IROperand dst, IROperand src, SourceLocation loc = {});
    IROperand createParam(int index, IRType type, SourceLocation loc = {});
    IROperand createSpecial(SpecialKind kind, SourceLocation loc = {});

    // Integer Binary
    IROperand createAdd(IROperand a, IROperand b, SourceLocation loc = {});
    IROperand createSub(IROperand a, IROperand b, SourceLocation loc = {});
    IROperand createMul(IROperand a, IROperand b, SourceLocation loc = {});
    IROperand createDiv(IROperand a, IROperand b, SourceLocation loc = {});
    IROperand createRem(IROperand a, IROperand b, SourceLocation loc = {});
    IROperand createAnd(IROperand a, IROperand b, SourceLocation loc = {});
    IROperand createOr(IROperand a, IROperand b, SourceLocation loc = {});
    IROperand createXor(IROperand a, IROperand b, SourceLocation loc = {});
    IROperand createShl(IROperand a, IROperand b, SourceLocation loc = {});
    IROperand createShr(IROperand a, IROperand b, SourceLocation loc = {});

    // Float Binary
    IROperand createFAdd(IROperand a, IROperand b, SourceLocation loc = {});
    IROperand createFSub(IROperand a, IROperand b, SourceLocation loc = {});
    IROperand createFMul(IROperand a, IROperand b, SourceLocation loc = {});
    IROperand createFDiv(IROperand a, IROperand b, SourceLocation loc = {});

    // Unary
    IROperand createNeg(IROperand a, SourceLocation loc = {});
    IROperand createFNeg(IROperand a, SourceLocation loc = {});
    IROperand createNot(IROperand a, SourceLocation loc = {});

    // Comparison
    IROperand createCmp(Opcode cmpOp, IROperand a, IROperand b, SourceLocation loc = {});

    // Type Conversion
    IROperand createCvtI32ToF32(IROperand a, SourceLocation loc = {});
    IROperand createCvtF32ToI32(IROperand a, SourceLocation loc = {});

    // Memory
    IROperand createElemAddr(IROperand base, IROperand idx, SourceLocation loc = {});
    IROperand createLoad(IROperand addr, IRType elemType, SourceLocation loc = {});
    void createStore(IROperand addr, IROperand val, SourceLocation loc = {});

    // Terminators
    void createBr(BasicBlock* target, SourceLocation loc = {});
    void createCondBr(IROperand pred, BasicBlock* thenTarget, BasicBlock* elseTarget, SourceLocation loc = {});
    void createRet(SourceLocation loc = {});

private:
    void emit(std::unique_ptr<Instruction> inst);
    IROperand nextReg(IRType type);

    Function* function_ = nullptr;
    BasicBlock* insertBlock_ = nullptr;
};

} // namespace ptxlab::ir
