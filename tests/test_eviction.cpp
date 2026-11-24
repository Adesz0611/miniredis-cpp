#include "miniredis/lfu_policy.h"
#include "miniredis/lru_policy.h"

#include <gtest/gtest.h>

using namespace miniredis;

TEST(LruPolicyTest, EmptyPolicyHasNoVictim) {
    LruPolicy p;
    EXPECT_FALSE(p.victim().has_value());
}

TEST(LruPolicyTest, VictimIsLeastRecentlyUsed) {
    LruPolicy p;
    p.onInsert("a");
    p.onInsert("b");
    p.onInsert("c");
    EXPECT_EQ(*p.victim(), "a");
    p.onAccess("a");
    EXPECT_EQ(*p.victim(), "b");
}

TEST(LruPolicyTest, RemovedKeyIsNoLongerVictim) {
    LruPolicy p;
    p.onInsert("a");
    p.onInsert("b");
    p.onRemove("a");
    EXPECT_EQ(*p.victim(), "b");
    p.onRemove("b");
    EXPECT_FALSE(p.victim().has_value());
}

TEST(LruPolicyTest, ReinsertingExistingKeyDoesNotDuplicateIt) {
    LruPolicy p;
    p.onInsert("a");
    p.onInsert("b");
    p.onInsert("a");
    EXPECT_EQ(*p.victim(), "b");
    p.onRemove("b");
    p.onRemove("a");
    EXPECT_FALSE(p.victim().has_value());
}

TEST(LfuPolicyTest, VictimIsLeastFrequentlyUsed) {
    LfuPolicy p;
    p.onInsert("a");
    p.onInsert("b");
    p.onAccess("a");
    p.onAccess("a");
    p.onAccess("b");
    p.onInsert("c");
    EXPECT_EQ(*p.victim(), "c");
}

TEST(LfuPolicyTest, TiesAreBrokenByOldestAccess) {
    LfuPolicy p;
    p.onInsert("a");
    p.onInsert("b");
    EXPECT_EQ(*p.victim(), "a");
    p.onAccess("a");
    p.onAccess("b");
    EXPECT_EQ(*p.victim(), "a");
}

TEST(LfuPolicyTest, RemovedKeyIsNoLongerVictim) {
    LfuPolicy p;
    p.onInsert("a");
    p.onInsert("b");
    p.onRemove("a");
    EXPECT_EQ(*p.victim(), "b");
}
