#include <gtest/gtest.h>
#include "ptxlab/support/diagnostic.h"
#include "ptxlab/support/source_manager.h"

using namespace ptxlab;

TEST(DiagnosticTest, BasicReportingAndCounts) {
    DiagnosticEngine diag;
    EXPECT_FALSE(diag.hasErrors());
    EXPECT_EQ(diag.errorCount(), 0);
    EXPECT_EQ(diag.warningCount(), 0);

    diag.warning({1, 5}, "unused variable 'x'");
    EXPECT_FALSE(diag.hasErrors());
    EXPECT_EQ(diag.warningCount(), 1);

    diag.error({2, 10}, "unexpected token");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_EQ(diag.errorCount(), 1);

    diag.note({2, 10}, "consider adding a semicolon");
    EXPECT_EQ(diag.getDiagnostics().size(), 3);

    diag.clear();
    EXPECT_FALSE(diag.hasErrors());
    EXPECT_EQ(diag.errorCount(), 0);
    EXPECT_EQ(diag.warningCount(), 0);
    EXPECT_TRUE(diag.getDiagnostics().empty());
}

TEST(DiagnosticTest, FormattingWithCaret) {
    SourceManager sm("kernel.kc", "kernel add(i32 a) {\nlet x = @ + 1;\n}");
    DiagnosticEngine diag;

    Diagnostic d{DiagnosticLevel::Error, {2, 9}, "unexpected character '@'"};
    std::string formatted = diag.format(d, sm);

    std::string expected = 
        "kernel.kc:2:9: error: unexpected character '@'\n"
        "  let x = @ + 1;\n"
        "          ^\n";

    EXPECT_EQ(formatted, expected);
}

TEST(DiagnosticTest, FormattingWithoutSourceLocation) {
    SourceManager sm("kernel.kc", "");
    DiagnosticEngine diag;

    Diagnostic d{DiagnosticLevel::Error, {0, 0}, "cannot open file"};
    std::string formatted = diag.format(d, sm);

    std::string expected = "kernel.kc: error: cannot open file\n";
    EXPECT_EQ(formatted, expected);
}
