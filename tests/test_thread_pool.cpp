#include "miniredis/thread_pool.h"

#include <atomic>
#include <chrono>
#include <future>

#include <gtest/gtest.h>

using miniredis::ThreadPool;

TEST(ThreadPoolTest, DestructorRunsAllQueuedTasks) {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 100; ++i) {
            pool.submit([&counter] { ++counter; });
        }
    }
    EXPECT_EQ(counter, 100);
}

TEST(ThreadPoolTest, TasksRunConcurrently) {
    ThreadPool pool(2);
    std::promise<void> first;
    std::promise<void> second;
    auto firstDone = first.get_future();
    auto secondDone = second.get_future();
    std::promise<bool> result;

    // Each task waits for the other one, so this only finishes if both run at the same time.
    pool.submit([&] {
        first.set_value();
        result.set_value(secondDone.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    });
    pool.submit([&] {
        firstDone.wait();
        second.set_value();
    });
    EXPECT_TRUE(result.get_future().get());
}

TEST(ThreadPoolTest, SingleThreadPreservesSubmissionOrder) {
    std::vector<int> order;
    {
        ThreadPool pool(1);
        for (int i = 0; i < 20; ++i) {
            pool.submit([&order, i] { order.push_back(i); });
        }
    }
    ASSERT_EQ(order.size(), 20u);
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(order[static_cast<std::size_t>(i)], i);
    }
}
