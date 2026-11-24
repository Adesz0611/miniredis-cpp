#include "miniredis/lfu_policy.h"
#include "miniredis/lru_policy.h"
#include "miniredis/store.h"

#include <memory>

#include "manual_clock.h"
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

TEST(StoreEvictionTest, CapacityWithoutPolicyIsRejected) {
    EXPECT_THROW(Store(3, nullptr), std::invalid_argument);
}

TEST(StoreEvictionTest, LruEvictsLeastRecentlyUsedKey) {
    Store s(2, std::make_unique<LruPolicy>());
    s.set("a", "1");
    s.set("b", "2");
    s.get("a");
    s.set("c", "3");
    EXPECT_TRUE(s.get("a").has_value());
    EXPECT_FALSE(s.get("b").has_value());
    EXPECT_TRUE(s.get("c").has_value());
    EXPECT_EQ(s.size(), 2u);
}

TEST(StoreEvictionTest, LfuEvictsLeastFrequentlyUsedKey) {
    Store s(2, std::make_unique<LfuPolicy>());
    s.set("a", "1");
    s.set("b", "2");
    s.get("a");
    s.get("a");
    s.get("b");
    s.set("c", "3");
    EXPECT_TRUE(s.get("a").has_value());
    EXPECT_FALSE(s.get("b").has_value());
    EXPECT_TRUE(s.get("c").has_value());
}

TEST(StoreEvictionTest, OverwritingExistingKeyDoesNotEvict) {
    Store s(2, std::make_unique<LruPolicy>());
    s.set("a", "1");
    s.set("b", "2");
    s.set("a", "3");
    EXPECT_EQ(s.size(), 2u);
    EXPECT_TRUE(s.get("b").has_value());
}

TEST(StoreEvictionTest, ExpiredKeysAreDroppedBeforeLiveOnesAreEvicted) {
    ManualClock clock;
    Store s(2, std::make_unique<LruPolicy>(), clock);
    s.set("a", "1");
    s.set("b", "2");
    s.expire("b", std::chrono::seconds(1));
    s.get("b");
    s.get("a");
    clock.advance(std::chrono::seconds(1));
    s.set("c", "3");
    EXPECT_TRUE(s.get("a").has_value());
    EXPECT_TRUE(s.get("c").has_value());
}

TEST(StoreEvictionTest, DeletedKeysFreeCapacity) {
    Store s(2, std::make_unique<LruPolicy>());
    s.set("a", "1");
    s.set("b", "2");
    s.del("a");
    s.set("c", "3");
    EXPECT_TRUE(s.get("b").has_value());
    EXPECT_TRUE(s.get("c").has_value());
}

TEST(StoreEvictionTest, UnlimitedStoreNeverEvicts) {
    Store s;
    for (int i = 0; i < 1000; ++i) {
        s.set("k" + std::to_string(i), "v");
    }
    EXPECT_EQ(s.size(), 1000u);
}
