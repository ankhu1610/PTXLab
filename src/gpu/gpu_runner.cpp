#include "ptxlab/gpu/gpu_runner.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

// ============================================================================
// Windows dynamic loading (we avoid linking to cuda.lib / nvcuda.lib entirely)
// ============================================================================
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// ============================================================================
// Minimal CUDA Driver API type definitions (from cuda.h, inlined here so we
// never need the CUDA Toolkit SDK headers to compile)
// ============================================================================
using CUresult   = int;
using CUdevice   = int;
using CUcontext  = void*;
using CUmodule   = void*;
using CUfunction = void*;
using CUdeviceptr = unsigned long long;

constexpr int CUDA_SUCCESS = 0;
constexpr int CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT = 16;
constexpr int CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR = 75;
constexpr int CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR = 76;

// Signature typedefs for every CUDA Driver API function we use
using PFN_cuInit              = CUresult(*)(unsigned int);
using PFN_cuDeviceGet         = CUresult(*)(CUdevice*, int);
using PFN_cuDeviceGetCount    = CUresult(*)(int*);
using PFN_cuDeviceGetName     = CUresult(*)(char*, int, CUdevice);
using PFN_cuDeviceGetAttribute= CUresult(*)(int*, int, CUdevice);
using PFN_cuDeviceTotalMem    = CUresult(*)(size_t*, CUdevice);
using PFN_cuCtxCreate         = CUresult(*)(CUcontext*, unsigned int, CUdevice);
using PFN_cuCtxDestroy        = CUresult(*)(CUcontext);
using PFN_cuModuleLoadDataEx  = CUresult(*)(CUmodule*, const void*, unsigned int, void*, void*);
using PFN_cuModuleUnload      = CUresult(*)(CUmodule);
using PFN_cuModuleGetFunction = CUresult(*)(CUfunction*, CUmodule, const char*);
using PFN_cuMemAlloc          = CUresult(*)(CUdeviceptr*, size_t);
using PFN_cuMemFree           = CUresult(*)(CUdeviceptr);
using PFN_cuMemcpyHtoD        = CUresult(*)(CUdeviceptr, const void*, size_t);
using PFN_cuMemcpyDtoH        = CUresult(*)(void*, CUdeviceptr, size_t);
using PFN_cuLaunchKernel      = CUresult(*)(CUfunction, unsigned, unsigned, unsigned,
                                            unsigned, unsigned, unsigned,
                                            unsigned, void*, void**, void**);
using PFN_cuCtxSynchronize    = CUresult(*)();

