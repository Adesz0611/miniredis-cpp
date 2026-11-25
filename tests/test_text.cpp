#include "miniredis/text.h"

#include <gtest/gtest.h>

using namespace miniredis;

TEST(TextTest, ToUpper) {
    EXPECT_EQ(toUpper("sEt 1"), "SET 1");
    EXPECT_EQ(toUpper(""), "");
}

TEST(TextTest, SplitWordsCollapsesWhitespace) {
    EXPECT_EQ(splitWords("  SET \t a   1 "), (std::vector<std::string>{"SET", "a", "1"}));
}

TEST(TextTest, SplitWordsOfBlankLineIsEmpty) {
    EXPECT_TRUE(splitWords("").empty());
    EXPECT_TRUE(splitWords("  \t ").empty());
}
