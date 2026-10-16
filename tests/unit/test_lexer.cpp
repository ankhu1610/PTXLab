#include <gtest/gtest.h>
#include "ptxlab/kc/lexer.h"
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

using namespace ptxlab;

class LexerTest : public ::testing::Test {
protected:
    std::vector<Token> tokenize(std::string_view source, std::string filename = "test.kc") {
        sm.setSource(std::move(filename), std::string(source));
        diag.clear();
        Lexer lexer(sm, diag);
        return lexer.tokenizeAll();
    }

    SourceManager sm;
    DiagnosticEngine diag;
};

// 1. Empty input
TEST_F(LexerTest, EmptyInput) {
    auto tokens = tokenize("");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Eof);
    EXPECT_FALSE(diag.hasErrors());
}

// 2. Whitespace only
TEST_F(LexerTest, WhitespaceOnly) {
    auto tokens = tokenize("   \t\r\n \n\t  ");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Eof);
    EXPECT_EQ(tokens[0].location.line, 3);
    EXPECT_FALSE(diag.hasErrors());
}

// 3. All keywords
TEST_F(LexerTest, KeywordsAll) {
    auto tokens = tokenize("kernel i32 f32 bool true false if else while for return");
    ASSERT_EQ(tokens.size(), 12);
    EXPECT_EQ(tokens[0].kind, TokenKind::KwKernel);
    EXPECT_EQ(tokens[1].kind, TokenKind::KwI32);
    EXPECT_EQ(tokens[2].kind, TokenKind::KwF32);
    EXPECT_EQ(tokens[3].kind, TokenKind::KwBool);
    EXPECT_EQ(tokens[4].kind, TokenKind::KwTrue);
    EXPECT_EQ(tokens[5].kind, TokenKind::KwFalse);
    EXPECT_EQ(tokens[6].kind, TokenKind::KwIf);
    EXPECT_EQ(tokens[7].kind, TokenKind::KwElse);
    EXPECT_EQ(tokens[8].kind, TokenKind::KwWhile);
    EXPECT_EQ(tokens[9].kind, TokenKind::KwFor);
    EXPECT_EQ(tokens[10].kind, TokenKind::KwReturn);
    EXPECT_EQ(tokens[11].kind, TokenKind::Eof);
    EXPECT_FALSE(diag.hasErrors());
}

// 4. Identifiers with letters, numbers, and underscores
TEST_F(LexerTest, IdentifiersBasicAndUnderscore) {
    auto tokens = tokenize("foo bar_123 _tid_x CTA_dim");
    ASSERT_EQ(tokens.size(), 5);
    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[0].text, "foo");
    EXPECT_EQ(tokens[1].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[1].text, "bar_123");
    EXPECT_EQ(tokens[2].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[2].text, "_tid_x");
    EXPECT_EQ(tokens[3].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[3].text, "CTA_dim");
    EXPECT_EQ(tokens[4].kind, TokenKind::Eof);
    EXPECT_FALSE(diag.hasErrors());
}

// 5. Single-line comment
TEST_F(LexerTest, SingleLineComment) {
    auto tokens = tokenize("x = 10; // set x\ny = 20;");
    ASSERT_EQ(tokens.size(), 9);
    EXPECT_EQ(tokens[0].text, "x");
    EXPECT_EQ(tokens[1].kind, TokenKind::Equal);
    EXPECT_EQ(tokens[2].text, "10");
    EXPECT_EQ(tokens[3].kind, TokenKind::Semicolon);
    EXPECT_EQ(tokens[4].text, "y");
    EXPECT_EQ(tokens[5].kind, TokenKind::Equal);
    EXPECT_EQ(tokens[6].text, "20");
    EXPECT_EQ(tokens[7].kind, TokenKind::Semicolon);
    EXPECT_EQ(tokens[8].kind, TokenKind::Eof);
    EXPECT_FALSE(diag.hasErrors());
}

// 6. Multi-line block comment
TEST_F(LexerTest, MultiLineComment) {
    auto tokens = tokenize("a /* inside comment */ + b");
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0].text, "a");
    EXPECT_EQ(tokens[1].kind, TokenKind::Plus);
    EXPECT_EQ(tokens[2].text, "b");
    EXPECT_EQ(tokens[3].kind, TokenKind::Eof);
    EXPECT_FALSE(diag.hasErrors());
}

