#pragma once

#include <optional>
#include <string>

namespace miniredis {

/// Decides which key to evict when the store is full.
/// The store calls these methods with its lock held, so implementations need no locking.
class EvictionPolicy {
public:
    virtual ~EvictionPolicy() = default;

    virtual void onInsert(const std::string& key) = 0;
    virtual void onAccess(const std::string& key) = 0;
    virtual void onRemove(const std::string& key) = 0;

    /// The key that should be evicted next, or nullopt if no keys are tracked.
    virtual std::optional<std::string> victim() const = 0;
};

}  // namespace miniredis
