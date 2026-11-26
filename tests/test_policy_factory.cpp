#include "miniredis/lfu_policy.h"
#include "miniredis/lru_policy.h"
#include "miniredis/policy_factory.h"

#include <gtest/gtest.h>

using namespace miniredis;

TEST(PolicyFactoryTest, CreatesLru) {
    auto policy = makeEvictionPolicy("lru");
    EXPECT_NE(dynamic_cast<LruPolicy*>(policy.get()), nullptr);
}

TEST(PolicyFactoryTest, CreatesLfu) {
    auto policy = makeEvictionPolicy("lfu");
    EXPECT_NE(dynamic_cast<LfuPolicy*>(policy.get()), nullptr);
}

TEST(PolicyFactoryTest, NamesAreCaseInsensitive) {
    EXPECT_NE(dynamic_cast<LruPolicy*>(makeEvictionPolicy("LrU").get()), nullptr);
}

TEST(PolicyFactoryTest, UnknownNameThrows) {
    EXPECT_THROW(makeEvictionPolicy("fifo"), std::invalid_argument);
    EXPECT_THROW(makeEvictionPolicy(""), std::invalid_argument);
}