// 7. Multi-line comment preserves accurate line and column count
TEST_F(LexerTest, MultiLineCommentPreservesLineCol) {
    auto tokens = tokenize("a /* line 1\nline 2\nline 3 */ b");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].text, "a");
    EXPECT_EQ(tokens[0].location.line, 1);
    EXPECT_EQ(tokens[1].text, "b");
    EXPECT_EQ(tokens[1].location.line, 3);
    EXPECT_EQ(tokens[1].location.column, 11);
    EXPECT_FALSE(diag.hasErrors());
}

// 8. Comment at end of file without newline
TEST_F(LexerTest, CommentAtEof) {
    auto tokens = tokenize("val // trailing comment");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].text, "val");
    EXPECT_EQ(tokens[1].kind, TokenKind::Eof);
    EXPECT_FALSE(diag.hasErrors());
}

// 9. Decimal integer literals
TEST_F(LexerTest, IntLiteralDecimal) {
    auto tokens = tokenize("42 1000 2147483647");
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0].kind, TokenKind::IntLiteral);
    EXPECT_EQ(tokens[0].intVal, 42);
    EXPECT_EQ(tokens[1].kind, TokenKind::IntLiteral);
    EXPECT_EQ(tokens[1].intVal, 1000);
    EXPECT_EQ(tokens[2].kind, TokenKind::IntLiteral);
    EXPECT_EQ(tokens[2].intVal, 2147483647);
    EXPECT_FALSE(diag.hasErrors());
}

// 10. Integer literal zero
TEST_F(LexerTest, IntLiteralZero) {
    auto tokens = tokenize("0");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].kind, TokenKind::IntLiteral);
    EXPECT_EQ(tokens[0].intVal, 0);
    EXPECT_FALSE(diag.hasErrors());
}

// 11. Hexadecimal integer literals
TEST_F(LexerTest, IntLiteralHex) {
    auto tokens = tokenize("0x0 0x1f 0x7fffffff 0xffffffff");
    ASSERT_EQ(tokens.size(), 5);
    EXPECT_EQ(tokens[0].intVal, 0);
    EXPECT_EQ(tokens[1].intVal, 0x1f);
    EXPECT_EQ(tokens[2].intVal, 0x7fffffff);
    EXPECT_EQ(tokens[3].intVal, static_cast<int32_t>(0xffffffffU));
    EXPECT_FALSE(diag.hasErrors());
}

// 12. Hexadecimal with uppercase 0X and uppercase digits
TEST_F(LexerTest, IntLiteralHexUppercase) {
    auto tokens = tokenize("0XABC123");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0].kind, TokenKind::IntLiteral);
    EXPECT_EQ(tokens[0].intVal, 0xABC123);
    EXPECT_FALSE(diag.hasErrors());
}

// 13. Standard float literals
TEST_F(LexerTest, FloatLiteralStandard) {
    auto tokens = tokenize("1.0 3.14159 0.0");
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0].kind, TokenKind::FloatLiteral);
    EXPECT_FLOAT_EQ(tokens[0].floatVal, 1.0f);
    EXPECT_EQ(tokens[1].kind, TokenKind::FloatLiteral);
    EXPECT_FLOAT_EQ(tokens[1].floatVal, 3.14159f);
    EXPECT_EQ(tokens[2].kind, TokenKind::FloatLiteral);
    EXPECT_FLOAT_EQ(tokens[2].floatVal, 0.0f);
    EXPECT_FALSE(diag.hasErrors());
}

// 14. Float literal with leading dot (.5)
TEST_F(LexerTest, FloatLiteralLeadingDot) {
    auto tokens = tokenize(".5 .125");
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].kind, TokenKind::FloatLiteral);
    EXPECT_FLOAT_EQ(tokens[0].floatVal, 0.5f);
    EXPECT_EQ(tokens[1].kind, TokenKind::FloatLiteral);
    EXPECT_FLOAT_EQ(tokens[1].floatVal, 0.125f);
    EXPECT_FALSE(diag.hasErrors());
}

