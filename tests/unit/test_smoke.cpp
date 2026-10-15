#include <gtest/gtest.h>
#include "ptxlab/version.h"

TEST(SmokeTest, FrameworkSanity) {
    EXPECT_TRUE(true);
    EXPECT_EQ(1 + 1, 2);
}

TEST(SmokeTest, VersionIntegrity) {
    EXPECT_EQ(ptxlab::VERSION_MAJOR, 0);
    EXPECT_EQ(ptxlab::VERSION_MINOR, 1);
    EXPECT_EQ(ptxlab::VERSION_PATCH, 0);
    EXPECT_EQ(ptxlab::getVersion(), "0.1.0");
    EXPECT_FALSE(ptxlab::getBanner().empty());
}
