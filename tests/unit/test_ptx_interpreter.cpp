#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <vector>
#include "ptxlab/gpu/gpu_runner.h"
#include "ptxlab/ir/ir_interpreter.h"
#include "ptxlab/ir/lower.h"
#include "ptxlab/kc/ast_interpreter.h"
#include "ptxlab/kc/lexer.h"
#include "ptxlab/kc/parser.h"
#include "ptxlab/kc/sema.h"
#include "ptxlab/ptx/ptx_emitter.h"
#include "ptxlab/ptx/ptx_interpreter.h"
#include "ptxlab/ptx/ptx_parser.h"
#include "ptxlab/ptx/ptx_validator.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

using namespace ptxlab;
using namespace ptxlab::ir;
using namespace ptxlab::ptx;
using namespace ptxlab::gpu;

class PTXInterpreterTest : public ::testing::Test {
protected:
    struct CompiledArtifacts {
        std::unique_ptr<Program> ast;
        std::unique_ptr<Module> ir;
        std::string ptx;
    };

    CompiledArtifacts compileToPTX(std::string_view source) {
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

    std::string readFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            file.open("../" + path);
        }
        if (!file.is_open()) return "";
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    SourceManager sm;
    DiagnosticEngine diag;
};

// ===========================================================================
// Test 1: PTX Parser Directives and Entry Header
// ===========================================================================

TEST_F(PTXInterpreterTest, ParseDirectivesAndHeader) {
    std::string ptx = R"(
        .version 7.0
        .target sm_75
        .address_size 64

        .visible .entry myKernel(
            .param .u32 _param_n,
            .param .f32 _param_val,
            .param .u64 _param_buf
        )
        {
            .reg .b32 %r<8>;
            .reg .f32 %f<4>;
            .reg .b64 %rd<4>;
            ret;
        }
    )";

    std::string err;
    auto module = PTXParser::parseString(ptx, &err);
    ASSERT_NE(module, nullptr) << err;
    EXPECT_EQ(module->version(), "7.0");
    EXPECT_EQ(module->target(), "sm_75");
    EXPECT_EQ(module->addressSize(), 64);

    ASSERT_EQ(module->kernels().size(), 1u);
    const auto& kernel = *module->kernels()[0];
    EXPECT_EQ(kernel.name(), "myKernel");
    ASSERT_EQ(kernel.params().size(), 3u);
    EXPECT_EQ(kernel.params()[0].name, "_param_n");
    EXPECT_EQ(kernel.params()[0].type, PTXType::U32);
    EXPECT_EQ(kernel.params()[1].name, "_param_val");
    EXPECT_EQ(kernel.params()[1].type, PTXType::F32);
    EXPECT_EQ(kernel.params()[2].name, "_param_buf");
    EXPECT_EQ(kernel.params()[2].type, PTXType::U64);

    ASSERT_EQ(kernel.regDecls().size(), 3u);
    EXPECT_EQ(kernel.regDecls()[0].nameOrPrefix, "%r");
    EXPECT_EQ(kernel.regDecls()[0].count, 8);
}

// ===========================================================================
// Test 2: Hex Float Literal Parsing
// ===========================================================================

TEST_F(PTXInterpreterTest, ParseHexFloatLiteral) {
    // 0f3F800000 is 1.0f in IEEE 754
    // 0f40000000 is 2.0f in IEEE 754
    std::string ptx = R"(
        .version 7.0
        .target sm_75
        .address_size 64

        .visible .entry hexKernel() {
            .reg .f32 %f<2>;
            mov.f32 %f0, 0f3F800000;
            mov.f32 %f1, 0f40000000;
            ret;
        }
    )";

    std::string err;
    auto module = PTXParser::parseString(ptx, &err);
    ASSERT_NE(module, nullptr) << err;

    PTXValidator validator;
    EXPECT_TRUE(validator.validate(*module));
}

// ===========================================================================
// Test 3: Corpus Valid PTX Verification
// ===========================================================================

TEST_F(PTXInterpreterTest, CorpusValidPTX) {
    std::vector<std::string> validFiles = {
        "tests/ptx_corpus/valid/smoke.ptx",
        "tests/ptx_corpus/valid/saxpy.ptx",
        "tests/ptx_corpus/valid/int_math.ptx",
    };

    for (const auto& path : validFiles) {
        std::string content = readFile(path);
        ASSERT_FALSE(content.empty()) << "Could not read " << path;

        std::string parseErr;
        auto mod = PTXParser::parseString(content, &parseErr);
        ASSERT_NE(mod, nullptr) << "Failed to parse " << path << ": " << parseErr;

        PTXValidator validator;
        EXPECT_TRUE(validator.validate(*mod)) << "Failed to validate " << path;
    }
}

