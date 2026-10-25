#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "ptxlab/kc/ast_interpreter.h"
#include "ptxlab/ptx/ptx_ast.h"

namespace ptxlab::ptx {

struct PTXExecResult {
    bool success{false};
    std::string error;
    uint64_t totalCycles{0};
};

class PTXInterpreter {
public:
    explicit PTXInterpreter(BufferManager& buffers, uint64_t maxCyclesPerThread = 1'000'000);

    PTXExecResult launch(const PTXKernel& kernel,
                         int gridDim,
                         int blockDim,
                         const std::unordered_map<std::string, KernelArg>& args);

    PTXExecResult launch(const PTXModule& module,
                         std::string_view kernelName,
                         int gridDim,
                         int blockDim,
                         const std::unordered_map<std::string, KernelArg>& args);

    PTXExecResult launch(std::string_view ptxSource,
                         std::string_view kernelName,
                         int gridDim,
                         int blockDim,
                         const std::unordered_map<std::string, KernelArg>& args);

private:
    struct BufferRegion {
        std::string name;
        uint64_t baseAddr{0};
        uint64_t sizeBytes{0};
        bool isFloat{false};
    };

    struct ThreadContext {
        std::unordered_map<std::string, uint32_t> rRegs;
        std::unordered_map<std::string, float> fRegs;
        std::unordered_map<std::string, uint64_t> rdRegs;
        std::unordered_map<std::string, bool> pRegs;

        int32_t tidX{0};
        int32_t ctaidX{0};
        int32_t ntidX{1};
    };

    void setupBufferMemory(const std::unordered_map<std::string, KernelArg>& args);
    const BufferRegion* findBufferRegion(uint64_t addr) const;

    void executeThread(const PTXKernel& kernel,
                       int ctaid,
                       int tid,
                       int blockDim,
                       const std::vector<PTXInstruction>& instructions,
                       const std::unordered_map<std::string, size_t>& labelIndices,
                       const std::unordered_map<std::string, KernelArg>& args,
                       uint64_t& threadCycles);

    uint32_t readU32(const ThreadContext& ctx, const PTXOperand& op) const;
    int32_t readS32(const ThreadContext& ctx, const PTXOperand& op) const;
    float readF32(const ThreadContext& ctx, const PTXOperand& op) const;
    uint64_t readU64(const ThreadContext& ctx, const PTXOperand& op) const;
    bool readPred(const ThreadContext& ctx, const PTXOperand& op) const;

    void writeU32(ThreadContext& ctx, const PTXOperand& op, uint32_t val);
    void writeS32(ThreadContext& ctx, const PTXOperand& op, int32_t val);
    void writeF32(ThreadContext& ctx, const PTXOperand& op, float val);
    void writeU64(ThreadContext& ctx, const PTXOperand& op, uint64_t val);
    void writePred(ThreadContext& ctx, const PTXOperand& op, bool val);

    BufferManager& buffers_;
    uint64_t maxCyclesPerThread_{1'000'000};
    std::vector<BufferRegion> memoryMap_;
};

} // namespace ptxlab::ptx
