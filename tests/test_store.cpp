#include "miniredis/store.h"

#include <algorithm>
#include <thread>
#include <vector>

#include "manual_clock.h"
#include <gtest/gtest.h>

using miniredis::Store;

TEST(StoreTest, GetMissingKeyReturnsNullopt) {
    Store s;
    EXPECT_FALSE(s.get("nope").has_value());
}

TEST(StoreTest, SetThenGet) {
    Store s;
    s.set("k", "v");
    ASSERT_TRUE(s.get("k").has_value());
    EXPECT_EQ(*s.get("k"), "v");
}

TEST(StoreTest, SetOverwrites) {
    Store s;
    s.set("k", "a");
    s.set("k", "b");
    EXPECT_EQ(*s.get("k"), "b");
    EXPECT_EQ(s.size(), 1u);
}

TEST(StoreTest, DelReportsExistence) {
    Store s;
    s.set("k", "v");
    EXPECT_TRUE(s.del("k"));
    EXPECT_FALSE(s.del("k"));
    EXPECT_FALSE(s.get("k").has_value());
}

TEST(StoreTest, ConcurrentWritersDoNotLoseKeys) {
    Store s;
    constexpr int kThreads = 8;
    constexpr int kPerThread = 500;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&s, t] {
            for (int i = 0; i < kPerThread; ++i) {
                s.set("k" + std::to_string(t) + "_" + std::to_string(i), "x");
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(s.size(), static_cast<std::size_t>(kThreads * kPerThread));
}

TEST(StoreExpiryTest, KeyDisappearsAfterTtl) {
    ManualClock clock;
    Store s(clock);
    s.set("k", "v");
    ASSERT_TRUE(s.expire("k", std::chrono::seconds(10)));

    clock.advance(std::chrono::seconds(9));
    EXPECT_TRUE(s.get("k").has_value());

    clock.advance(std::chrono::seconds(1));
    EXPECT_FALSE(s.get("k").has_value());
}

TEST(StoreExpiryTest, ExpireOnMissingOrExpiredKeyFails) {
    ManualClock clock;
    Store s(clock);
    EXPECT_FALSE(s.expire("nope", std::chrono::seconds(1)));

    s.set("k", "v");
    s.expire("k", std::chrono::seconds(1));
    clock.advance(std::chrono::seconds(2));
    EXPECT_FALSE(s.expire("k", std::chrono::seconds(1)));
}

TEST(StoreExpiryTest, SetClearsExpiry) {
    ManualClock clock;
    Store s(clock);
    s.set("k", "v");
    s.expire("k", std::chrono::seconds(1));
    s.set("k", "w");
    clock.advance(std::chrono::seconds(5));
    EXPECT_EQ(*s.get("k"), "w");
}

TEST(StoreExpiryTest, SizeIgnoresExpiredKeys) {
    ManualClock clock;
    Store s(clock);
    s.set("a", "1");
    s.set("b", "2");
    s.expire("a", std::chrono::seconds(1));
    clock.advance(std::chrono::seconds(1));
    EXPECT_EQ(s.size(), 1u);
}

TEST(StoreExpiryTest, DelOfExpiredKeyReportsFalse) {
    ManualClock clock;
    Store s(clock);
    s.set("k", "v");
    s.expire("k", std::chrono::seconds(1));
    clock.advance(std::chrono::seconds(1));
    EXPECT_FALSE(s.del("k"));
}

TEST(StoreExpiryTest, RemoveExpiredCountsRemovedKeys) {
    ManualClock clock;
    Store s(clock);
    s.set("a", "1");
    s.set("b", "2");
    s.set("c", "3");
    s.expire("a", std::chrono::seconds(1));
    s.expire("b", std::chrono::seconds(1));
    clock.advance(std::chrono::seconds(2));
    EXPECT_EQ(s.removeExpired(), 2u);
    EXPECT_EQ(s.size(), 1u);
}

TEST(StoreSnapshotTest, ContainsLiveKeysWithTheirRemainingTtl) {
    ManualClock clock;
    Store s(clock);
    s.set("plain", "1");
    s.set("long", "2");
    s.set("short", "3");
    s.expire("long", std::chrono::seconds(10));
    s.expire("short", std::chrono::seconds(1));
    clock.advance(std::chrono::milliseconds(2500));

    auto items = s.snapshot();
    ASSERT_EQ(items.size(), 2u);
    std::sort(items.begin(), items.end(),
              [](const auto& a, const auto& b) { return a.key < b.key; });
    EXPECT_EQ(items[0].key, "long");
    EXPECT_EQ(items[0].ttl, std::chrono::milliseconds(7500));
    EXPECT_EQ(items[1].key, "plain");
    EXPECT_FALSE(items[1].ttl.has_value());
}

TEST(StoreSnapshotTest, IsACopyThatLaterChangesDoNotAffect) {
    Store s;
    s.set("k", "old");
    auto items = s.snapshot();
    s.set("k", "new");
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].value, "old");
}
