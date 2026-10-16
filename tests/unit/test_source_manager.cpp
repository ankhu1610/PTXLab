#include <gtest/gtest.h>
#include "ptxlab/support/source_manager.h"

using namespace ptxlab;

TEST(SourceManagerTest, EmptySource) {
    SourceManager sm("empty.kc", "");
    EXPECT_EQ(sm.getFilename(), "empty.kc");
    EXPECT_EQ(sm.getSource(), "");
    EXPECT_EQ(sm.getLineCount(), 1);
    
    SourceLocation loc = sm.getLocation(0);
    EXPECT_EQ(loc.line, 1);
    EXPECT_EQ(loc.column, 1);
    EXPECT_EQ(sm.getLineContent(1), "");
}

TEST(SourceManagerTest, SingleLineSource) {
    std::string code = "kernel foo() {}";
    SourceManager sm("single.kc", code);
    EXPECT_EQ(sm.getLineCount(), 1);

    SourceLocation loc0 = sm.getLocation(0);
    EXPECT_EQ(loc0.line, 1);
    EXPECT_EQ(loc0.column, 1);

    SourceLocation loc7 = sm.getLocation(7); // 'f'
    EXPECT_EQ(loc7.line, 1);
    EXPECT_EQ(loc7.column, 8);

    EXPECT_EQ(sm.getLineContent(1), code);
    EXPECT_EQ(sm.getLineContent(2), "");
}

TEST(SourceManagerTest, MultiLineLF) {
    std::string code = "line 1\nline 22\nline 333";
    SourceManager sm("multi_lf.kc", code);
    EXPECT_EQ(sm.getLineCount(), 3);

    // Line 1: "line 1" (indices 0..5, '\n' at 6)
    EXPECT_EQ(sm.getLocation(0), (SourceLocation{1, 1}));
    EXPECT_EQ(sm.getLocation(5), (SourceLocation{1, 6}));

    // Line 2: "line 22" (index 7 begins line 2)
    EXPECT_EQ(sm.getLocation(7), (SourceLocation{2, 1}));
    EXPECT_EQ(sm.getLocation(13), (SourceLocation{2, 7}));

    // Line 3: "line 333" (index 15 begins line 3)
    EXPECT_EQ(sm.getLocation(15), (SourceLocation{3, 1}));

    EXPECT_EQ(sm.getLineContent(1), "line 1");
    EXPECT_EQ(sm.getLineContent(2), "line 22");
    EXPECT_EQ(sm.getLineContent(3), "line 333");
}

TEST(SourceManagerTest, MultiLineCRLF) {
    std::string code = "first\r\nsecond\r\nthird";
    SourceManager sm("crlf.kc", code);
    EXPECT_EQ(sm.getLineCount(), 3);

    EXPECT_EQ(sm.getLineContent(1), "first");
    EXPECT_EQ(sm.getLineContent(2), "second");
    EXPECT_EQ(sm.getLineContent(3), "third");
}

TEST(SourceManagerTest, OutOfBoundsOffsetClamping) {
    SourceManager sm("bounds.kc", "abc");
    SourceLocation loc = sm.getLocation(999);
    EXPECT_EQ(loc.line, 1);
    EXPECT_EQ(loc.column, 4);
}
