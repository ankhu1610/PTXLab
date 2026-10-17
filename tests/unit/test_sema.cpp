#include <gtest/gtest.h>
#include "ptxlab/kc/lexer.h"
#include "ptxlab/kc/parser.h"
#include "ptxlab/kc/sema.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

using namespace ptxlab;

class SemaTest : public ::testing::Test {
protected:
    std::unique_ptr<Program> parseAndAnalyze(std::string_view source, bool expectSuccess = true) {
        sm.setSource("test.kc", std::string(source));
        diag.clear();
        Lexer lexer(sm, diag);
        Parser parser(lexer, sm, diag);

        auto program = parser.parseProgram();
        if (diag.hasErrors()) {
            if (expectSuccess) {
                ADD_FAILURE() << "Parsing failed unexpectedly:\n" << getDiagnosticString();
            }
            return nullptr;
        }

        Sema sema(sm, diag);
        bool success = sema.analyze(*program);
        if (expectSuccess && !success) {
            ADD_FAILURE() << "Semantic analysis failed unexpectedly:\n" << getDiagnosticString();
        }
        return program;
    }

    std::string getDiagnosticString() const {
        std::string s;
        for (const auto& d : diag.getDiagnostics()) {
            s += diag.format(d, sm);
        }
        return s;
    }

