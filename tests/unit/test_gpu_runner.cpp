#include <gtest/gtest.h>
#include <vector>
#include "ptxlab/gpu/gpu_runner.h"
#include "ptxlab/ir/ir_interpreter.h"
#include "ptxlab/ir/lower.h"
#include "ptxlab/kc/ast_interpreter.h"
#include "ptxlab/kc/lexer.h"
#include "ptxlab/kc/parser.h"
#include "ptxlab/kc/sema.h"
#include "ptxlab/ptx/ptx_emitter.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

using namespace ptxlab;
using namespace ptxlab::ir;
using namespace ptxlab::ptx;
using namespace ptxlab::gpu;

class GPURunnerTest : public ::testing::Test {
protected:
    void SetUp() override {
        gpuAvailable_ = runner_.init(&initError_);
    }

    struct CompiledProgram {
        std::unique_ptr<Program> ast;
        std::unique_ptr<Module> ir;
        std::string ptx;
    };

    CompiledProgram compile(std::string_view source) {
        sm.setSource("test.kc", std::string(source));
        diag.clear();
        Lexer lexer(sm, diag);
        Parser parser(lexer, sm, diag);
        auto program = parser.parseProgram();
        if (diag.hasErrors() || !program) {
            ADD_FAILURE() << "Parsing failed";
            return {};
        }

        Sema sema(sm, diag);
        if (!sema.analyze(*program)) {
            ADD_FAILURE() << "Semantic analysis failed";
            return {};
        }

        Lowering lowerer;
        auto irMod = lowerer.lower(*program);
        if (!irMod) {
            ADD_FAILURE() << "Lowering failed";
            return {};
        }

        PTXEmitter emitter;
        std::string ptx = emitter.emit(*irMod);

        return {std::move(program), std::move(irMod), std::move(ptx)};
    }

    GPURunner runner_;
    bool gpuAvailable_ = false;
    std::string initError_;
    SourceManager sm;
    DiagnosticEngine diag;
};

// ===========================================================================
// Test 1: Device Discovery and Driver Query
// ===========================================================================

TEST_F(GPURunnerTest, DeviceDiscovery) {
    if (!gpuAvailable_) {
        GTEST_SKIP() << "CUDA GPU driver not available: " << initError_;
    }

    EXPECT_TRUE(runner_.isAvailable());
    const auto& dev = runner_.deviceInfo();
    EXPECT_FALSE(dev.name.empty());
    EXPECT_GE(dev.computeMajor, 5); // At least Maxwell or newer
    EXPECT_GT(dev.totalMemBytes, 0u);
    EXPECT_GT(dev.smCount, 0);

    std::cout << "[INFO] Detected GPU: " << dev.name
              << " | Compute: sm_" << dev.computeMajor << dev.computeMinor
              << " | SMs: " << dev.smCount
              << " | VRAM: " << (dev.totalMemBytes / (1024 * 1024)) << " MB\n";
}

// ===========================================================================
// Test 2: Saxpy Kernel Execution on GPU Hardware
// ===========================================================================

TEST_F(GPURunnerTest, SaxpyGPUExecution) {
    if (!gpuAvailable_) {
        GTEST_SKIP() << "CUDA GPU driver not available: " << initError_;
    }

    auto comp = compile(R"(
        kernel saxpy(i32 n, f32 a, f32* x, f32* y) {
            i32 tid = ctaid_x() * ntid_x() + tid_x();
            if (tid < n) {
                y[tid] = a * x[tid] + y[tid];
            }
        }
    )");
    ASSERT_NE(comp.ast, nullptr);
    ASSERT_FALSE(comp.ptx.empty());

    const int N = 8;
    std::vector<float> xData = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    std::vector<float> yData = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f};
    float a = 2.5f;

    BufferManager buffers;
    buffers.allocateF32("x", xData);
    buffers.allocateF32("y", yData);

    std::unordered_map<std::string, KernelArg> args = {
        {"n", KernelArg::fromI32(N)},
        {"a", KernelArg::fromF32(a)},
        {"x", KernelArg::fromBuffer("x")},
        {"y", KernelArg::fromBuffer("y")},
    };

    const auto& irFunc = *comp.ir->functions()[0];
    auto result = runner_.launch(comp.ptx, irFunc.name(), 2, 4, irFunc.params(), buffers, args);

    ASSERT_TRUE(result.success) << "GPU Launch failed: " << result.error;
    EXPECT_GT(result.jitTimeMs, 0.0);

    const auto& resY = buffers.getF32("y");
    for (int i = 0; i < N; ++i) {
        float expected = a * xData[static_cast<size_t>(i)] + yData[static_cast<size_t>(i)];
        EXPECT_FLOAT_EQ(resY[static_cast<size_t>(i)], expected)
            << "Mismatch at index " << i;
    }
}

// ===========================================================================
// Test 3: 3-Way Differential Testing: O1 (AST) == O2 (IR) == O4 (GPU)
// ===========================================================================