// ===========================================================================
// Test 4: Corpus Invalid PTX Rejections
// ===========================================================================

TEST_F(PTXInterpreterTest, CorpusInvalidMissingVersion) {
    std::string content = readFile("tests/ptx_corpus/invalid/missing_version.ptx");
    ASSERT_FALSE(content.empty());

    auto mod = PTXParser::parseString(content);
    ASSERT_NE(mod, nullptr);
    PTXValidator validator;
    EXPECT_FALSE(validator.validate(*mod));
    EXPECT_FALSE(validator.errors().empty());
}

TEST_F(PTXInterpreterTest, CorpusInvalidUndeclaredReg) {
    std::string content = readFile("tests/ptx_corpus/invalid/undeclared_reg.ptx");
    ASSERT_FALSE(content.empty());

    auto mod = PTXParser::parseString(content);
    ASSERT_NE(mod, nullptr);
    PTXValidator validator;
    EXPECT_FALSE(validator.validate(*mod));
    EXPECT_FALSE(validator.errors().empty());
}

TEST_F(PTXInterpreterTest, CorpusInvalidTypeMismatch) {
    std::string content = readFile("tests/ptx_corpus/invalid/type_mismatch.ptx");
    ASSERT_FALSE(content.empty());

    auto mod = PTXParser::parseString(content);
    ASSERT_NE(mod, nullptr);
    PTXValidator validator;
    EXPECT_FALSE(validator.validate(*mod));
    EXPECT_FALSE(validator.errors().empty());
}

TEST_F(PTXInterpreterTest, CorpusInvalidDuplicateLabel) {
    std::string content = readFile("tests/ptx_corpus/invalid/duplicate_label.ptx");
    ASSERT_FALSE(content.empty());

    auto mod = PTXParser::parseString(content);
    ASSERT_NE(mod, nullptr);
    PTXValidator validator;
    EXPECT_FALSE(validator.validate(*mod));
    EXPECT_FALSE(validator.errors().empty());
}

TEST_F(PTXInterpreterTest, CorpusInvalidUndefinedLabel) {
    std::string content = readFile("tests/ptx_corpus/invalid/undefined_label.ptx");
    ASSERT_FALSE(content.empty());

    auto mod = PTXParser::parseString(content);
    ASSERT_NE(mod, nullptr);
    PTXValidator validator;
    EXPECT_FALSE(validator.validate(*mod));
    EXPECT_FALSE(validator.errors().empty());
}

TEST_F(PTXInterpreterTest, CorpusInvalidMemoryBase) {
    std::string content = readFile("tests/ptx_corpus/invalid/invalid_memory_base.ptx");
    ASSERT_FALSE(content.empty());

    auto mod = PTXParser::parseString(content);
    ASSERT_NE(mod, nullptr);
    PTXValidator validator;
    EXPECT_FALSE(validator.validate(*mod));
    EXPECT_FALSE(validator.errors().empty());
}

// ===========================================================================
// Test 5: PTX Interpreter Basic Execution (Smoke)
// ===========================================================================

TEST_F(PTXInterpreterTest, ExecuteSmokeKernel) {
    std::string ptx = readFile("tests/ptx_corpus/valid/smoke.ptx");
    BufferManager buffers;
    std::unordered_map<std::string, KernelArg> args;

    PTXInterpreter interpreter(buffers);
    auto result = interpreter.launch(ptx, "smoke", 1, 1, args);
    EXPECT_TRUE(result.success) << result.error;
}

// ===========================================================================
// Test 6: PTX Interpreter Integer Arithmetic & Bitwise
// ===========================================================================

