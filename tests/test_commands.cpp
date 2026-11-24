#include "miniredis/commands.h"

#include <gtest/gtest.h>

using namespace miniredis;

TEST(CommandsTest, SetStoresValueAndReplies) {
    Store s;
    EXPECT_EQ(SetCommand("k", "v").execute(s), "OK");
    EXPECT_EQ(*s.get("k"), "v");
}

TEST(CommandsTest, GetReturnsValueOrNil) {
    Store s;
    EXPECT_EQ(GetCommand("k").execute(s), "(nil)");
    s.set("k", "v");
    EXPECT_EQ(GetCommand("k").execute(s), "v");
}

TEST(CommandsTest, DelReturnsOneWhenKeyExisted) {
    Store s;
    s.set("k", "v");
    EXPECT_EQ(DelCommand("k").execute(s), "1");
    EXPECT_EQ(DelCommand("k").execute(s), "0");
}
