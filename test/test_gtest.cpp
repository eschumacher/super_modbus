#include <gtest/gtest.h>

TEST(GTestTest, BasicAssertions) {
  EXPECT_STRNE("hello", "world");
  EXPECT_EQ(8 * 8, 64);
}