namespace ptxlab::gpu {

// ============================================================================
// Helper: CUDA error code to string
// ============================================================================
static std::string cudaErrorStr(CUresult err) {
    return "CUDA error code " + std::to_string(err);
}

// ============================================================================
// Destructor: clean up CUDA context
// ============================================================================
GPURunner::~GPURunner() {
    if (context_ && fn_.cuCtxDestroy) {
        auto cuCtxDestroy = reinterpret_cast<PFN_cuCtxDestroy>(fn_.cuCtxDestroy);
        cuCtxDestroy(static_cast<CUcontext>(context_));
        context_ = nullptr;
    }
#ifdef _WIN32
    if (dllHandle_) {
        FreeLibrary(static_cast<HMODULE>(dllHandle_));
        dllHandle_ = nullptr;
    }
#else
    if (dllHandle_) {
        dlclose(dllHandle_);
        dllHandle_ = nullptr;
    }
#endif
}

// ============================================================================
// Dynamic library loading
// ============================================================================
bool GPURunner::loadLibrary() {
#ifdef _WIN32
    HMODULE h = LoadLibraryA("nvcuda.dll");
    if (!h) return false;
    dllHandle_ = static_cast<void*>(h);
    return true;
#else
    void* h = dlopen("libcuda.so.1", RTLD_LAZY);
    if (!h) h = dlopen("libcuda.so", RTLD_LAZY);
    if (!h) return false;
    dllHandle_ = h;
    return true;
#endif
}

bool GPURunner::loadFunctions() {
    auto getProc = [this](const char* name) -> void* {
#ifdef _WIN32
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(dllHandle_), name));
#else
        return dlsym(dllHandle_, name);
#endif
    };

    // Try versioned symbols first (CUDA 11+), then unversioned
    auto getFunc = [&](const char* name, const char* versioned) -> void* {
        void* p = versioned ? getProc(versioned) : nullptr;
        if (!p) p = getProc(name);
        return p;
    };

    fn_.cuInit              = getFunc("cuInit", nullptr);
    fn_.cuDeviceGet         = getFunc("cuDeviceGet", nullptr);
    fn_.cuDeviceGetCount    = getFunc("cuDeviceGetCount", nullptr);
    fn_.cuDeviceGetName     = getFunc("cuDeviceGetName", nullptr);
    fn_.cuDeviceGetAttribute= getFunc("cuDeviceGetAttribute", nullptr);
    fn_.cuDeviceTotalMem    = getFunc("cuDeviceTotalMem", "cuDeviceTotalMem_v2");
    fn_.cuCtxCreate         = getFunc("cuCtxCreate", "cuCtxCreate_v2");
    fn_.cuCtxDestroy        = getFunc("cuCtxDestroy", "cuCtxDestroy_v2");
    fn_.cuModuleLoadDataEx  = getFunc("cuModuleLoadDataEx", nullptr);
    fn_.cuModuleUnload      = getFunc("cuModuleUnload", nullptr);
    fn_.cuModuleGetFunction = getFunc("cuModuleGetFunction", nullptr);
    fn_.cuMemAlloc          = getFunc("cuMemAlloc", "cuMemAlloc_v2");
    fn_.cuMemFree           = getFunc("cuMemFree", "cuMemFree_v2");
    fn_.cuMemcpyHtoD        = getFunc("cuMemcpyHtoD", "cuMemcpyHtoD_v2");
    fn_.cuMemcpyDtoH        = getFunc("cuMemcpyDtoH", "cuMemcpyDtoH_v2");
    fn_.cuLaunchKernel      = getFunc("cuLaunchKernel", nullptr);
    fn_.cuCtxSynchronize    = getFunc("cuCtxSynchronize", nullptr);

    // Check critical functions
    return fn_.cuInit && fn_.cuDeviceGet && fn_.cuDeviceGetCount &&
           fn_.cuCtxCreate && fn_.cuModuleLoadDataEx && fn_.cuModuleGetFunction &&
           fn_.cuMemAlloc && fn_.cuMemFree && fn_.cuMemcpyHtoD && fn_.cuMemcpyDtoH &&
           fn_.cuLaunchKernel && fn_.cuCtxSynchronize;
}

// ============================================================================
// Initialization: load DLL, init CUDA, create context, query device info
// ============================================================================
bool GPURunner::init(std::string* errorOut) {
    if (available_) return true;

    auto fail = [&](const std::string& msg) -> bool {
        if (errorOut) *errorOut = msg;
        return false;
    };

    // Step 1: Load the shared library
    if (!loadLibrary()) {
        return fail("could not load NVIDIA CUDA driver library (nvcuda.dll / libcuda.so)");
    }

    // Step 2: Resolve function pointers
    if (!loadFunctions()) {
        return fail("could not resolve required CUDA Driver API functions");
    }

    // Step 3: cuInit
    auto cuInit = reinterpret_cast<PFN_cuInit>(fn_.cuInit);
    CUresult err = cuInit(0);
    if (err != CUDA_SUCCESS) {
        return fail("cuInit failed: " + cudaErrorStr(err));
    }

    // Step 4: Check device count
    auto cuDeviceGetCount = reinterpret_cast<PFN_cuDeviceGetCount>(fn_.cuDeviceGetCount);
    int count = 0;
    err = cuDeviceGetCount(&count);
    if (err != CUDA_SUCCESS || count == 0) {
        return fail("no CUDA-capable GPU devices found");
    }

    // Step 5: Get device 0
    auto cuDeviceGet = reinterpret_cast<PFN_cuDeviceGet>(fn_.cuDeviceGet);
    CUdevice dev = 0;
    err = cuDeviceGet(&dev, 0);
    if (err != CUDA_SUCCESS) {
        return fail("cuDeviceGet failed: " + cudaErrorStr(err));
    }
    device_ = dev;

    // Step 6: Query device info
    char devName[256] = {};
    if (fn_.cuDeviceGetName) {
        auto cuDeviceGetName = reinterpret_cast<PFN_cuDeviceGetName>(fn_.cuDeviceGetName);
        cuDeviceGetName(devName, sizeof(devName), dev);
    }
    deviceInfo_.name = devName;

    if (fn_.cuDeviceGetAttribute) {
        auto cuDeviceGetAttribute = reinterpret_cast<PFN_cuDeviceGetAttribute>(fn_.cuDeviceGetAttribute);
        cuDeviceGetAttribute(&deviceInfo_.computeMajor,
                             CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, dev);
        cuDeviceGetAttribute(&deviceInfo_.computeMinor,
                             CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, dev);
        cuDeviceGetAttribute(&deviceInfo_.smCount,
                             CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, dev);
    }

    if (fn_.cuDeviceTotalMem) {
        auto cuDeviceTotalMem = reinterpret_cast<PFN_cuDeviceTotalMem>(fn_.cuDeviceTotalMem);
        cuDeviceTotalMem(&deviceInfo_.totalMemBytes, dev);
    }

    // Step 7: Create context
    auto cuCtxCreate = reinterpret_cast<PFN_cuCtxCreate>(fn_.cuCtxCreate);
    CUcontext ctx = nullptr;
    err = cuCtxCreate(&ctx, 0, dev);
    if (err != CUDA_SUCCESS) {
        return fail("cuCtxCreate failed: " + cudaErrorStr(err));
    }
    context_ = ctx;

    available_ = true;
    return true;
}