TEST_F(PTXInterpreterTest, ExecuteIntegerMathKernel) {
    std::string ptx = readFile("tests/ptx_corpus/valid/int_math.ptx");
    BufferManager buffers;
    const int N = 16;
    buffers.allocateI32("out", std::vector<int32_t>(N, 0));

    std::unordered_map<std::string, KernelArg> args;
    args["_param_n"] = KernelArg::fromI32(N);
    args["_param_out"] = KernelArg::fromBuffer("out");

    PTXInterpreter interpreter(buffers);
    auto result = interpreter.launch(ptx, "int_math", 1, N, args);
    ASSERT_TRUE(result.success) << result.error;

    const auto& outBuf = buffers.getI32("out");
    for (int tid = 0; tid < N; ++tid) {
        // Calculation from int_math.ptx:
        // r2 = tid + 10
        // r3 = r2 - 2 = tid + 8
        // r4 = r3 * 3 = (tid + 8) * 3
        // r5 = r4 << 1 = ((tid + 8) * 3) * 2
        // r6 = r5 ^ 7
        int expected = (((tid + 8) * 3) << 1) ^ 7;
        EXPECT_EQ(outBuf[static_cast<size_t>(tid)], expected) << "Mismatch at thread " << tid;
    }
}

// ===========================================================================
// Test 7: PTX Interpreter Division by Zero Trap
// ===========================================================================

TEST_F(PTXInterpreterTest, TrapIntegerDivisionByZero) {
    std::string ptx = R"(
        .version 7.0
        .target sm_75
        .address_size 64

        .visible .entry divZero() {
            .reg .b32 %r<3>;
            mov.u32 %r0, 10;
            mov.u32 %r1, 0;
            div.s32 %r2, %r0, %r1;
            ret;
        }
    )";

    BufferManager buffers;
    std::unordered_map<std::string, KernelArg> args;

    PTXInterpreter interpreter(buffers);
    auto result = interpreter.launch(ptx, "divZero", 1, 1, args);
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error.find("division by zero"), std::string::npos);
}

// ===========================================================================
// Test 8: 4-Way Differential Parity: O1 == O2 == O3 (== O4 if GPU present)
// ===========================================================================

TEST_F(PTXInterpreterTest, DifferentialSaxpyAllOracles) {
    auto comp = compileToPTX(R"(
        kernel saxpy(i32 n, f32 a, f32* x, f32* y) {
            i32 idx = ctaid_x() * ntid_x() + tid_x();
            if (idx < n) {
                y[idx] = a * x[idx] + y[idx];
            }
        }
    )");

    ASSERT_NE(comp.ast, nullptr);
    ASSERT_NE(comp.ir, nullptr);
    ASSERT_FALSE(comp.ptx.empty());

    const int N = 64;
    const float alpha = 3.5f;

    std::vector<float> xInit(N);
    std::vector<float> yInit(N);
    for (int i = 0; i < N; ++i) {
        xInit[static_cast<size_t>(i)] = static_cast<float>(i + 1) * 1.5f;
        yInit[static_cast<size_t>(i)] = static_cast<float>(i + 1) * 0.5f;
    }

    std::unordered_map<std::string, KernelArg> args;
    args["n"] = KernelArg::fromI32(N);
    args["a"] = KernelArg::fromF32(alpha);
    args["x"] = KernelArg::fromBuffer("x");
    args["y"] = KernelArg::fromBuffer("y");

    int gridDim = 2;
    int blockDim = 32;

    // --- Oracle 1: AST Interpreter ---
    BufferManager bmAst;
    bmAst.allocateF32("x", xInit);
    bmAst.allocateF32("y", yInit);
    ASTInterpreter o1(bmAst);
    o1.launch1D(*comp.ast->kernels()[0], gridDim, blockDim, args);
    const auto& resO1 = bmAst.getF32("y");

    // --- Oracle 2: IR Interpreter ---
    BufferManager bmIr;
    bmIr.allocateF32("x", xInit);
    bmIr.allocateF32("y", yInit);
    IRInterpreter o2(bmIr);
    o2.launch1D(*comp.ir->functions()[0], gridDim, blockDim, args);
    const auto& resO2 = bmIr.getF32("y");

    // --- Oracle 3: PTX Reference Interpreter ---
    BufferManager bmPtx;
    bmPtx.allocateF32("x", xInit);
    bmPtx.allocateF32("y", yInit);
    PTXInterpreter o3(bmPtx);
    auto ptxRes = o3.launch(comp.ptx, "saxpy", gridDim, blockDim, args);
    ASSERT_TRUE(ptxRes.success) << ptxRes.error;
    const auto& resO3 = bmPtx.getF32("y");

    // Check bit-exact parity O1 == O2 == O3
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(resO1[i], resO2[i]) << "O1 vs O2 mismatch at index " << i;
        EXPECT_EQ(resO2[i], resO3[i]) << "O2 vs O3 mismatch at index " << i;
    }

    // --- Oracle 4: GPU Runner (if available) ---
    GPURunner gpuRunner;
    std::string gpuErr;
    if (gpuRunner.init(&gpuErr)) {
        BufferManager bmGpu;
        bmGpu.allocateF32("x", xInit);
        bmGpu.allocateF32("y", yInit);
        const auto& func = *comp.ir->functions()[0];
        auto gpuRes = gpuRunner.launch(comp.ptx, "saxpy", gridDim, blockDim, func.params(), bmGpu, args);
        ASSERT_TRUE(gpuRes.success) << gpuRes.error;
        const auto& resO4 = bmGpu.getF32("y");

        for (size_t i = 0; i < N; ++i) {
            EXPECT_EQ(resO3[i], resO4[i]) << "O3 vs O4 mismatch at index " << i;
        }
        std::cout << "[INFO] Verified 4-Way Differential Parity: O1 == O2 == O3 == O4 (bit-for-bit identical across all 64 elements)!\n";
    } else {
        std::cout << "[INFO] Verified 3-Way Differential Parity: O1 == O2 == O3 (GPU skipped: " << gpuErr << ")\n";
    }
}