// 15. Float literal scientific notation
TEST_F(LexerTest, FloatLiteralScientific) {
    auto tokens = tokenize("1e3 2.5E2 1e+4");
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_FLOAT_EQ(tokens[0].floatVal, 1000.0f);
    EXPECT_FLOAT_EQ(tokens[1].floatVal, 250.0f);
    EXPECT_FLOAT_EQ(tokens[2].floatVal, 10000.0f);
    EXPECT_FALSE(diag.hasErrors());
}

// 16. Float literal negative exponent
TEST_F(LexerTest, FloatLiteralScientificNegativeExponent) {
    auto tokens = tokenize("1.5e-2");
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_FLOAT_EQ(tokens[0].floatVal, 0.015f);
    EXPECT_FALSE(diag.hasErrors());
}

// 17. Float literal trailing f / F suffix
TEST_F(LexerTest, FloatLiteralSuffixF) {
    auto tokens = tokenize("1.0f 0.5F 1e2f");
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_FLOAT_EQ(tokens[0].floatVal, 1.0f);
    EXPECT_FLOAT_EQ(tokens[1].floatVal, 0.5f);
    EXPECT_FLOAT_EQ(tokens[2].floatVal, 100.0f);
    EXPECT_FALSE(diag.hasErrors());
}

// 18. Punctuation delimiters
TEST_F(LexerTest, PunctuationDelimiters) {
    auto tokens = tokenize("( ) { } [ ] ; ,");
    ASSERT_EQ(tokens.size(), 9);
    EXPECT_EQ(tokens[0].kind, TokenKind::LParen);
    EXPECT_EQ(tokens[1].kind, TokenKind::RParen);
    EXPECT_EQ(tokens[2].kind, TokenKind::LBrace);
    EXPECT_EQ(tokens[3].kind, TokenKind::RBrace);
    EXPECT_EQ(tokens[4].kind, TokenKind::LBracket);
    EXPECT_EQ(tokens[5].kind, TokenKind::RBracket);
    EXPECT_EQ(tokens[6].kind, TokenKind::Semicolon);
    EXPECT_EQ(tokens[7].kind, TokenKind::Comma);
    EXPECT_FALSE(diag.hasErrors());
}

// 19. Arithmetic operators
TEST_F(LexerTest, ArithmeticOperators) {
    auto tokens = tokenize("+ - * / %");
    ASSERT_EQ(tokens.size(), 6);
    EXPECT_EQ(tokens[0].kind, TokenKind::Plus);
    EXPECT_EQ(tokens[1].kind, TokenKind::Minus);
    EXPECT_EQ(tokens[2].kind, TokenKind::Star);
    EXPECT_EQ(tokens[3].kind, TokenKind::Slash);
    EXPECT_EQ(tokens[4].kind, TokenKind::Percent);
    EXPECT_FALSE(diag.hasErrors());
}

// 20. Bitwise operators
TEST_F(LexerTest, BitwiseOperators) {
    auto tokens = tokenize("& | ^ ~ << >>");
    ASSERT_EQ(tokens.size(), 7);
    EXPECT_EQ(tokens[0].kind, TokenKind::Amp);
    EXPECT_EQ(tokens[1].kind, TokenKind::Pipe);
    EXPECT_EQ(tokens[2].kind, TokenKind::Caret);
    EXPECT_EQ(tokens[3].kind, TokenKind::Tilde);
    EXPECT_EQ(tokens[4].kind, TokenKind::LessLess);
    EXPECT_EQ(tokens[5].kind, TokenKind::GreaterGreater);
    EXPECT_FALSE(diag.hasErrors());
}

// 21. Relational operators
TEST_F(LexerTest, RelationalOperators) {
    auto tokens = tokenize("== != < <= > >=");
    ASSERT_EQ(tokens.size(), 7);
    EXPECT_EQ(tokens[0].kind, TokenKind::EqualEqual);
    EXPECT_EQ(tokens[1].kind, TokenKind::ExclaimEqual);
    EXPECT_EQ(tokens[2].kind, TokenKind::Less);
    EXPECT_EQ(tokens[3].kind, TokenKind::LessEqual);
    EXPECT_EQ(tokens[4].kind, TokenKind::Greater);
    EXPECT_EQ(tokens[5].kind, TokenKind::GreaterEqual);
    EXPECT_FALSE(diag.hasErrors());
}

