#include "miniredis/store.h"

namespace miniredis {

Store::Store(const Clock& clock) : clock_(clock) {}

bool Store::isExpired(const Entry& entry) const {
    return entry.expiresAt && *entry.expiresAt <= clock_.now();
}

void Store::set(const std::string& key, std::string value) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_[key] = Entry{std::move(value), std::nullopt};
}

std::optional<std::string> Store::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end() || isExpired(it->second)) {
        return std::nullopt;
    }
    return it->second.value;
}

bool Store::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }
    bool wasLive = !isExpired(it->second);
    data_.erase(it);
    return wasLive;
}

bool Store::expire(const std::string& key, std::chrono::milliseconds ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end() || isExpired(it->second)) {
        return false;
    }
    it->second.expiresAt = clock_.now() + ttl;
    return true;
}

std::size_t Store::removeExpired() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t removed = 0;
    for (auto it = data_.begin(); it != data_.end();) {
        if (isExpired(it->second)) {
            it = data_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

std::size_t Store::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t live = 0;
    for (const auto& kv : data_) {
        if (!isExpired(kv.second)) {
            ++live;
        }
    }
    return live;
}

}  // namespace miniredis
