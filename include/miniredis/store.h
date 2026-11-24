#pragma once

#include "miniredis/clock.h"

#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace miniredis {

/// Thread-safe in-memory key-value storage with optional per-key expiry.
class Store {
public:
    /// The clock must outlive the store.
    explicit Store(const Clock& clock = SystemClock::instance());

    /// Overwrites the value and clears any expiry on the key.
    void set(const std::string& key, std::string value);
    std::optional<std::string> get(const std::string& key) const;

    /// Returns true if the key existed.
    bool del(const std::string& key);

    /// Makes the key expire after @p ttl. Returns false if the key does not exist.
    bool expire(const std::string& key, std::chrono::milliseconds ttl);

    /// Drops all expired keys and returns how many were removed.
    std::size_t removeExpired();

    /// Number of live (not expired) keys.
    std::size_t size() const;

private:
    struct Entry {
        std::string value;
        std::optional<Clock::TimePoint> expiresAt;
    };

    bool isExpired(const Entry& entry) const;

    const Clock& clock_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> data_;
};

}  // namespace miniredis