// ===========================================================================
// Test 9: Differential Testing on Integer Vector Kernel
// ===========================================================================

TEST_F(PTXInterpreterTest, DifferentialIntVectorAllOracles) {
    auto comp = compileToPTX(R"(
        kernel int_vec(i32 n, i32* a, i32* b, i32* c) {
            i32 idx = ctaid_x() * ntid_x() + tid_x();
            if (idx < n) {
                c[idx] = (a[idx] * 2 + b[idx] - 3) ^ 5;
            }
        }
    )");

    ASSERT_NE(comp.ast, nullptr);
    ASSERT_NE(comp.ir, nullptr);
    ASSERT_FALSE(comp.ptx.empty());

    const int N = 64;
    std::vector<int32_t> aInit(N);
    std::vector<int32_t> bInit(N);
    std::vector<int32_t> cInit(N, 0);
    for (int i = 0; i < N; ++i) {
        aInit[static_cast<size_t>(i)] = (i + 1) * 3;
        bInit[static_cast<size_t>(i)] = (i + 1) * 7;
    }

    std::unordered_map<std::string, KernelArg> args;
    args["n"] = KernelArg::fromI32(N);
    args["a"] = KernelArg::fromBuffer("a");
    args["b"] = KernelArg::fromBuffer("b");
    args["c"] = KernelArg::fromBuffer("c");

    int gridDim = 4;
    int blockDim = 16;

    // O1
    BufferManager bmA;
    bmA.allocateI32("a", aInit);
    bmA.allocateI32("b", bInit);
    bmA.allocateI32("c", cInit);
    ASTInterpreter o1(bmA);
    o1.launch1D(*comp.ast->kernels()[0], gridDim, blockDim, args);
    const auto& resO1 = bmA.getI32("c");

    // O2
    BufferManager bmB;
    bmB.allocateI32("a", aInit);
    bmB.allocateI32("b", bInit);
    bmB.allocateI32("c", cInit);
    IRInterpreter o2(bmB);
    o2.launch1D(*comp.ir->functions()[0], gridDim, blockDim, args);
    const auto& resO2 = bmB.getI32("c");

    // O3
    BufferManager bmC;
    bmC.allocateI32("a", aInit);
    bmC.allocateI32("b", bInit);
    bmC.allocateI32("c", cInit);
    PTXInterpreter o3(bmC);
    auto ptxRes = o3.launch(comp.ptx, "int_vec", gridDim, blockDim, args);
    ASSERT_TRUE(ptxRes.success) << ptxRes.error;
    const auto& resO3 = bmC.getI32("c");

    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(resO1[i], resO2[i]);
        EXPECT_EQ(resO2[i], resO3[i]);
    }

    // O4
    GPURunner gpuRunner;
    if (gpuRunner.init()) {
        BufferManager bmD;
        bmD.allocateI32("a", aInit);
        bmD.allocateI32("b", bInit);
        bmD.allocateI32("c", cInit);
        const auto& func = *comp.ir->functions()[0];
        auto gpuRes = gpuRunner.launch(comp.ptx, "int_vec", gridDim, blockDim, func.params(), bmD, args);
        ASSERT_TRUE(gpuRes.success) << gpuRes.error;
        const auto& resO4 = bmD.getI32("c");

        for (size_t i = 0; i < N; ++i) {
            EXPECT_EQ(resO3[i], resO4[i]);
        }
    }
}
