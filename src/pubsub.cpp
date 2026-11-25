#include "miniredis/pubsub.h"

#include <algorithm>

namespace miniredis {

namespace {

// Drops expired entries and the entry of @p target (if given).
void prune(std::vector<std::weak_ptr<Subscriber>>& list, const Subscriber* target) {
    list.erase(std::remove_if(list.begin(), list.end(),
                              [target](const std::weak_ptr<Subscriber>& weak) {
                                  auto strong = weak.lock();
                                  return !strong || strong.get() == target;
                              }),
               list.end());
}

}  // namespace

void Broker::subscribe(const std::string& channel, const std::shared_ptr<Subscriber>& subscriber) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& list = channels_[channel];
    for (const auto& weak : list) {
        if (weak.lock() == subscriber) {
            return;
        }
    }
    list.push_back(subscriber);
}

void Broker::unsubscribe(const std::string& channel, const Subscriber& subscriber) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(channel);
    if (it == channels_.end()) {
        return;
    }
    prune(it->second, &subscriber);
    if (it->second.empty()) {
        channels_.erase(it);
    }
}

void Broker::unsubscribeAll(const Subscriber& subscriber) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = channels_.begin(); it != channels_.end();) {
        prune(it->second, &subscriber);
        it = it->second.empty() ? channels_.erase(it) : std::next(it);
    }
}

std::size_t Broker::publish(const std::string& channel, const std::string& message) {
    std::vector<std::shared_ptr<Subscriber>> receivers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(channel);
        if (it == channels_.end()) {
            return 0;
        }
        prune(it->second, nullptr);
        for (const auto& weak : it->second) {
            // A subscriber may still be destroyed concurrently, so lock() can yield null.
            if (auto strong = weak.lock()) {
                receivers.push_back(std::move(strong));
            }
        }
        if (it->second.empty()) {
            channels_.erase(it);
        }
    }
    // Delivered outside the lock: a slow subscriber must not block the broker.
    for (const auto& receiver : receivers) {
        receiver->onMessage(channel, message);
    }
    return receivers.size();
}

std::size_t Broker::subscriberCount(const std::string& channel) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = channels_.find(channel);
    if (it == channels_.end()) {
        return 0;
    }
    std::size_t live = 0;
    for (const auto& weak : it->second) {
        if (!weak.expired()) {
            ++live;
        }
    }
    return live;
}

}  // namespace miniredis
