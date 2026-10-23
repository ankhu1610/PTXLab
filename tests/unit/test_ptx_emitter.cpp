#include <gtest/gtest.h>
#include "ptxlab/ir/lower.h"
#include "ptxlab/kc/lexer.h"
#include "ptxlab/kc/parser.h"
#include "ptxlab/kc/sema.h"
#include "ptxlab/ptx/ptx_emitter.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

using namespace ptxlab;
using namespace ptxlab::ir;
using namespace ptxlab::ptx;

class PTXEmitterTest : public ::testing::Test {
protected:
    std::string compileToPTX(std::string_view source, PTXOptions opts = {}) {
        sm.setSource("kernel.kc", std::string(source));
        diag.clear();
        Lexer lexer(sm, diag);
        Parser parser(lexer, sm, diag);
        auto program = parser.parseProgram();
        if (diag.hasErrors() || !program) {
            ADD_FAILURE() << "Parsing failed";
            return "";
        }

        Sema sema(sm, diag);
        if (!sema.analyze(*program)) {
            ADD_FAILURE() << "Semantic analysis failed";
            return "";
        }

        Lowering lowerer;
        auto irMod = lowerer.lower(*program);
        if (!irMod) {
            ADD_FAILURE() << "Lowering failed";
            return "";
        }

        PTXEmitter emitter(std::move(opts));
        return emitter.emit(*irMod);
    }

    SourceManager sm;
    DiagnosticEngine diag;
};

// ===========================================================================
// Test 1: Header Directives and Prologue
// ===========================================================================

TEST_F(PTXEmitterTest, DirectivesAndPrologue) {
    std::string ptx = compileToPTX(R"(
        kernel emptyKernel() {
            return;
        }
    )");

    EXPECT_NE(ptx.find(".version 7.0"), std::string::npos);
    EXPECT_NE(ptx.find(".target sm_75"), std::string::npos);
    EXPECT_NE(ptx.find(".address_size 64"), std::string::npos);
    EXPECT_NE(ptx.find(".visible .entry emptyKernel("), std::string::npos);
    EXPECT_NE(ptx.find("ret;"), std::string::npos);
}

// ===========================================================================
// Test 2: Custom Architecture and Version Options
// ===========================================================================

TEST_F(PTXEmitterTest, CustomOptionsTarget) {
    PTXOptions customOpts;
    customOpts.target = "sm_89";
    customOpts.version = "8.2";
    customOpts.addressSize = 64;

    std::string ptx = compileToPTX(R"(
        kernel dummy() {}
    )", customOpts);

    EXPECT_NE(ptx.find(".version 8.2"), std::string::npos);
    EXPECT_NE(ptx.find(".target sm_89"), std::string::npos);
}

// ===========================================================================
// Test 3: Realistic SAXPY Kernel (Golden PTX Test)
// ===========================================================================

