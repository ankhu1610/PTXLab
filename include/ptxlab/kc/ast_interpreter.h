#pragma once

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "ptxlab/kc/ast.h"
#include "ptxlab/support/source_location.h"

namespace ptxlab {

class UBTrap : public std::runtime_error {
public:
    explicit UBTrap(std::string message, SourceLocation loc = {})
        : std::runtime_error(message), message_(std::move(message)), loc_(loc) {}

    [[nodiscard]] const std::string& message() const noexcept { return message_; }
    [[nodiscard]] SourceLocation location() const noexcept { return loc_; }

private:
    std::string message_;
    SourceLocation loc_;
};

struct Value {
    enum class Kind {
        I32,
        F32,
        Bool,
        Ptr
    } kind = Kind::I32;

    int32_t i32 = 0;
    float f32 = 0.0f;
    bool b = false;
    std::string ptrBuffer;

    [[nodiscard]] static Value makeI32(int32_t v) {
        Value val; val.kind = Kind::I32; val.i32 = v; return val;
    }
    [[nodiscard]] static Value makeF32(float v) {
        Value val; val.kind = Kind::F32; val.f32 = v; return val;
    }
    [[nodiscard]] static Value makeBool(bool v) {
        Value val; val.kind = Kind::Bool; val.b = v; return val;
    }
    [[nodiscard]] static Value makePtr(std::string bufferName) {
        Value val; val.kind = Kind::Ptr; val.ptrBuffer = std::move(bufferName); return val;
    }
};

class BufferManager {
public:
    void allocateI32(const std::string& name, std::vector<int32_t> data) {
        i32Buffers_[name] = std::move(data);
    }
    void allocateF32(const std::string& name, std::vector<float> data) {
        f32Buffers_[name] = std::move(data);
    }

    [[nodiscard]] std::vector<int32_t>& getI32(const std::string& name) {
        auto it = i32Buffers_.find(name);
        if (it == i32Buffers_.end()) {
            throw UBTrap("unknown i32 buffer '" + name + "'");
        }
        return it->second;
    }

    [[nodiscard]] const std::vector<int32_t>& getI32(const std::string& name) const {
        auto it = i32Buffers_.find(name);
        if (it == i32Buffers_.end()) {
            throw UBTrap("unknown i32 buffer '" + name + "'");
        }
        return it->second;
    }

    [[nodiscard]] std::vector<float>& getF32(const std::string& name) {
        auto it = f32Buffers_.find(name);
        if (it == f32Buffers_.end()) {
            throw UBTrap("unknown f32 buffer '" + name + "'");
        }
        return it->second;
    }

    [[nodiscard]] const std::vector<float>& getF32(const std::string& name) const {
        auto it = f32Buffers_.find(name);
        if (it == f32Buffers_.end()) {
            throw UBTrap("unknown f32 buffer '" + name + "'");
        }
        return it->second;
    }

    [[nodiscard]] int32_t readI32(const std::string& name, int32_t index, SourceLocation loc = {}) {
        auto& buf = getI32(name);
        if (index < 0 || static_cast<size_t>(index) >= buf.size()) {
            throw UBTrap("out-of-bounds buffer access on '" + name + "' with index " +
                            std::to_string(index) + " (size " + std::to_string(buf.size()) + ")", loc);
        }
        return buf[static_cast<size_t>(index)];
    }

    void writeI32(const std::string& name, int32_t index, int32_t value, SourceLocation loc = {}) {
        auto& buf = getI32(name);
        if (index < 0 || static_cast<size_t>(index) >= buf.size()) {
            throw UBTrap("out-of-bounds buffer access on '" + name + "' with index " +
                            std::to_string(index) + " (size " + std::to_string(buf.size()) + ")", loc);
        }
        buf[static_cast<size_t>(index)] = value;
    }

    [[nodiscard]] float readF32(const std::string& name, int32_t index, SourceLocation loc = {}) {
        auto& buf = getF32(name);
        if (index < 0 || static_cast<size_t>(index) >= buf.size()) {
            throw UBTrap("out-of-bounds buffer access on '" + name + "' with index " +
                            std::to_string(index) + " (size " + std::to_string(buf.size()) + ")", loc);
        }
        return buf[static_cast<size_t>(index)];
    }

    void writeF32(const std::string& name, int32_t index, float value, SourceLocation loc = {}) {
        auto& buf = getF32(name);
        if (index < 0 || static_cast<size_t>(index) >= buf.size()) {
            throw UBTrap("out-of-bounds buffer access on '" + name + "' with index " +
                            std::to_string(index) + " (size " + std::to_string(buf.size()) + ")", loc);
        }
        buf[static_cast<size_t>(index)] = value;
    }

private:
    std::unordered_map<std::string, std::vector<int32_t>> i32Buffers_;
    std::unordered_map<std::string, std::vector<float>> f32Buffers_;
};

struct KernelArg {
    enum class Kind { I32, F32, Bool, Buffer } kind;
    int32_t i32Val = 0;
    float f32Val = 0.0f;
    bool boolVal = false;
    std::string bufferName;

    static KernelArg fromI32(int32_t val) {
        KernelArg a; a.kind = Kind::I32; a.i32Val = val; return a;
    }
    static KernelArg fromF32(float val) {
        KernelArg a; a.kind = Kind::F32; a.f32Val = val; return a;
    }
    static KernelArg fromBool(bool val) {
        KernelArg a; a.kind = Kind::Bool; a.boolVal = val; return a;
    }
    static KernelArg fromBuffer(std::string name) {
        KernelArg a; a.kind = Kind::Buffer; a.bufferName = std::move(name); return a;
    }
};

class ASTInterpreter : public ASTVisitor {
public:
    explicit ASTInterpreter(BufferManager& buffers, uint64_t maxSteps = 1'000'000);

    /// Launches a 1D grid over the specified kernel
    void launch1D(KernelDecl& kernel,
                  int gridDim,
                  int blockDim,
                  const std::unordered_map<std::string, KernelArg>& args);

    // Expressions
    void visit(IntLiteralExpr& node) override;
    void visit(FloatLiteralExpr& node) override;
    void visit(BoolLiteralExpr& node) override;
    void visit(IdentifierExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(BinaryExpr& node) override;
    void visit(IndexExpr& node) override;
    void visit(CallExpr& node) override;

    // Statements
    void visit(CompoundStmt& node) override;
    void visit(DeclStmt& node) override;
    void visit(AssignStmt& node) override;
    void visit(IfStmt& node) override;
    void visit(WhileStmt& node) override;
    void visit(ForStmt& node) override;
    void visit(ReturnStmt& node) override;

    // Declarations
    void visit(ParamDecl& node) override;
    void visit(KernelDecl& node) override;
    void visit(Program& node) override;

    [[nodiscard]] Value evaluate(Expr& expr);
    void execute(Stmt& stmt);

private:
    void enterScope();
    void exitScope();
    void setVar(const std::string& name, Value val);
    void assignVar(const std::string& name, Value val, SourceLocation loc);
    [[nodiscard]] Value getVar(const std::string& name, SourceLocation loc) const;
    void step(SourceLocation loc);

    BufferManager& buffers_;
    uint64_t maxSteps_;
    uint64_t stepCount_ = 0;

    int32_t tid_x_ = 0;
    int32_t ctaid_x_ = 0;
    int32_t ntid_x_ = 1;

    bool hasReturned_ = false;
    Value lastExprVal_;
    std::vector<std::unordered_map<std::string, Value>> scopes_;
};

} // namespace ptxlab
