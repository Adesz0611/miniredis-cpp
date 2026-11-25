#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace miniredis {

/// Fixed-size pool of worker threads running queued tasks in FIFO order.
/// Tasks must not throw. The destructor runs all queued tasks, then joins the workers.
class ThreadPool {
public:
    explicit ThreadPool(std::size_t threads);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submit(std::function<void()> task);

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable wakeup_;
    std::queue<std::function<void()>> tasks_;
    bool stopping_ = false;
};

}  // namespace miniredis
