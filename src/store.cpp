#include "miniredis/store.h"

#include <stdexcept>

namespace miniredis {

Store::Store(const Clock& clock) : clock_(clock) {}

Store::Store(std::size_t capacity, std::unique_ptr<EvictionPolicy> policy, const Clock& clock)
    : clock_(clock), capacity_(capacity), policy_(std::move(policy)) {
    if (capacity_ > 0 && !policy_) {
        throw std::invalid_argument("a capacity limit requires an eviction policy");
    }
}

bool Store::isExpired(const Entry& entry) const {
    return entry.expiresAt && *entry.expiresAt <= clock_.now();
}

void Store::set(const std::string& key, std::string value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);
    if (it != data_.end()) {
        it->second = Entry{std::move(value), std::nullopt};
        if (policy_) {
            policy_->onAccess(key);
        }
        return;
    }
    makeRoom();
    data_.emplace(key, Entry{std::move(value), std::nullopt});
    if (policy_) {
        policy_->onInsert(key);
    }
}

std::optional<std::string> Store::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end() || isExpired(it->second)) {
        return std::nullopt;
    }
    if (policy_) {
        policy_->onAccess(key);
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
    if (policy_) {
        policy_->onRemove(key);
    }
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
    return removeExpiredLocked();
}

std::size_t Store::removeExpiredLocked() {
    std::size_t removed = 0;
    for (auto it = data_.begin(); it != data_.end();) {
        if (isExpired(it->second)) {
            if (policy_) {
                policy_->onRemove(it->first);
            }
            it = data_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

void Store::makeRoom() {
    if (capacity_ == 0 || data_.size() < capacity_) {
        return;
    }
    removeExpiredLocked();
    while (data_.size() >= capacity_) {
        auto victim = policy_->victim();
        if (!victim) {
            return;
        }
        data_.erase(*victim);
        policy_->onRemove(*victim);
    }
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
