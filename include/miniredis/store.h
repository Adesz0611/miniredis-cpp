#pragma once

#include "miniredis/clock.h"
#include "miniredis/eviction_policy.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace miniredis {

/// Thread-safe in-memory key-value storage with optional per-key expiry.
class Store {
public:
    /// The clock must outlive the store.
    explicit Store(const Clock& clock = SystemClock::instance());

    /// Store holding at most @p capacity keys (0 means unlimited). When it is full, the
    /// policy picks the key to evict. A non-zero capacity requires a policy.
    Store(std::size_t capacity, std::unique_ptr<EvictionPolicy> policy,
          const Clock& clock = SystemClock::instance());

    /// Overwrites the value and clears any expiry on the key.
    void set(const std::string& key, std::string value);
    std::optional<std::string> get(const std::string& key) const;

    /// Returns true if the key existed.
    bool del(const std::string& key);

    /// Makes the key expire after @p ttl. Returns false if the key does not exist.
    bool expire(const std::string& key, std::chrono::milliseconds ttl);

    /// Drops all expired keys and returns how many were removed.
    std::size_t removeExpired();

    struct SnapshotItem {
        std::string key;
        std::string value;
        std::optional<std::chrono::milliseconds> ttl;  // time left, if the key expires
    };

    /// Consistent copy of all live keys, in unspecified order.
    std::vector<SnapshotItem> snapshot() const;

    /// Number of live (not expired) keys.
    std::size_t size() const;

private:
    struct Entry {
        std::string value;
        std::optional<Clock::TimePoint> expiresAt;
    };

    bool isExpired(const Entry& entry) const;
    std::size_t removeExpiredLocked();
    void makeRoom();

    const Clock& clock_;
    std::size_t capacity_ = 0;
    std::unique_ptr<EvictionPolicy> policy_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> data_;
};

}  // namespace miniredis
