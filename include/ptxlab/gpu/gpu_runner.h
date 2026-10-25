#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "ptxlab/ir/function.h"
#include "ptxlab/kc/ast_interpreter.h"  // BufferManager, KernelArg

namespace ptxlab::gpu {

/// Error information from the GPU runner.
struct GPUError {
    std::string message;
    explicit GPUError(std::string msg) : message(std::move(msg)) {}
};

/// GPU device info discovered at runtime.
struct GPUDeviceInfo {
    std::string name;
    int computeMajor = 0;
    int computeMinor = 0;
    size_t totalMemBytes = 0;
    int smCount = 0;
};

/// Result of a GPU kernel launch.
struct GPULaunchResult {
    bool success = false;
    std::string error;
    GPUDeviceInfo device;
    double jitTimeMs = 0.0;
    double launchTimeMs = 0.0;
};

/// Dynamically loads nvcuda.dll and executes PTX kernels on the GPU.
///
/// Lifecycle:
///   1. GPURunner runner;
///   2. if (!runner.init()) { /* no GPU available */ }
///   3. runner.launch(ptxString, kernelName, grid, block, params, buffers);
///   4. Read back buffers.
class GPURunner {
public:
    GPURunner() = default;
    ~GPURunner();

    // Non-copyable, non-movable (owns raw GPU context handles)
    GPURunner(const GPURunner&) = delete;
    GPURunner& operator=(const GPURunner&) = delete;

    /// Attempt to load nvcuda.dll and initialize a CUDA context.
    /// Returns false if no GPU is available (not an error, just graceful degradation).
    [[nodiscard]] bool init(std::string* errorOut = nullptr);

    /// Returns true if init() succeeded and the GPU is ready.
    [[nodiscard]] bool isAvailable() const noexcept { return available_; }

    /// Query device info (valid after init()).
    [[nodiscard]] const GPUDeviceInfo& deviceInfo() const noexcept { return deviceInfo_; }

    /// Launch a kernel from PTX source on the GPU.
    ///
    /// @param ptxSource   The full PTX module as a string.
    /// @param kernelName  The .entry name to launch.
    /// @param gridDim     Number of blocks.
    /// @param blockDim    Number of threads per block.
    /// @param params      Kernel parameter descriptors (from AST).
    /// @param buffers     BufferManager containing I/O data (read/written in place).
    /// @param args        Kernel argument values (scalars and buffer references).
    [[nodiscard]] GPULaunchResult launch(
        const std::string& ptxSource,
        const std::string& kernelName,
        int gridDim,
        int blockDim,
        const std::vector<ir::IRParam>& params,
        BufferManager& buffers,
        const std::unordered_map<std::string, KernelArg>& args);

private:
    bool available_ = false;
    GPUDeviceInfo deviceInfo_;

    // Opaque CUDA handles (stored as void* to avoid including cuda.h)
    void* dllHandle_ = nullptr;
    void* context_ = nullptr;   // CUcontext
    int device_ = 0;            // CUdevice (actually an int in the CUDA driver API)

    // Function pointers loaded from nvcuda.dll
    struct CUDAFunctions {
        void* cuInit = nullptr;
        void* cuDeviceGet = nullptr;
        void* cuDeviceGetCount = nullptr;
        void* cuDeviceGetName = nullptr;
        void* cuDeviceGetAttribute = nullptr;
        void* cuDeviceTotalMem = nullptr;
        void* cuCtxCreate = nullptr;
        void* cuCtxDestroy = nullptr;
        void* cuModuleLoadDataEx = nullptr;
        void* cuModuleUnload = nullptr;
        void* cuModuleGetFunction = nullptr;
        void* cuMemAlloc = nullptr;
        void* cuMemFree = nullptr;
        void* cuMemcpyHtoD = nullptr;
        void* cuMemcpyDtoH = nullptr;
        void* cuLaunchKernel = nullptr;
        void* cuCtxSynchronize = nullptr;
    } fn_;

    bool loadLibrary();
    bool loadFunctions();
};

} // namespace ptxlab::gpu
