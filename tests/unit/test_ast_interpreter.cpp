#include <gtest/gtest.h>
#include <climits>
#include "ptxlab/kc/ast_interpreter.h"
#include "ptxlab/kc/lexer.h"
#include "ptxlab/kc/parser.h"
#include "ptxlab/kc/sema.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

using namespace ptxlab;

class ASTInterpreterTest : public ::testing::Test {
protected:
    std::unique_ptr<Program> parseAndValidate(std::string_view source) {
        sm.setSource("test.kc", std::string(source));
        diag.clear();
        Lexer lexer(sm, diag);
        Parser parser(lexer, sm, diag);
        auto program = parser.parseProgram();
        if (diag.hasErrors()) {
            ADD_FAILURE() << "Parsing failed";
            return nullptr;
        }

        Sema sema(sm, diag);
        if (!sema.analyze(*program)) {
            ADD_FAILURE() << "Semantic analysis failed";
            return nullptr;
        }

        return program;
    }

    SourceManager sm;
    DiagnosticEngine diag;
};

// ===========================================================================
// Positive Execution Tests
// ===========================================================================

TEST_F(ASTInterpreterTest, RealisticKernelSaxpy) {
    auto prog = parseAndValidate(R"(
        kernel saxpy(i32 n, f32 a, f32* x, f32* y) {
            i32 tid = ctaid_x() * ntid_x() + tid_x();
            if (tid < n) {
                y[tid] = a * x[tid] + y[tid];
            }
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    std::vector<float> xData = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    std::vector<float> yData = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f};
    buffers.allocateF32("x", xData);
    buffers.allocateF32("y", yData);

    std::unordered_map<std::string, KernelArg> args = {
        {"n", KernelArg::fromI32(8)},
        {"a", KernelArg::fromF32(2.5f)},
        {"x", KernelArg::fromBuffer("x")},
        {"y", KernelArg::fromBuffer("y")},
    };

    ASTInterpreter interp(buffers);
    // Launch 2 blocks of 4 threads = 8 total threads
    interp.launch1D(*prog->kernels()[0], 2, 4, args);

    const auto& resultY = buffers.getF32("y");
    ASSERT_EQ(resultY.size(), 8);
    EXPECT_FLOAT_EQ(resultY[0], 2.5f * 1.0f + 10.0f); // 12.5
    EXPECT_FLOAT_EQ(resultY[1], 2.5f * 2.0f + 20.0f); // 25.0
    EXPECT_FLOAT_EQ(resultY[2], 2.5f * 3.0f + 30.0f); // 37.5
    EXPECT_FLOAT_EQ(resultY[3], 2.5f * 4.0f + 40.0f); // 50.0
    EXPECT_FLOAT_EQ(resultY[4], 2.5f * 5.0f + 50.0f); // 62.5
    EXPECT_FLOAT_EQ(resultY[5], 2.5f * 6.0f + 60.0f); // 75.0
    EXPECT_FLOAT_EQ(resultY[6], 2.5f * 7.0f + 70.0f); // 87.5
    EXPECT_FLOAT_EQ(resultY[7], 2.5f * 8.0f + 80.0f); // 100.0
}

TEST_F(ASTInterpreterTest, ArithmeticAndTwoComplementWrapping) {
    auto prog = parseAndValidate(R"(
        kernel test_arith(i32* out) {
            i32 max_int = 2147483647;
            out[0] = max_int + 1; // Must wrap to INT_MIN (-2147483648)
            out[1] = 7 * 6;
            out[2] = 100 / 8;     // Truncates toward zero: 12
            out[3] = 100 % 8;     // 4
            out[4] = 10 - 25;     // -15
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(5, 0));

    std::unordered_map<std::string, KernelArg> args = {
        {"out", KernelArg::fromBuffer("out")}
    };

    ASTInterpreter interp(buffers);
    interp.launch1D(*prog->kernels()[0], 1, 1, args);

    const auto& res = buffers.getI32("out");
    EXPECT_EQ(res[0], INT32_MIN);
    EXPECT_EQ(res[1], 42);
    EXPECT_EQ(res[2], 12);
    EXPECT_EQ(res[3], 4);
    EXPECT_EQ(res[4], -15);
}

TEST_F(ASTInterpreterTest, BitwiseOperations) {
    auto prog = parseAndValidate(R"(
        kernel test_bits(i32* out) {
            i32 a = 0x0F;
            i32 b = 0x33;
            out[0] = a & b;  // 0x03
            out[1] = a | b;  // 0x3F
            out[2] = a ^ b;  // 0x3C
            out[3] = a << 2; // 0x3C
            out[4] = b >> 1; // 0x19
            out[5] = ~0;     // -1
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(6, 0));

    std::unordered_map<std::string, KernelArg> args = {
        {"out", KernelArg::fromBuffer("out")}
    };

    ASTInterpreter interp(buffers);
    interp.launch1D(*prog->kernels()[0], 1, 1, args);

    const auto& res = buffers.getI32("out");
    EXPECT_EQ(res[0], 0x03);
    EXPECT_EQ(res[1], 0x3F);
    EXPECT_EQ(res[2], 0x3C);
    EXPECT_EQ(res[3], 0x3C);
    EXPECT_EQ(res[4], 0x19);
    EXPECT_EQ(res[5], -1);
}

TEST_F(ASTInterpreterTest, LogicalShortCircuiting) {
    // Both lines have division by zero on the RHS, which must NOT be evaluated due to short-circuit!
    auto prog = parseAndValidate(R"(
        kernel test_sc(i32* out) {
            i32 zero = 0;
            bool and_ok = false && ((1 / zero) == 0);
            bool or_ok  = true || ((1 / zero) == 0);
            if (and_ok) {
                out[0] = 1;
            } else {
                out[0] = 0;
            }
            if (or_ok) {
                out[1] = 1;
            } else {
                out[1] = 0;
            }
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(2, 0));

    std::unordered_map<std::string, KernelArg> args = {
        {"out", KernelArg::fromBuffer("out")}
    };

    ASTInterpreter interp(buffers);
    EXPECT_NO_THROW(interp.launch1D(*prog->kernels()[0], 1, 1, args));

    const auto& res = buffers.getI32("out");
    EXPECT_EQ(res[0], 0);
    EXPECT_EQ(res[1], 1);
}

TEST_F(ASTInterpreterTest, LoopsAndEarlyReturn) {
    auto prog = parseAndValidate(R"(
        kernel test_loop(i32* out) {
            i32 sum = 0;
            for (i32 i = 0; i < 5; i = i + 1) {
                sum = sum + i;
            }
            out[0] = sum; // 0 + 1 + 2 + 3 + 4 = 10

            i32 count = 0;
            while (count < 3) {
                count = count + 1;
                if (count == 2) {
                    out[1] = 999;
                    return; // Early return
                }
            }
            out[1] = 123; // Should not be reached
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(2, 0));

    std::unordered_map<std::string, KernelArg> args = {
        {"out", KernelArg::fromBuffer("out")}
    };

    ASTInterpreter interp(buffers);
    interp.launch1D(*prog->kernels()[0], 1, 1, args);

    const auto& res = buffers.getI32("out");
    EXPECT_EQ(res[0], 10);
    EXPECT_EQ(res[1], 999);
}

// ===========================================================================
// UNDEFINED BEHAVIOR (UB) TRAP TESTS
// ===========================================================================

TEST_F(ASTInterpreterTest, TrapIntegerDivisionByZero) {
    auto prog = parseAndValidate(R"(
        kernel trap_div(i32* out) {
            i32 z = 0;
            out[0] = 42 / z;
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(1, 0));
    std::unordered_map<std::string, KernelArg> args = {{"out", KernelArg::fromBuffer("out")}};

    ASTInterpreter interp(buffers);
    EXPECT_THROW({
        try {
            interp.launch1D(*prog->kernels()[0], 1, 1, args);
        } catch (const UBTrap& trap) {
            EXPECT_NE(trap.message().find("division by zero"), std::string::npos);
            throw;
        }
    }, UBTrap);
}

TEST_F(ASTInterpreterTest, TrapIntegerModuloByZero) {
    auto prog = parseAndValidate(R"(
        kernel trap_mod(i32* out) {
            i32 z = 0;
            out[0] = 42 % z;
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(1, 0));
    std::unordered_map<std::string, KernelArg> args = {{"out", KernelArg::fromBuffer("out")}};

    ASTInterpreter interp(buffers);
    EXPECT_THROW({
        try {
            interp.launch1D(*prog->kernels()[0], 1, 1, args);
        } catch (const UBTrap& trap) {
            EXPECT_NE(trap.message().find("modulo by zero"), std::string::npos);
            throw;
        }
    }, UBTrap);
}

TEST_F(ASTInterpreterTest, TrapSignedDivisionOverflow) {
    auto prog = parseAndValidate(R"(
        kernel trap_overflow(i32* out) {
            i32 min_val = -2147483648;
            i32 neg_one = -1;
            out[0] = min_val / neg_one;
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(1, 0));
    std::unordered_map<std::string, KernelArg> args = {{"out", KernelArg::fromBuffer("out")}};

    ASTInterpreter interp(buffers);
    EXPECT_THROW({
        try {
            interp.launch1D(*prog->kernels()[0], 1, 1, args);
        } catch (const UBTrap& trap) {
            EXPECT_NE(trap.message().find("signed integer division overflow"), std::string::npos);
            throw;
        }
    }, UBTrap);
}

TEST_F(ASTInterpreterTest, TrapShiftCountNegative) {
    auto prog = parseAndValidate(R"(
        kernel trap_shift_neg(i32* out) {
            i32 shift = -1;
            out[0] = 1 << shift;
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(1, 0));
    std::unordered_map<std::string, KernelArg> args = {{"out", KernelArg::fromBuffer("out")}};

    ASTInterpreter interp(buffers);
    EXPECT_THROW({
        try {
            interp.launch1D(*prog->kernels()[0], 1, 1, args);
        } catch (const UBTrap& trap) {
            EXPECT_NE(trap.message().find("shift count out of range"), std::string::npos);
            throw;
        }
    }, UBTrap);
}

TEST_F(ASTInterpreterTest, TrapShiftCountTooLarge) {
    auto prog = parseAndValidate(R"(
        kernel trap_shift_large(i32* out) {
            i32 shift = 32;
            out[0] = 1 << shift;
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(1, 0));
    std::unordered_map<std::string, KernelArg> args = {{"out", KernelArg::fromBuffer("out")}};

    ASTInterpreter interp(buffers);
    EXPECT_THROW({
        try {
            interp.launch1D(*prog->kernels()[0], 1, 1, args);
        } catch (const UBTrap& trap) {
            EXPECT_NE(trap.message().find("shift count out of range"), std::string::npos);
            throw;
        }
    }, UBTrap);
}

TEST_F(ASTInterpreterTest, TrapBufferOutOfBoundsHigh) {
    auto prog = parseAndValidate(R"(
        kernel trap_oob(i32* out) {
            out[10] = 42;
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(4, 0)); // Size 4, index 10
    std::unordered_map<std::string, KernelArg> args = {{"out", KernelArg::fromBuffer("out")}};

    ASTInterpreter interp(buffers);
    EXPECT_THROW({
        try {
            interp.launch1D(*prog->kernels()[0], 1, 1, args);
        } catch (const UBTrap& trap) {
            EXPECT_NE(trap.message().find("out-of-bounds buffer access"), std::string::npos);
            throw;
        }
    }, UBTrap);
}

TEST_F(ASTInterpreterTest, TrapBufferOutOfBoundsNegative) {
    auto prog = parseAndValidate(R"(
        kernel trap_oob_neg(i32* out) {
            out[-1] = 42;
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(4, 0));
    std::unordered_map<std::string, KernelArg> args = {{"out", KernelArg::fromBuffer("out")}};

    ASTInterpreter interp(buffers);
    EXPECT_THROW({
        try {
            interp.launch1D(*prog->kernels()[0], 1, 1, args);
        } catch (const UBTrap& trap) {
            EXPECT_NE(trap.message().find("out-of-bounds buffer access"), std::string::npos);
            throw;
        }
    }, UBTrap);
}

TEST_F(ASTInterpreterTest, TrapInfiniteLoopStepLimit) {
    auto prog = parseAndValidate(R"(
        kernel trap_inf() {
            while (true) {
                i32 x = 1;
            }
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    std::unordered_map<std::string, KernelArg> args;

    // Set maxSteps to 100 for fast test
    ASTInterpreter interp(buffers, /*maxSteps=*/100);
    EXPECT_THROW({
        try {
            interp.launch1D(*prog->kernels()[0], 1, 1, args);
        } catch (const UBTrap& trap) {
            EXPECT_NE(trap.message().find("step limit exceeded"), std::string::npos);
            throw;
        }
    }, UBTrap);
}

TEST_F(ASTInterpreterTest, TrapFloatToIntOverflow) {
    auto prog = parseAndValidate(R"(
        kernel trap_cast(i32* out) {
            f32 huge = 1.0e20f;
            out[0] = i32(huge);
        }
    )");
    ASSERT_NE(prog, nullptr);

    BufferManager buffers;
    buffers.allocateI32("out", std::vector<int32_t>(1, 0));
    std::unordered_map<std::string, KernelArg> args = {{"out", KernelArg::fromBuffer("out")}};

    ASTInterpreter interp(buffers);
    EXPECT_THROW({
        try {
            interp.launch1D(*prog->kernels()[0], 1, 1, args);
        } catch (const UBTrap& trap) {
            EXPECT_NE(trap.message().find("float to int conversion out of range"), std::string::npos);
            throw;
        }
    }, UBTrap);
}