    bool hasErrorMessage(std::string_view snippet) const {
        for (const auto& d : diag.getDiagnostics()) {
            if (d.message.find(snippet) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    SourceManager sm;
    DiagnosticEngine diag;
};

// ===========================================================================
// NEGATIVE TESTS (>= 20 tests required by Milestone 3)
// ===========================================================================

// 1. Undeclared identifier read
TEST_F(SemaTest, NegativeUndeclaredIdentifier) {
    parseAndAnalyze("kernel k() { i32 y = x + 1; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("use of undeclared identifier 'x'"));
}

// 2. Local pointer variable declaration
TEST_F(SemaTest, NegativeLocalPointerVariable) {
    parseAndAnalyze("kernel k(f32* buf) { i32* p = 0; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("local variable cannot have pointer type 'i32*'"));
}

// 3. Local float pointer variable declaration
TEST_F(SemaTest, NegativeLocalFloatPointerVariable) {
    parseAndAnalyze("kernel k() { f32* p = 0; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("local variable cannot have pointer type 'f32*'"));
}

// 4. Redefinition of variable in same scope
TEST_F(SemaTest, NegativeRedefinitionVariableSameScope) {
    parseAndAnalyze("kernel k() { i32 x = 1; i32 x = 2; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("redefinition of variable 'x'"));
}

// 5. Type mismatch in declaration initialization (i32 with f32)
TEST_F(SemaTest, NegativeTypeMismatchDeclInitFloatToInt) {
    parseAndAnalyze("kernel k() { i32 x = 3.14f; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("cannot initialize variable of type 'i32' with rvalue of type 'f32'"));
}

// 6. Type mismatch in declaration initialization (bool with i32)
TEST_F(SemaTest, NegativeTypeMismatchDeclInitIntToBool) {
    parseAndAnalyze("kernel k() { bool b = 1; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("cannot initialize variable of type 'bool' with rvalue of type 'i32'"));
}

// 7. Type mismatch in assignment
TEST_F(SemaTest, NegativeTypeMismatchAssignment) {
    parseAndAnalyze("kernel k() { i32 x = 1; x = 2.5f; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("assigning to 'i32' from incompatible type 'f32'"));
}

// 8. Direct assignment to pointer variable
TEST_F(SemaTest, NegativeDirectPointerAssignment) {
    parseAndAnalyze("kernel k(f32* p) { p = 1.0f; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("cannot assign directly to pointer variable 'p'"));
}

// 9. Subscript on non-pointer
TEST_F(SemaTest, NegativeNonPointerSubscript) {
    parseAndAnalyze("kernel k() { i32 x = 10; x[0] = 5; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("subscripted value is not a pointer (type is 'i32')"));
}

// 10. Non-integer array subscript index
TEST_F(SemaTest, NegativeNonIntegerSubscriptIndex) {
    parseAndAnalyze("kernel k(f32* p) { p[1.5f] = 2.0f; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("array subscript is not an integer (type is 'f32')"));
}

// 11. If condition must be bool
TEST_F(SemaTest, NegativeIfConditionNotBool) {
    parseAndAnalyze("kernel k() { if (1) { i32 a = 0; } }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("statement condition must have type 'bool' (got 'i32')"));
}

// 12. While condition must be bool
TEST_F(SemaTest, NegativeWhileConditionNotBool) {
    parseAndAnalyze("kernel k() { while (3.14f) { } }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("statement condition must have type 'bool' (got 'f32')"));
}

// 13. For loop condition must be bool
TEST_F(SemaTest, NegativeForConditionNotBool) {
    parseAndAnalyze("kernel k() { for (i32 i = 0; i; i = i + 1) { } }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("statement condition must have type 'bool' (got 'i32')"));
}

// 14. Binary arithmetic operand type mismatch
TEST_F(SemaTest, NegativeBinaryArithmeticTypeMismatch) {
    parseAndAnalyze("kernel k() { f32 res = 1 + 2.0f; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("invalid operands to binary expression ('i32' and 'f32')"));
}

// 15. Modulo with float operands
TEST_F(SemaTest, NegativeModuloFloat) {
    parseAndAnalyze("kernel k() { f32 res = 5.0f % 2.0f; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("invalid operands to modulo expression ('f32' and 'f32')"));
}

// 16. Bitwise operator on float
TEST_F(SemaTest, NegativeBitwiseFloat) {
    parseAndAnalyze("kernel k() { f32 res = 5.0f & 2.0f; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("bitwise operator requires integer operands ('f32' given)"));
}

// 17. Bitwise shift by float
TEST_F(SemaTest, NegativeBitwiseShiftFloat) {
    parseAndAnalyze("kernel k() { i32 res = 1 << 2.0f; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("bitwise operator requires integer operands ('f32' given)"));
}

// 18. Relational comparison between incompatible types
TEST_F(SemaTest, NegativeRelationalTypeMismatch) {
    parseAndAnalyze("kernel k() { bool b = 1 < 2.0f; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("comparison between incompatible types ('i32' and 'f32')"));
}

// 19. Logical NOT on integer
TEST_F(SemaTest, NegativeLogicalNotInt) {
    parseAndAnalyze("kernel k() { bool b = !5; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("invalid argument type 'i32' to logical not"));
}

// 20. Bitwise NOT on float
TEST_F(SemaTest, NegativeBitwiseNotFloat) {
    parseAndAnalyze("kernel k() { i32 res = ~3.14f; }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("invalid argument type 'f32' to bitwise not"));
}

// 21. Built-in with unexpected arguments
TEST_F(SemaTest, NegativeBuiltinTidArgs) {
    parseAndAnalyze("kernel k() { i32 tid = tid_x(0); }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("builtin 'tid_x' expects 0 arguments"));
}

// 22. Unknown function or builtin
TEST_F(SemaTest, NegativeUnknownFunction) {
    parseAndAnalyze("kernel k() { i32 res = my_func(); }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("unknown function or builtin 'my_func'"));
}

// 23. Explicit cast with wrong argument count
TEST_F(SemaTest, NegativeExplicitCastArgCount) {
    parseAndAnalyze("kernel k() { i32 res = i32(1, 2); }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("explicit conversion 'i32' expects 1 argument"));
}

// 24. Explicit cast with invalid argument type
TEST_F(SemaTest, NegativeExplicitCastInvalidType) {
    parseAndAnalyze("kernel k(f32* buf) { i32 res = i32(buf); }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("cannot cast 'f32*' to 'i32'"));
}

// 25. Duplicate parameter name in kernel declaration
TEST_F(SemaTest, NegativeDuplicateParameterName) {
    parseAndAnalyze("kernel k(i32 a, f32 a) { }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("redefinition of parameter 'a'"));
}

// 26. Redefinition of kernel
TEST_F(SemaTest, NegativeKernelRedefinition) {
    parseAndAnalyze("kernel k() { } kernel k() { }", /*expectSuccess=*/false);
    EXPECT_TRUE(hasErrorMessage("redefinition of kernel 'k'"));
}

// ===========================================================================
// POSITIVE TESTS
// ===========================================================================

TEST_F(SemaTest, PositiveRealisticKernelSaxpy) {
    auto prog = parseAndAnalyze(R"(
        kernel saxpy(i32 n, f32 a, f32* x, f32* y) {
            i32 tid = tid_x();
            if (tid < n) {
                y[tid] = a * x[tid] + y[tid];
            }
        }
    )", /*expectSuccess=*/true);
    ASSERT_NE(prog, nullptr);
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(SemaTest, PositiveVariableShadowingInNestedScope) {
    auto prog = parseAndAnalyze(R"(
        kernel shadow_test(i32 x) {
            i32 a = x;
            {
                i32 x = 42; // Allowed: shadows outer parameter x
                a = a + x;
            }
        }
    )", /*expectSuccess=*/true);
    ASSERT_NE(prog, nullptr);
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(SemaTest, PositiveExplicitCasts) {
    auto prog = parseAndAnalyze(R"(
        kernel cast_test(f32 f, i32 i) {
            i32 to_i = i32(f);
            f32 to_f = f32(i);
            i32 b_to_i = i32(true);
        }
    )", /*expectSuccess=*/true);
    ASSERT_NE(prog, nullptr);
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(SemaTest, PositiveAllOperators) {
    auto prog = parseAndAnalyze(R"(
        kernel op_test(i32 a, i32 b, f32 x, f32 y) {
            i32 add_i = a + b;
            i32 sub_i = a - b;
            i32 mul_i = a * b;
            i32 div_i = a / b;
            i32 mod_i = a % b;
            i32 and_i = a & b;
            i32 or_i  = a | b;
            i32 xor_i = a ^ b;
            i32 shl_i = a << b;
            i32 shr_i = a >> b;
            i32 not_i = ~a;
            i32 neg_i = -a;

            f32 add_f = x + y;
            f32 sub_f = x - y;
            f32 mul_f = x * y;
            f32 div_f = x / y;
            f32 neg_f = -x;

            bool eq_i = (a == b);
            bool ne_i = (a != b);
            bool lt_i = (a < b);
            bool le_i = (a <= b);
            bool gt_i = (a > b);
            bool ge_i = (a >= b);

            bool log_and = eq_i && ne_i;
            bool log_or  = lt_i || ge_i;
            bool log_not = !log_and;
        }
    )", /*expectSuccess=*/true);
    ASSERT_NE(prog, nullptr);
    EXPECT_FALSE(diag.hasErrors());
}
