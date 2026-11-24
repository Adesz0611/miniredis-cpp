#pragma once

#include "miniredis/eviction_policy.h"

#include <list>
#include <string>
#include <unordered_map>

namespace miniredis {

/// Evicts the least recently used key.
class LruPolicy : public EvictionPolicy {
public:
    void onInsert(const std::string& key) override;
    void onAccess(const std::string& key) override;
    void onRemove(const std::string& key) override;
    std::optional<std::string> victim() const override;

private:
    std::list<std::string> order_;  // most recently used first
    std::unordered_map<std::string, std::list<std::string>::iterator> position_;
};

}  // namespace miniredis