// ============================================================================
// Kernel launch: JIT compile PTX, allocate GPU memory, execute, read back
// ============================================================================
GPULaunchResult GPURunner::launch(
    const std::string& ptxSource,
    const std::string& kernelName,
    int gridDim,
    int blockDim,
    const std::vector<ir::IRParam>& params,
    BufferManager& buffers,
    const std::unordered_map<std::string, KernelArg>& args)
{
    GPULaunchResult result;
    result.device = deviceInfo_;

    if (!available_) {
        result.error = "GPU not initialized";
        return result;
    }

    // Typed function pointers
    auto cuModuleLoadDataEx  = reinterpret_cast<PFN_cuModuleLoadDataEx>(fn_.cuModuleLoadDataEx);
    auto cuModuleUnload      = reinterpret_cast<PFN_cuModuleUnload>(fn_.cuModuleUnload);
    auto cuModuleGetFunction = reinterpret_cast<PFN_cuModuleGetFunction>(fn_.cuModuleGetFunction);
    auto cuMemAlloc          = reinterpret_cast<PFN_cuMemAlloc>(fn_.cuMemAlloc);
    auto cuMemFree           = reinterpret_cast<PFN_cuMemFree>(fn_.cuMemFree);
    auto cuMemcpyHtoD        = reinterpret_cast<PFN_cuMemcpyHtoD>(fn_.cuMemcpyHtoD);
    auto cuMemcpyDtoH        = reinterpret_cast<PFN_cuMemcpyDtoH>(fn_.cuMemcpyDtoH);
    auto cuLaunchKernel      = reinterpret_cast<PFN_cuLaunchKernel>(fn_.cuLaunchKernel);
    auto cuCtxSynchronize    = reinterpret_cast<PFN_cuCtxSynchronize>(fn_.cuCtxSynchronize);

    // ---- Step 1: JIT-compile PTX ----
    CUmodule mod = nullptr;
    auto jitStart = std::chrono::high_resolution_clock::now();
    CUresult err = cuModuleLoadDataEx(&mod, ptxSource.c_str(), 0, nullptr, nullptr);
    auto jitEnd = std::chrono::high_resolution_clock::now();
    result.jitTimeMs = std::chrono::duration<double, std::milli>(jitEnd - jitStart).count();

    if (err != CUDA_SUCCESS) {
        result.error = "cuModuleLoadDataEx (JIT compile) failed: " + cudaErrorStr(err);
        return result;
    }

    // ---- Step 2: Get kernel function handle ----
    CUfunction func = nullptr;
    err = cuModuleGetFunction(&func, mod, kernelName.c_str());
    if (err != CUDA_SUCCESS) {
        result.error = "cuModuleGetFunction('" + kernelName + "') failed: " + cudaErrorStr(err);
        cuModuleUnload(mod);
        return result;
    }

    // ---- Step 3: Allocate GPU memory and build kernel argument list ----
    // CUDA Driver API passes kernel arguments as void* array pointing to each arg's storage.
    // We must keep storage alive until after launch + sync.
    struct GPUBuffer {
        CUdeviceptr devPtr = 0;
        size_t bytes = 0;
        std::string name;
        bool isFloat = false;
    };

    std::vector<GPUBuffer> gpuBuffers;
    std::vector<void*> kernelParams;

    // Persistent storage for scalar values passed by pointer
    std::vector<int32_t>    i32Storage(params.size());
    std::vector<float>      f32Storage(params.size());
    std::vector<CUdeviceptr> ptrStorage(params.size());

    for (size_t i = 0; i < params.size(); ++i) {
        const auto& p = params[i];
        auto argIt = args.find(p.name);
        if (argIt == args.end()) {
            result.error = "missing argument for parameter '" + p.name + "'";
            // Free already allocated GPU memory
            for (auto& gb : gpuBuffers) cuMemFree(gb.devPtr);
            cuModuleUnload(mod);
            return result;
        }
        const auto& arg = argIt->second;

        if (p.type.isPtr()) {
            // Allocate device memory and copy host buffer to device
            bool isFloat = p.type.isPtrF32();
            size_t elemSize = 4; // Both i32 and f32 are 4 bytes
            size_t numElems = 0;
            const void* hostData = nullptr;

            if (isFloat) {
                auto& buf = buffers.getF32(arg.bufferName);
                numElems = buf.size();
                hostData = buf.data();
            } else {
                auto& buf = buffers.getI32(arg.bufferName);
                numElems = buf.size();
                hostData = buf.data();
            }

            size_t bytes = numElems * elemSize;
            CUdeviceptr dptr = 0;
            err = cuMemAlloc(&dptr, bytes);
            if (err != CUDA_SUCCESS) {
                result.error = "cuMemAlloc failed for '" + p.name + "': " + cudaErrorStr(err);
                for (auto& gb : gpuBuffers) cuMemFree(gb.devPtr);
                cuModuleUnload(mod);
                return result;
            }

            err = cuMemcpyHtoD(dptr, hostData, bytes);
            if (err != CUDA_SUCCESS) {
                result.error = "cuMemcpyHtoD failed for '" + p.name + "': " + cudaErrorStr(err);
                cuMemFree(dptr);
                for (auto& gb : gpuBuffers) cuMemFree(gb.devPtr);
                cuModuleUnload(mod);
                return result;
            }

            gpuBuffers.push_back({dptr, bytes, arg.bufferName, isFloat});
            ptrStorage[i] = dptr;
            kernelParams.push_back(&ptrStorage[i]);

        } else if (arg.kind == KernelArg::Kind::I32) {
            i32Storage[i] = arg.i32Val;
            kernelParams.push_back(&i32Storage[i]);

        } else if (arg.kind == KernelArg::Kind::F32) {
            f32Storage[i] = arg.f32Val;
            kernelParams.push_back(&f32Storage[i]);

        } else if (arg.kind == KernelArg::Kind::Bool) {
            // Pass bool as u32 (matching our PTX param ABI)
            i32Storage[i] = arg.boolVal ? 1 : 0;
            kernelParams.push_back(&i32Storage[i]);
        }
    }

    // ---- Step 4: Launch kernel ----
    auto launchStart = std::chrono::high_resolution_clock::now();
    err = cuLaunchKernel(
        func,
        static_cast<unsigned>(gridDim), 1, 1,  // grid dimensions
        static_cast<unsigned>(blockDim), 1, 1,  // block dimensions
        0,          // shared memory bytes
        nullptr,    // stream (0 = default)
        kernelParams.data(),
        nullptr     // extra params
    );
    if (err != CUDA_SUCCESS) {
        result.error = "cuLaunchKernel failed: " + cudaErrorStr(err);
        for (auto& gb : gpuBuffers) cuMemFree(gb.devPtr);
        cuModuleUnload(mod);
        return result;
    }

    // ---- Step 5: Synchronize ----
    err = cuCtxSynchronize();
    auto launchEnd = std::chrono::high_resolution_clock::now();
    result.launchTimeMs = std::chrono::duration<double, std::milli>(launchEnd - launchStart).count();

    if (err != CUDA_SUCCESS) {
        result.error = "cuCtxSynchronize failed (kernel may have trapped): " + cudaErrorStr(err);
        for (auto& gb : gpuBuffers) cuMemFree(gb.devPtr);
        cuModuleUnload(mod);
        return result;
    }

    // ---- Step 6: Copy results back to host ----
    for (auto& gb : gpuBuffers) {
        if (gb.isFloat) {
            auto& buf = buffers.getF32(gb.name);
            err = cuMemcpyDtoH(buf.data(), gb.devPtr, gb.bytes);
        } else {
            auto& buf = buffers.getI32(gb.name);
            err = cuMemcpyDtoH(buf.data(), gb.devPtr, gb.bytes);
        }
        if (err != CUDA_SUCCESS) {
            result.error = "cuMemcpyDtoH failed for '" + gb.name + "': " + cudaErrorStr(err);
            for (auto& g2 : gpuBuffers) cuMemFree(g2.devPtr);
            cuModuleUnload(mod);
            return result;
        }
    }

    // ---- Cleanup: free GPU allocations and unload module ----
    for (auto& gb : gpuBuffers) {
        cuMemFree(gb.devPtr);
    }
    cuModuleUnload(mod);

    result.success = true;
    return result;
}

} // namespace ptxlab::gpu
