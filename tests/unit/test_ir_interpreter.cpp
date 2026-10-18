#include <gtest/gtest.h>
#include <climits>
#include "ptxlab/ir/ir_interpreter.h"
#include "ptxlab/ir/lower.h"
#include "ptxlab/kc/ast_interpreter.h"
#include "ptxlab/kc/lexer.h"
#include "ptxlab/kc/parser.h"
#include "ptxlab/kc/sema.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

using namespace ptxlab;
using namespace ptxlab::ir;

class IRInterpreterTest : public ::testing::Test {
protected:
    std::pair<std::unique_ptr<Program>, std::unique_ptr<Module>> compileProgram(std::string_view source) {
        sm.setSource("test.kc", std::string(source));
        diag.clear();
        Lexer lexer(sm, diag);
        Parser parser(lexer, sm, diag);
        auto program = parser.parseProgram();
        if (diag.hasErrors()) {
            ADD_FAILURE() << "Parsing failed";
            return {nullptr, nullptr};
        }

        Sema sema(sm, diag);
        if (!sema.analyze(*program)) {
            ADD_FAILURE() << "Semantic analysis failed";
            return {nullptr, nullptr};
        }

        Lowering lowerer;
        auto mod = lowerer.lower(*program);
        return {std::move(program), std::move(mod)};
    }

    SourceManager sm;
    DiagnosticEngine diag;
};

// ===========================================================================
// Positive Execution Tests & Differential Testing (O1 == O2)
// ===========================================================================

TEST_F(IRInterpreterTest, DifferentialSaxpyO1EqualsO2) {
    auto [astProg, irMod] = compileProgram(R"(
        kernel saxpy(i32 n, f32 a, f32* x, f32* y) {
            i32 tid = ctaid_x() * ntid_x() + tid_x();
            if (tid < n) {
                y[tid] = a * x[tid] + y[tid];
            }
        }
    )");
    ASSERT_NE(astProg, nullptr);
    ASSERT_NE(irMod, nullptr);

    // Initial input data
    std::vector<float> xData = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    std::vector<float> yData = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f};

    // 1. Run Oracle O1 (AST Interpreter)
    BufferManager buffersO1;
    buffersO1.allocateF32("x", xData);
    buffersO1.allocateF32("y", yData);
    std::unordered_map<std::string, KernelArg> argsO1 = {
        {"n", KernelArg::fromI32(8)},
        {"a", KernelArg::fromF32(2.5f)},
        {"x", KernelArg::fromBuffer("x")},
        {"y", KernelArg::fromBuffer("y")},
    };
    ASTInterpreter o1(buffersO1);
    o1.launch1D(*astProg->kernels()[0], 2, 4, argsO1);

    // 2. Run Oracle O2 (IR Interpreter)
    BufferManager buffersO2;
    buffersO2.allocateF32("x", xData);
    buffersO2.allocateF32("y", yData);
    std::unordered_map<std::string, KernelArg> argsO2 = {
        {"n", KernelArg::fromI32(8)},
        {"a", KernelArg::fromF32(2.5f)},
        {"x", KernelArg::fromBuffer("x")},
        {"y", KernelArg::fromBuffer("y")},
    };
    IRInterpreter o2(buffersO2);
    o2.launch1D(*irMod->functions()[0], 2, 4, argsO2);

    // 3. Differential Parity Check: O1 == O2
    const auto& resO1 = buffersO1.getF32("y");
    const auto& resO2 = buffersO2.getF32("y");
    ASSERT_EQ(resO1.size(), resO2.size());
    for (size_t i = 0; i < resO1.size(); ++i) {
        EXPECT_FLOAT_EQ(resO1[i], resO2[i]) << "Mismatch at index " << i;
    }
}

TEST_F(IRInterpreterTest, LoopsAndArithmeticInIR) {
    auto [astProg, irMod] = compileProgram(R"(
        kernel loop_ir(i32* out) {
            i32 sum = 0;
            for (i32 i = 1; i <= 5; i = i + 1) {
                sum = sum + i;
            }
            out[0] = sum; // 1 + 2 + 3 + 4 + 5 = 15
        }
    )");
    ASSERT_NE(irMod, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(1, 0));
    std::unordered_map<std::string, KernelArg> args = {
        {"out", KernelArg::fromBuffer("out")}
    };

    IRInterpreter o2(buffers);
    o2.launch1D(*irMod->functions()[0], 1, 1, args);

    EXPECT_EQ(buffers.getI32("out")[0], 15);
}

// ===========================================================================
// UB Trap Tests in IR (O2)
// ===========================================================================

TEST_F(IRInterpreterTest, TrapIntegerDivisionByZeroInIR) {
    auto [astProg, irMod] = compileProgram(R"(
        kernel trap_div_ir(i32* out) {
            i32 z = 0;
            out[0] = 100 / z;
        }
    )");
    ASSERT_NE(irMod, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(1, 0));
    std::unordered_map<std::string, KernelArg> args = {{"out", KernelArg::fromBuffer("out")}};

    IRInterpreter o2(buffers);
    EXPECT_THROW({
        try {
            o2.launch1D(*irMod->functions()[0], 1, 1, args);
        } catch (const UBTrap& trap) {
            EXPECT_NE(trap.message().find("division by zero"), std::string::npos);
            throw;
        }
    }, UBTrap);
}

TEST_F(IRInterpreterTest, TrapSignedOverflowInIR) {
    auto [astProg, irMod] = compileProgram(R"(
        kernel trap_overflow_ir(i32* out) {
            i32 min_val = -2147483648;
            i32 neg_one = -1;
            out[0] = min_val / neg_one;
        }
    )");
    ASSERT_NE(irMod, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(1, 0));
    std::unordered_map<std::string, KernelArg> args = {{"out", KernelArg::fromBuffer("out")}};

    IRInterpreter o2(buffers);
    EXPECT_THROW({
        try {
            o2.launch1D(*irMod->functions()[0], 1, 1, args);
        } catch (const UBTrap& trap) {
            EXPECT_NE(trap.message().find("signed integer division overflow"), std::string::npos);
            throw;
        }
    }, UBTrap);
}

TEST_F(IRInterpreterTest, TrapShiftCountOutOfBoundsInIR) {
    auto [astProg, irMod] = compileProgram(R"(
        kernel trap_shift_ir(i32* out) {
            i32 shift = 32;
            out[0] = 1 << shift;
        }
    )");
    ASSERT_NE(irMod, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(1, 0));
    std::unordered_map<std::string, KernelArg> args = {{"out", KernelArg::fromBuffer("out")}};

    IRInterpreter o2(buffers);
    EXPECT_THROW({
        try {
            o2.launch1D(*irMod->functions()[0], 1, 1, args);
        } catch (const UBTrap& trap) {
            EXPECT_NE(trap.message().find("shift count out of range"), std::string::npos);
            throw;
        }
    }, UBTrap);
}

TEST_F(IRInterpreterTest, TrapBufferOutOfBoundsInIR) {
    auto [astProg, irMod] = compileProgram(R"(
        kernel trap_oob_ir(i32* out) {
            out[10] = 999;
        }
    )");
    ASSERT_NE(irMod, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(4, 0)); // Size 4, index 10
    std::unordered_map<std::string, KernelArg> args = {{"out", KernelArg::fromBuffer("out")}};

    IRInterpreter o2(buffers);
    EXPECT_THROW({
        try {
            o2.launch1D(*irMod->functions()[0], 1, 1, args);
        } catch (const UBTrap& trap) {
            EXPECT_NE(trap.message().find("out-of-bounds buffer access"), std::string::npos);
            throw;
        }
    }, UBTrap);
}
