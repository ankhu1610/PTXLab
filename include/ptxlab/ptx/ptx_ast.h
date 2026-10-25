#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ptxlab::ptx {

enum class PTXType {
    Unknown,
    U32,
    S32,
    B32,
    F32,
    U64,
    S64,
    B64,
    Pred,
};

inline std::string_view ptxTypeToString(PTXType type) {
    switch (type) {
        case PTXType::U32: return ".u32";
        case PTXType::S32: return ".s32";
        case PTXType::B32: return ".b32";
        case PTXType::F32: return ".f32";
        case PTXType::U64: return ".u64";
        case PTXType::S64: return ".s64";
        case PTXType::B64: return ".b64";
        case PTXType::Pred: return ".pred";
        default: return ".unknown";
    }
}

enum class PTXOperandKind {
    Register,
    ImmediateInt,
    ImmediateFloat,
    ImmediatePred,
    SpecialReg,
    MemoryRef,
    LabelRef,
};

enum class PTXSpecialReg {
    TidX,
    CtaidX,
    NtidX,
};

struct PTXOperand {
    PTXOperandKind kind{PTXOperandKind::Register};
    std::string regName;
    int64_t intVal{0};
    float floatVal{0.0f};
    bool boolVal{false};
    PTXSpecialReg specialReg{PTXSpecialReg::TidX};
    std::string memBase;
    int64_t memOffset{0};
    std::string labelName;

    static PTXOperand reg(std::string name) {
        PTXOperand op;
        op.kind = PTXOperandKind::Register;
        op.regName = std::move(name);
        return op;
    }

    static PTXOperand immInt(int64_t val) {
        PTXOperand op;
        op.kind = PTXOperandKind::ImmediateInt;
        op.intVal = val;
        return op;
    }

    static PTXOperand immFloat(float val) {
        PTXOperand op;
        op.kind = PTXOperandKind::ImmediateFloat;
        op.floatVal = val;
        return op;
    }

    static PTXOperand immPred(bool val) {
        PTXOperand op;
        op.kind = PTXOperandKind::ImmediatePred;
        op.boolVal = val;
        return op;
    }

    static PTXOperand special(PTXSpecialReg spec) {
        PTXOperand op;
        op.kind = PTXOperandKind::SpecialReg;
        op.specialReg = spec;
        return op;
    }

    static PTXOperand mem(std::string base, int64_t offset = 0) {
        PTXOperand op;
        op.kind = PTXOperandKind::MemoryRef;
        op.memBase = std::move(base);
        op.memOffset = offset;
        return op;
    }

    static PTXOperand label(std::string name) {
        PTXOperand op;
        op.kind = PTXOperandKind::LabelRef;
        op.labelName = std::move(name);
        return op;
    }
};

enum class PTXOpcode {
    Mov,
    LdParam,
    Cvta,
    Cvt,
    Add,
    Sub,
    Mul,
    MulWide,
    Div,
    Rem,
    And,
    Or,
    Xor,
    Shl,
    Shr,
    Neg,
    Not,
    Setp,
    LdGlobal,
    StGlobal,
    Bra,
    Ret,
};

enum class PTXCmpCondition {
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
};

enum class PTXRoundingMode {
    None,
    Rn,
    Rzi,
};

struct PTXPredicate {
    std::string regName;
    bool negated{false};
};

struct PTXInstruction {
    std::optional<PTXPredicate> predicate;
    PTXOpcode opcode{PTXOpcode::Mov};
    PTXType typeMod{PTXType::Unknown};
    PTXType srcTypeMod{PTXType::Unknown};
    PTXRoundingMode rounding{PTXRoundingMode::None};
    PTXCmpCondition cmpCond{PTXCmpCondition::Eq};
    std::vector<PTXOperand> operands;
    int line{0};
};

struct PTXStatement {
    enum class Kind {
        Instruction,
        Label,
    } kind{Kind::Instruction};

    std::string labelName;
    PTXInstruction inst;
    int line{0};

    static PTXStatement makeLabel(std::string name, int line = 0) {
        PTXStatement stmt;
        stmt.kind = Kind::Label;
        stmt.labelName = std::move(name);
        stmt.line = line;
        return stmt;
    }

    static PTXStatement makeInstruction(PTXInstruction inst, int line = 0) {
        PTXStatement stmt;
        stmt.kind = Kind::Instruction;
        stmt.inst = std::move(inst);
        stmt.line = line;
        return stmt;
    }
};

struct PTXParamDecl {
    std::string name;
    PTXType type{PTXType::Unknown};
    int line{0};
};

struct PTXRegDecl {
    PTXType type{PTXType::Unknown};
    std::string nameOrPrefix;
    int count{1};
    bool isRange{false};
    int line{0};
};

class PTXKernel {
public:
    explicit PTXKernel(std::string name) : name_(std::move(name)) {}

    const std::string& name() const { return name_; }

    void addParam(PTXParamDecl param) { params_.push_back(std::move(param)); }
    const std::vector<PTXParamDecl>& params() const { return params_; }

    void addRegDecl(PTXRegDecl regDecl) { regDecls_.push_back(std::move(regDecl)); }
    const std::vector<PTXRegDecl>& regDecls() const { return regDecls_; }

    void addStatement(PTXStatement stmt) { statements_.push_back(std::move(stmt)); }
    const std::vector<PTXStatement>& statements() const { return statements_; }

private:
    std::string name_;
    std::vector<PTXParamDecl> params_;
    std::vector<PTXRegDecl> regDecls_;
    std::vector<PTXStatement> statements_;
};

class PTXModule {
public:
    void setVersion(std::string ver) { version_ = std::move(ver); }
    const std::string& version() const { return version_; }

    void setTarget(std::string target) { target_ = std::move(target); }
    const std::string& target() const { return target_; }

    void setAddressSize(int size) { addressSize_ = size; }
    int addressSize() const { return addressSize_; }

    void addKernel(std::unique_ptr<PTXKernel> kernel) {
        kernels_.push_back(std::move(kernel));
    }

    const std::vector<std::unique_ptr<PTXKernel>>& kernels() const {
        return kernels_;
    }

    const PTXKernel* findKernel(std::string_view name) const {
        for (const auto& k : kernels_) {
            if (k->name() == name) return k.get();
        }
        return nullptr;
    }

private:
    std::string version_;
    std::string target_;
    int addressSize_{0};
    std::vector<std::unique_ptr<PTXKernel>> kernels_;
};

} // namespace ptxlab::ptx