TEST_F(PTXEmitterTest, SaxpyGoldenPTX) {
    std::string ptx = compileToPTX(R"(
        kernel saxpy(i32 n, f32 a, f32* x, f32* y) {
            i32 tid = ctaid_x() * ntid_x() + tid_x();
            if (tid < n) {
                y[tid] = a * x[tid] + y[tid];
            }
        }
    )");

    // 1. Parameter ABI
    EXPECT_NE(ptx.find(".param .u32 _param_n"), std::string::npos);
    EXPECT_NE(ptx.find(".param .f32 _param_a"), std::string::npos);
    EXPECT_NE(ptx.find(".param .u64 _param_x"), std::string::npos);
    EXPECT_NE(ptx.find(".param .u64 _param_y"), std::string::npos);

    // 2. Register Bank Declarations
    EXPECT_NE(ptx.find(".reg .b32   %r<"), std::string::npos);
    EXPECT_NE(ptx.find(".reg .f32   %f<"), std::string::npos);
    EXPECT_NE(ptx.find(".reg .b64   %rd<"), std::string::npos);
    EXPECT_NE(ptx.find(".reg .b64   %rd_offset;"), std::string::npos);
    EXPECT_NE(ptx.find(".reg .pred  %p<"), std::string::npos);

    // 3. Parameter Loading & cvta to global address space
    EXPECT_NE(ptx.find("ld.param.u32"), std::string::npos);
    EXPECT_NE(ptx.find("ld.param.f32"), std::string::npos);
    EXPECT_NE(ptx.find("ld.param.u64"), std::string::npos);
    EXPECT_NE(ptx.find("cvta.to.global.u64"), std::string::npos);

    // 4. Special Thread Intrinsics
    EXPECT_NE(ptx.find("mov.u32"), std::string::npos);
    EXPECT_NE(ptx.find("%ctaid.x"), std::string::npos);
    EXPECT_NE(ptx.find("%ntid.x"), std::string::npos);
    EXPECT_NE(ptx.find("%tid.x"), std::string::npos);

    // 5. Index Arithmetic & Comparisons
    EXPECT_NE(ptx.find("mul.lo.s32"), std::string::npos);
    EXPECT_NE(ptx.find("add.s32"), std::string::npos);
    EXPECT_NE(ptx.find("setp.lt.s32"), std::string::npos);

    // 6. Memory Indexing (Wide Multiplication & Offset Add)
    EXPECT_NE(ptx.find("mul.wide.s32    %rd_offset,"), std::string::npos);
    EXPECT_NE(ptx.find("add.s64"), std::string::npos);

    // 7. Global Memory Loads, Floats with .rn, and Stores
    EXPECT_NE(ptx.find("ld.global.f32"), std::string::npos);
    EXPECT_NE(ptx.find("mul.rn.f32"), std::string::npos);
    EXPECT_NE(ptx.find("add.rn.f32"), std::string::npos);
    EXPECT_NE(ptx.find("st.global.f32"), std::string::npos);

    // 8. Control Flow Branches
    EXPECT_NE(ptx.find("bra"), std::string::npos);
    EXPECT_NE(ptx.find("ret;"), std::string::npos);
}

// ===========================================================================
// Test 4: Integer Arithmetic and Bitwise Opcodes
// ===========================================================================

TEST_F(PTXEmitterTest, IntegerArithmeticAndBitwise) {
    std::string ptx = compileToPTX(R"(
        kernel mathInt(i32 a, i32 b, i32* out) {
            i32 rAdd = a + b;
            i32 rSub = a - b;
            i32 rMul = a * b;
            i32 rDiv = a / b;
            i32 rRem = a % b;
            i32 rAnd = a & b;
            i32 rOr  = a | b;
            i32 rXor = a ^ b;
            i32 rShl = a << b;
            i32 rShr = a >> b;
            i32 rNeg = -a;
            i32 rNot = ~b;
            out[0] = rAdd + rSub + rMul + rDiv + rRem + rAnd + rOr + rXor + rShl + rShr + rNeg + rNot;
        }
    )");

    EXPECT_NE(ptx.find("add.s32"), std::string::npos);
    EXPECT_NE(ptx.find("sub.s32"), std::string::npos);
    EXPECT_NE(ptx.find("mul.lo.s32"), std::string::npos);
    EXPECT_NE(ptx.find("div.s32"), std::string::npos);
    EXPECT_NE(ptx.find("rem.s32"), std::string::npos);
    EXPECT_NE(ptx.find("and.b32"), std::string::npos);
    EXPECT_NE(ptx.find("or.b32"), std::string::npos);
    EXPECT_NE(ptx.find("xor.b32"), std::string::npos);
    EXPECT_NE(ptx.find("shl.b32"), std::string::npos);
    EXPECT_NE(ptx.find("shr.s32"), std::string::npos);
    EXPECT_NE(ptx.find("neg.s32"), std::string::npos);
    EXPECT_NE(ptx.find("not.b32"), std::string::npos);
    EXPECT_NE(ptx.find("st.global.s32"), std::string::npos);
}

// ===========================================================================
// Test 5: Floating Point Arithmetic and Explicit Rounding (.rn)
// ===========================================================================

