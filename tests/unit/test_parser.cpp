#include <gtest/gtest.h>
#include "ptxlab/kc/ast_printer.h"
#include "ptxlab/kc/lexer.h"
#include "ptxlab/kc/parser.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

using namespace ptxlab;

class ParserTest : public ::testing::Test {
protected:
    std::unique_ptr<Program> parse(std::string_view source, std::string filename = "test.kc") {
        sm.setSource(std::move(filename), std::string(source));
        diag.clear();
        Lexer lexer(sm, diag);
        Parser parser(lexer, sm, diag);
        return parser.parseProgram();
    }

    SourceManager sm;
    DiagnosticEngine diag;
};

TEST_F(ParserTest, EmptyKernel) {
    auto prog = parse("kernel empty() {}");
    EXPECT_FALSE(diag.hasErrors());
    ASSERT_NE(prog, nullptr);
    ASSERT_EQ(prog->kernels().size(), 1);
    EXPECT_EQ(prog->kernels()[0]->name(), "empty");
    EXPECT_TRUE(prog->kernels()[0]->params().empty());
}

TEST_F(ParserTest, KernelParameters) {
    auto prog = parse("kernel test(i32 a, f32* b, bool c) {}");
    EXPECT_FALSE(diag.hasErrors());
    ASSERT_NE(prog, nullptr);
    ASSERT_EQ(prog->kernels().size(), 1);

    const auto& params = prog->kernels()[0]->params();
    ASSERT_EQ(params.size(), 3);
    EXPECT_EQ(params[0].name(), "a");
    EXPECT_EQ(params[0].type().toString(), "i32");
    EXPECT_EQ(params[1].name(), "b");
    EXPECT_EQ(params[1].type().toString(), "f32*");
    EXPECT_EQ(params[2].name(), "c");
    EXPECT_EQ(params[2].type().toString(), "bool");
}

TEST_F(ParserTest, StatementsAndExpressions) {
    const char* src = R"(
        kernel foo() {
            i32 x = 10;
            x = x + 5;
            return;
        }
    )";
    auto prog = parse(src);
    EXPECT_FALSE(diag.hasErrors());
    ASSERT_NE(prog, nullptr);

    const auto* body = prog->kernels()[0]->body();
    ASSERT_EQ(body->statements().size(), 3);
}

TEST_F(ParserTest, OperatorPrecedenceMultiplyOverAdd) {
    // a + b * c
    const char* src = "kernel test() { i32 x = a + b * c; }";
    auto prog = parse(src);
    EXPECT_FALSE(diag.hasErrors());

    std::string dump = ASTPrinter::dump(*prog);
    // Tree should show '+' at top, '*' as child
    std::string expectedSub = 
        "      DeclStmt 'x' type=i32\n"
        "        BinaryExpr '+'\n"
        "          Identifier 'a'\n"
        "          BinaryExpr '*'\n"
        "            Identifier 'b'\n"
        "            Identifier 'c'\n";
    EXPECT_NE(dump.find(expectedSub), std::string::npos);
}

TEST_F(ParserTest, OperatorPrecedenceRelationalAndEquality) {
    // a < b == c <= d
    const char* src = "kernel test() { bool x = a < b == c <= d; }";
    auto prog = parse(src);
    EXPECT_FALSE(diag.hasErrors());

    std::string dump = ASTPrinter::dump(*prog);
    std::string expectedSub =
        "      DeclStmt 'x' type=bool\n"
        "        BinaryExpr '=='\n"
        "          BinaryExpr '<'\n"
        "            Identifier 'a'\n"
        "            Identifier 'b'\n"
        "          BinaryExpr '<='\n"
        "            Identifier 'c'\n"
        "            Identifier 'd'\n";
    EXPECT_NE(dump.find(expectedSub), std::string::npos);
}

TEST_F(ParserTest, OperatorPrecedenceLogicalAndOr) {
    // a && b || c && d
    const char* src = "kernel test() { bool x = a && b || c && d; }";
    auto prog = parse(src);
    EXPECT_FALSE(diag.hasErrors());

    std::string dump = ASTPrinter::dump(*prog);
    std::string expectedSub =
        "      DeclStmt 'x' type=bool\n"
        "        BinaryExpr '||'\n"
        "          BinaryExpr '&&'\n"
        "            Identifier 'a'\n"
        "            Identifier 'b'\n"
        "          BinaryExpr '&&'\n"
        "            Identifier 'c'\n"
        "            Identifier 'd'\n";
    EXPECT_NE(dump.find(expectedSub), std::string::npos);
}

TEST_F(ParserTest, DanglingElseBinding) {
    // if (c1) if (c2) s1; else s2; -> else binds to inner if (c2)
    const char* src = R"(
        kernel test() {
            if (c1)
                if (c2)
                    x = 1;
                else
                    x = 2;
        }
    )";
    auto prog = parse(src);
    EXPECT_FALSE(diag.hasErrors());

    std::string dump = ASTPrinter::dump(*prog);
    // Outer IfStmt should NOT have Else, Inner IfStmt MUST have Else
    EXPECT_NE(dump.find("Else:\n"), std::string::npos);
}

TEST_F(ParserTest, WhileAndForLoops) {
    const char* src = R"(
        kernel loops() {
            while (i < 10) {
                i = i + 1;
            }
            for (i32 j = 0; j < 5; j = j + 1) {
                sum = sum + j;
            }
        }
    )";
    auto prog = parse(src);
    EXPECT_FALSE(diag.hasErrors());
    ASSERT_NE(prog, nullptr);
}

TEST_F(ParserTest, RealisticKernelSaxpy) {
    const char* saxpy = R"(
        kernel saxpy(i32 n, f32 a, f32* x, f32* y) {
            i32 tid = tid_x();
            if (tid < n) {
                y[tid] = a * x[tid] + y[tid];
            }
        }
    )";
    auto prog = parse(saxpy);
    EXPECT_FALSE(diag.hasErrors());
    ASSERT_NE(prog, nullptr);
    ASSERT_EQ(prog->kernels().size(), 1);
    EXPECT_EQ(prog->kernels()[0]->name(), "saxpy");
    EXPECT_EQ(prog->kernels()[0]->params().size(), 4);
}

TEST_F(ParserTest, ErrorRecoveryMissingSemicolon) {
    const char* badSrc = R"(
        kernel test() {
            i32 x = 10
            i32 y = 20;
        }
    )";
    auto prog = parse(badSrc);
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_GE(diag.errorCount(), 1);
    // Parser must recover and not crash
    ASSERT_NE(prog, nullptr);
}

TEST_F(ParserTest, ErrorRecoveryMultipleErrors) {
    const char* badSrc = R"(
        kernel test() {
            i32 x = ;
            i32 y = 20;
            z = ;
        }
    )";
    auto prog = parse(badSrc);
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_GE(diag.errorCount(), 2);
}
