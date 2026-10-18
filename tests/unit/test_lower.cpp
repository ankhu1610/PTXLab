#include <gtest/gtest.h>
#include "ptxlab/ir/ir_printer.h"
#include "ptxlab/ir/lower.h"
#include "ptxlab/kc/lexer.h"
#include "ptxlab/kc/parser.h"
#include "ptxlab/kc/sema.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

using namespace ptxlab;

class LowerTest : public ::testing::Test {
protected:
    std::unique_ptr<ir::Module> compileToIR(std::string_view source) {
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

        ir::Lowering lowerer;
        return lowerer.lower(*program);
    }

    SourceManager sm;
    DiagnosticEngine diag;
};

TEST_F(LowerTest, LowerRealisticKernelSaxpy) {
    auto mod = compileToIR(R"(
        kernel saxpy(i32 n, f32 a, f32* x, f32* y) {
            i32 tid = tid_x();
            if (tid < n) {
                y[tid] = a * x[tid] + y[tid];
            }
        }
    )");
    ASSERT_NE(mod, nullptr);
    ASSERT_EQ(mod->functions().size(), 1);

    const auto& func = *mod->functions()[0];
    EXPECT_EQ(func.name(), "saxpy");
    EXPECT_EQ(func.params().size(), 4);

    std::string dump = ir::IRPrinter::dump(func);
    EXPECT_NE(dump.find("special tid_x"), std::string::npos);
    EXPECT_NE(dump.find("cmp.lt"), std::string::npos);
    EXPECT_NE(dump.find("condbr"), std::string::npos);
    EXPECT_NE(dump.find("elemaddr"), std::string::npos);
    EXPECT_NE(dump.find("load"), std::string::npos);
    EXPECT_NE(dump.find("fmul"), std::string::npos);
    EXPECT_NE(dump.find("fadd"), std::string::npos);
    EXPECT_NE(dump.find("store"), std::string::npos);
}

TEST_F(LowerTest, LowerWhileLoop) {
    auto mod = compileToIR(R"(
        kernel loop_test(i32 n) {
            i32 i = 0;
            while (i < n) {
                i = i + 1;
            }
        }
    )");
    ASSERT_NE(mod, nullptr);
    const auto& func = *mod->functions()[0];

    std::string dump = ir::IRPrinter::dump(func);
    EXPECT_NE(dump.find("while_head"), std::string::npos);
    EXPECT_NE(dump.find("while_body"), std::string::npos);
    EXPECT_NE(dump.find("while_exit"), std::string::npos);
}

TEST_F(LowerTest, LowerForLoop) {
    auto mod = compileToIR(R"(
        kernel for_test(i32 n) {
            i32 sum = 0;
            for (i32 i = 0; i < n; i = i + 1) {
                sum = sum + i;
            }
        }
    )");
    ASSERT_NE(mod, nullptr);
    const auto& func = *mod->functions()[0];

    std::string dump = ir::IRPrinter::dump(func);
    EXPECT_NE(dump.find("for_head"), std::string::npos);
    EXPECT_NE(dump.find("for_body"), std::string::npos);
    EXPECT_NE(dump.find("for_step"), std::string::npos);
    EXPECT_NE(dump.find("for_exit"), std::string::npos);
}

TEST_F(LowerTest, LowerShortCircuitAnd) {
    auto mod = compileToIR(R"(
        kernel sc_and(i32 a, i32 b) {
            if (a > 0 && b > 0) {
                i32 x = 1;
            }
        }
    )");
    ASSERT_NE(mod, nullptr);
    const auto& func = *mod->functions()[0];

    std::string dump = ir::IRPrinter::dump(func);
    EXPECT_NE(dump.find("and_rhs"), std::string::npos);
}