// 22. Logical operators and assignment
TEST_F(LexerTest, LogicalOperatorsAndAssignment) {
    auto tokens = tokenize("&& || ! =");
    ASSERT_EQ(tokens.size(), 5);
    EXPECT_EQ(tokens[0].kind, TokenKind::AmpAmp);
    EXPECT_EQ(tokens[1].kind, TokenKind::PipePipe);
    EXPECT_EQ(tokens[2].kind, TokenKind::Exclaim);
    EXPECT_EQ(tokens[3].kind, TokenKind::Equal);
    EXPECT_FALSE(diag.hasErrors());
}

// 23. Maximal munch disambiguation
TEST_F(LexerTest, MaximalMunchDisambiguation) {
    auto tokens = tokenize("<<< === !== >>=");
    ASSERT_EQ(tokens.size(), 9);
    // "<<<" -> "<<" followed by "<"
    EXPECT_EQ(tokens[0].kind, TokenKind::LessLess);
    EXPECT_EQ(tokens[1].kind, TokenKind::Less);
    // "===" -> "==" followed by "="
    EXPECT_EQ(tokens[2].kind, TokenKind::EqualEqual);
    EXPECT_EQ(tokens[3].kind, TokenKind::Equal);
    // "!==" -> "!=" followed by "="
    EXPECT_EQ(tokens[4].kind, TokenKind::ExclaimEqual);
    EXPECT_EQ(tokens[5].kind, TokenKind::Equal);
    // ">>=" -> ">>" followed by "="
    EXPECT_EQ(tokens[6].kind, TokenKind::GreaterGreater);
    EXPECT_EQ(tokens[7].kind, TokenKind::Equal);
    EXPECT_FALSE(diag.hasErrors());
}

// 24. Realistic kernel snippet (saxpy)
TEST_F(LexerTest, RealisticKernelSnippet) {
    const char* saxpy = R"(
        kernel saxpy(i32 n, f32 a, f32* x, f32* y) {
            i32 tid = tid_x();
            if (tid < n) {
                y[tid] = a * x[tid] + y[tid];
            }
        }
    )";
    auto tokens = tokenize(saxpy);
    EXPECT_FALSE(diag.hasErrors());
    EXPECT_GE(tokens.size(), 30);
    EXPECT_EQ(tokens[0].kind, TokenKind::KwKernel);
    EXPECT_EQ(tokens[1].text, "saxpy");
    EXPECT_EQ(tokens[2].kind, TokenKind::LParen);
}

// 25. Negative: Unterminated block comment
TEST_F(LexerTest, NegativeUnterminatedBlockComment) {
    auto tokens = tokenize("/* start of comment with no end");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_EQ(diag.errorCount(), 1);
    EXPECT_EQ(diag.getDiagnostics()[0].location.line, 1);
    EXPECT_EQ(diag.getDiagnostics()[0].location.column, 1);
    EXPECT_NE(diag.getDiagnostics()[0].message.find("unterminated block comment"), std::string::npos);
}

// 26. Negative: Unexpected character
TEST_F(LexerTest, NegativeUnexpectedCharacter) {
    auto tokens = tokenize("i32 @x = 5;");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_EQ(diag.errorCount(), 1);
    EXPECT_EQ(diag.getDiagnostics()[0].location.line, 1);
    EXPECT_EQ(diag.getDiagnostics()[0].location.column, 5); // '@' at column 5
    EXPECT_NE(diag.getDiagnostics()[0].message.find("unexpected character '@'"), std::string::npos);
}

// 27. Negative: Hex literal with no digits
TEST_F(LexerTest, NegativeHexLiteralNoDigits) {
    auto tokens = tokenize("0x;");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_EQ(diag.errorCount(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Invalid);
}

// 28. Negative: Floating point exponent with no digits
TEST_F(LexerTest, NegativeFloatExponentNoDigits) {
    auto tokens = tokenize("1.0e+ ;");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_EQ(diag.errorCount(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::Invalid);
}

// 29. Negative: Integer out of 32-bit range
TEST_F(LexerTest, NegativeIntOverflow) {
    auto tokens = tokenize("999999999999999999999999");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_EQ(diag.errorCount(), 1);
    EXPECT_NE(diag.getDiagnostics()[0].message.find("exceeds 32-bit range"), std::string::npos);
}