TEST_F(PTXEmitterTest, FloatArithmeticRounding) {
    std::string ptx = compileToPTX(R"(
        kernel mathFloat(f32 a, f32 b, f32* out) {
            f32 rAdd = a + b;
            f32 rSub = a - b;
            f32 rMul = a * b;
            f32 rDiv = a / b;
            f32 rNeg = -a;
            out[0] = (rAdd + rSub) * (rMul / rDiv) + rNeg;
        }
    )");

    EXPECT_NE(ptx.find("add.rn.f32"), std::string::npos);
    EXPECT_NE(ptx.find("sub.rn.f32"), std::string::npos);
    EXPECT_NE(ptx.find("mul.rn.f32"), std::string::npos);
    EXPECT_NE(ptx.find("div.rn.f32"), std::string::npos);
    EXPECT_NE(ptx.find("neg.f32"), std::string::npos);
    EXPECT_NE(ptx.find("st.global.f32"), std::string::npos);
}

// ===========================================================================
// Test 6: Relational Comparisons (Integers and Floats)
// ===========================================================================

TEST_F(PTXEmitterTest, RelationalComparisons) {
    std::string ptx = compileToPTX(R"(
        kernel cmpTest(i32 x, i32 y, f32 fa, f32 fb, i32* out) {
            bool c1 = (x == y);
            bool c2 = (x != y);
            bool c3 = (x < y);
            bool c4 = (x <= y);
            bool c5 = (x > y);
            bool c6 = (x >= y);

            bool fc1 = (fa < fb);
            bool fc2 = (fa >= fb);

            if (c1 && !c2 && (c3 || c4) && (c5 || c6) && (fc1 || fc2)) {
                out[0] = 1;
            }
        }
    )");

    EXPECT_NE(ptx.find("setp.eq.s32"), std::string::npos);
    EXPECT_NE(ptx.find("setp.ne.s32"), std::string::npos);
    EXPECT_NE(ptx.find("setp.lt.s32"), std::string::npos);
    EXPECT_NE(ptx.find("setp.le.s32"), std::string::npos);
    EXPECT_NE(ptx.find("setp.gt.s32"), std::string::npos);
    EXPECT_NE(ptx.find("setp.ge.s32"), std::string::npos);
    EXPECT_NE(ptx.find("setp.lt.f32"), std::string::npos);
    EXPECT_NE(ptx.find("setp.ge.f32"), std::string::npos);
}

// ===========================================================================
// Test 7: While and For Loop Control Flow
// ===========================================================================

TEST_F(PTXEmitterTest, LoopControlFlow) {
    std::string ptx = compileToPTX(R"(
        kernel loopTest(i32 n, i32* data) {
            i32 sum = 0;
            for (i32 i = 0; i < n; i = i + 1) {
                sum = sum + data[i];
            }

            while (sum > 100) {
                sum = sum - 1;
            }
            data[0] = sum;
        }
    )");

    EXPECT_NE(ptx.find("$L_for_head"), std::string::npos);
    EXPECT_NE(ptx.find("$L_for_body"), std::string::npos);
    EXPECT_NE(ptx.find("$L_for_step"), std::string::npos);
    EXPECT_NE(ptx.find("$L_for_exit"), std::string::npos);
    EXPECT_NE(ptx.find("$L_while_head"), std::string::npos);
    EXPECT_NE(ptx.find("$L_while_body"), std::string::npos);
    EXPECT_NE(ptx.find("$L_while_exit"), std::string::npos);
}

// ===========================================================================
// Test 8: IEEE 754 Floating Immediate Constant Bit Patterns
// ===========================================================================

TEST_F(PTXEmitterTest, FloatHexConstants) {
    std::string ptx = compileToPTX(R"(
        kernel floatConst(f32* out) {
            f32 zero = 0.0f;
            f32 one = 1.0f;
            f32 twoPointFive = 2.5f;
            out[0] = zero + one + twoPointFive;
        }
    )");

    // 0.0f = 0x00000000 -> 0f00000000
    EXPECT_NE(ptx.find("0f00000000"), std::string::npos);
    // 1.0f = 0x3F800000 -> 0f3F800000
    EXPECT_NE(ptx.find("0f3F800000"), std::string::npos);
    // 2.5f = 0x40200000 -> 0f40200000
    EXPECT_NE(ptx.find("0f40200000"), std::string::npos);
}
