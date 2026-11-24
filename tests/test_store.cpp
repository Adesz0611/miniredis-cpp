#include "miniredis/store.h"

#include <thread>
#include <vector>

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
