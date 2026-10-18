#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "ptxlab/support/source_location.h"

namespace ptxlab::ir {

enum class IRTypeKind {
    I32,
    F32,
    Pred,
    PtrI32,
    PtrF32,
    Void
};

struct IRType {
    IRTypeKind kind = IRTypeKind::Void;

    [[nodiscard]] constexpr bool isInt() const noexcept { return kind == IRTypeKind::I32; }
    [[nodiscard]] constexpr bool isFloat() const noexcept { return kind == IRTypeKind::F32; }
    [[nodiscard]] constexpr bool isPred() const noexcept { return kind == IRTypeKind::Pred; }
    [[nodiscard]] constexpr bool isPtr() const noexcept { return kind == IRTypeKind::PtrI32 || kind == IRTypeKind::PtrF32; }
    [[nodiscard]] constexpr bool isPtrI32() const noexcept { return kind == IRTypeKind::PtrI32; }
    [[nodiscard]] constexpr bool isPtrF32() const noexcept { return kind == IRTypeKind::PtrF32; }
    [[nodiscard]] constexpr bool isVoid() const noexcept { return kind == IRTypeKind::Void; }

    [[nodiscard]] constexpr bool operator==(const IRType& other) const noexcept = default;

    [[nodiscard]] static constexpr IRType i32() noexcept { return IRType{IRTypeKind::I32}; }
    [[nodiscard]] static constexpr IRType f32() noexcept { return IRType{IRTypeKind::F32}; }
    [[nodiscard]] static constexpr IRType pred() noexcept { return IRType{IRTypeKind::Pred}; }
    [[nodiscard]] static constexpr IRType ptrI32() noexcept { return IRType{IRTypeKind::PtrI32}; }
    [[nodiscard]] static constexpr IRType ptrF32() noexcept { return IRType{IRTypeKind::PtrF32}; }
    [[nodiscard]] static constexpr IRType voidType() noexcept { return IRType{IRTypeKind::Void}; }

    [[nodiscard]] std::string toString() const {
        switch (kind) {
            case IRTypeKind::I32:    return "i32";
            case IRTypeKind::F32:    return "f32";
            case IRTypeKind::Pred:   return "pred";
            case IRTypeKind::PtrI32: return "ptr_i32";
            case IRTypeKind::PtrF32: return "ptr_f32";
            case IRTypeKind::Void:   return "void";
        }
        return "unknown";
    }
};

struct IROperand {
    enum class Kind {
        Register,
        ConstantInt,
        ConstantFloat,
        ConstantBool
    } kind = Kind::Register;

    int regId = -1;
    IRType type = IRType::i32();
    int32_t intVal = 0;
    float floatVal = 0.0f;
    bool boolVal = false;

    [[nodiscard]] static IROperand reg(int id, IRType t) {
        IROperand op;
        op.kind = Kind::Register;
        op.regId = id;
        op.type = t;
        return op;
    }

    [[nodiscard]] static IROperand constInt(int32_t val) {
        IROperand op;
        op.kind = Kind::ConstantInt;
        op.type = IRType::i32();
        op.intVal = val;
        return op;
    }

    [[nodiscard]] static IROperand constFloat(float val) {
        IROperand op;
        op.kind = Kind::ConstantFloat;
        op.type = IRType::f32();
        op.floatVal = val;
        return op;
    }

    [[nodiscard]] static IROperand constBool(bool val) {
        IROperand op;
        op.kind = Kind::ConstantBool;
        op.type = IRType::pred();
        op.boolVal = val;
        return op;
    }

    [[nodiscard]] bool isReg() const noexcept { return kind == Kind::Register; }
    [[nodiscard]] bool isConst() const noexcept { return kind != Kind::Register; }

    [[nodiscard]] std::string toString() const {
        if (kind == Kind::Register) {
            if (type.isPred()) {
                return "%p" + std::to_string(regId);
            }
            return "%v" + std::to_string(regId);
        }
        if (kind == Kind::ConstantInt) {
            return std::to_string(intVal);
        }
        if (kind == Kind::ConstantFloat) {
            std::string s = std::to_string(floatVal);
            // Format clean float
            if (s.find('.') == std::string::npos) {
                s += ".0";
            }
            return s + "f";
        }
        return boolVal ? "true" : "false";
    }

    [[nodiscard]] bool operator==(const IROperand& other) const = default;
};

enum class Opcode {
    // Constants, Copies, Parameters, Intrinsics
    Const,
    Copy,
    Param,
    Special,

    // Integer Binary
    Add,
    Sub,
    Mul,
    Div,
    Rem,
    And,
    Or,
    Xor,
    Shl,
    Shr,

    // Floating Binary
    FAdd,
    FSub,
    FMul,
    FDiv,

    // Unary
    Neg,
    FNeg,
    Not,

    // Comparison
    CmpEq,
    CmpNe,
    CmpLt,
    CmpLe,
    CmpGt,
    CmpGe,

    // Conversions
    CvtI32ToF32,
    CvtF32ToI32,

    // Memory
    ElemAddr,
    Load,
    Store,

    // Terminators
    Br,
    CondBr,
    Ret
};

enum class SpecialKind {
    TidX,
    CtaidX,
    NtidX
};

class Instruction {
public:
    Instruction(Opcode op, SourceLocation loc = {})
        : opcode_(op), loc_(loc) {}

    [[nodiscard]] Opcode opcode() const noexcept { return opcode_; }
    [[nodiscard]] SourceLocation location() const noexcept { return loc_; }

    [[nodiscard]] bool hasDst() const noexcept { return dst_.has_value(); }
    [[nodiscard]] const IROperand& dst() const { return *dst_; }
    void setDst(IROperand dst) { dst_ = dst; }

    [[nodiscard]] const std::vector<IROperand>& operands() const noexcept { return operands_; }
    void addOperand(IROperand op) { operands_.push_back(op); }

    [[nodiscard]] SpecialKind specialKind() const noexcept { return specialKind_; }
    void setSpecialKind(SpecialKind sk) noexcept { specialKind_ = sk; }

    [[nodiscard]] const std::string& targetTrue() const noexcept { return targetTrue_; }
    [[nodiscard]] const std::string& targetFalse() const noexcept { return targetFalse_; }
    void setBranchTargets(std::string trueTarget, std::string falseTarget = "") {
        targetTrue_ = std::move(trueTarget);
        targetFalse_ = std::move(falseTarget);
    }

    [[nodiscard]] bool isTerminator() const noexcept {
        return opcode_ == Opcode::Br || opcode_ == Opcode::CondBr || opcode_ == Opcode::Ret;
    }

    [[nodiscard]] std::string toString() const;

private:
    Opcode opcode_;
    SourceLocation loc_;
    std::optional<IROperand> dst_;
    std::vector<IROperand> operands_;
    SpecialKind specialKind_ = SpecialKind::TidX;
    std::string targetTrue_;
    std::string targetFalse_;
};

} // namespace ptxlab::ir