TEST_F(GPURunnerTest, DifferentialTestingO1_O2_O4) {
    if (!gpuAvailable_) {
        GTEST_SKIP() << "CUDA GPU driver not available: " << initError_;
    }

    auto comp = compile(R"(
        kernel saxpy(i32 n, f32 a, f32* x, f32* y) {
            i32 tid = ctaid_x() * ntid_x() + tid_x();
            if (tid < n) {
                y[tid] = a * x[tid] + y[tid];
            }
        }
    )");
    ASSERT_NE(comp.ast, nullptr);

    const int N = 16;
    std::vector<float> xData(N);
    std::vector<float> yData(N);
    for (int i = 0; i < N; ++i) {
        xData[static_cast<size_t>(i)] = static_cast<float>(i + 1) * 1.5f;
        yData[static_cast<size_t>(i)] = static_cast<float>(i + 1) * 10.0f;
    }
    float a = 3.25f;

    // 1. Oracle O1: AST Interpreter
    BufferManager buffersO1;
    buffersO1.allocateF32("x", xData);
    buffersO1.allocateF32("y", yData);
    std::unordered_map<std::string, KernelArg> argsO1 = {
        {"n", KernelArg::fromI32(N)},
        {"a", KernelArg::fromF32(a)},
        {"x", KernelArg::fromBuffer("x")},
        {"y", KernelArg::fromBuffer("y")},
    };
    ASTInterpreter o1(buffersO1);
    o1.launch1D(*comp.ast->kernels()[0], 4, 4, argsO1);

    // 2. Oracle O2: IR Interpreter
    BufferManager buffersO2;
    buffersO2.allocateF32("x", xData);
    buffersO2.allocateF32("y", yData);
    std::unordered_map<std::string, KernelArg> argsO2 = {
        {"n", KernelArg::fromI32(N)},
        {"a", KernelArg::fromF32(a)},
        {"x", KernelArg::fromBuffer("x")},
        {"y", KernelArg::fromBuffer("y")},
    };
    IRInterpreter o2(buffersO2);
    o2.launch1D(*comp.ir->functions()[0], 4, 4, argsO2);

    // 3. Oracle O4: Real GPU Hardware
    BufferManager buffersO4;
    buffersO4.allocateF32("x", xData);
    buffersO4.allocateF32("y", yData);
    std::unordered_map<std::string, KernelArg> argsO4 = {
        {"n", KernelArg::fromI32(N)},
        {"a", KernelArg::fromF32(a)},
        {"x", KernelArg::fromBuffer("x")},
        {"y", KernelArg::fromBuffer("y")},
    };
    const auto& irFunc = *comp.ir->functions()[0];
    auto resO4 = runner_.launch(comp.ptx, irFunc.name(), 4, 4, irFunc.params(), buffersO4, argsO4);
    ASSERT_TRUE(resO4.success) << "GPU launch failed: " << resO4.error;

    // 4. Parity Assertions
    const auto& outO1 = buffersO1.getF32("y");
    const auto& outO2 = buffersO2.getF32("y");
    const auto& outO4 = buffersO4.getF32("y");

    EXPECT_EQ(outO1, outO2) << "O1 (AST) != O2 (IR)";
    EXPECT_EQ(outO2, outO4) << "O2 (IR) != O4 (GPU)";
}

// ===========================================================================
// Test 4: Integer Vector Math on GPU Hardware
// ===========================================================================

TEST_F(GPURunnerTest, IntegerVectorMathGPU) {
    if (!gpuAvailable_) {
        GTEST_SKIP() << "CUDA GPU driver not available: " << initError_;
    }

    auto comp = compile(R"(
        kernel intVec(i32 n, i32* data) {
            i32 tid = ctaid_x() * ntid_x() + tid_x();
            if (tid < n) {
                data[tid] = data[tid] * 3 + 7;
            }
        }
    )");
    ASSERT_NE(comp.ast, nullptr);

    const int N = 32;
    std::vector<int32_t> initData(N);
    for (int i = 0; i < N; ++i) {
        initData[static_cast<size_t>(i)] = i;
    }

    BufferManager buffers;
    buffers.allocateI32("data", initData);

    std::unordered_map<std::string, KernelArg> args = {
        {"n", KernelArg::fromI32(N)},
        {"data", KernelArg::fromBuffer("data")},
    };

    const auto& irFunc = *comp.ir->functions()[0];
    auto result = runner_.launch(comp.ptx, irFunc.name(), 2, 16, irFunc.params(), buffers, args);
    ASSERT_TRUE(result.success) << "GPU launch failed: " << result.error;

    const auto& res = buffers.getI32("data");
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(res[static_cast<size_t>(i)], i * 3 + 7)
            << "Mismatch at index " << i;
    }
}

// ===========================================================================
// Test 5: Multi-Block Grid Launch (Scaling test)
// ===========================================================================

TEST_F(GPURunnerTest, MultiBlockGridScaling) {
    if (!gpuAvailable_) {
        GTEST_SKIP() << "CUDA GPU driver not available: " << initError_;
    }

    auto comp = compile(R"(
        kernel scaleGrid(i32 n, i32* out) {
            i32 tid = ctaid_x() * ntid_x() + tid_x();
            if (tid < n) {
                out[tid] = tid * 2;
            }
        }
    )");
    ASSERT_NE(comp.ast, nullptr);

    const int N = 256;
    std::vector<int32_t> initData(N, 0);

    BufferManager buffers;
    buffers.allocateI32("out", initData);

    std::unordered_map<std::string, KernelArg> args = {
        {"n", KernelArg::fromI32(N)},
        {"out", KernelArg::fromBuffer("out")},
    };

    const auto& irFunc = *comp.ir->functions()[0];
    // 4 blocks x 64 threads = 256 threads
    auto result = runner_.launch(comp.ptx, irFunc.name(), 4, 64, irFunc.params(), buffers, args);
    ASSERT_TRUE(result.success) << "GPU launch failed: " << result.error;

    const auto& res = buffers.getI32("out");
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(res[static_cast<size_t>(i)], i * 2);
    }
}
