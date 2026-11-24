#pragma once

#include "miniredis/eviction_policy.h"

#include <cstdint>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>

namespace miniredis {

/// Evicts the least frequently used key; ties are broken by evicting the oldest access.
class LfuPolicy : public EvictionPolicy {
public:
    void onInsert(const std::string& key) override;
    void onAccess(const std::string& key) override;
    void onRemove(const std::string& key) override;
    std::optional<std::string> victim() const override;

private:
    struct Stats {
        std::uint64_t frequency;
        std::uint64_t lastUsed;
    };
    using Rank = std::tuple<std::uint64_t, std::uint64_t, std::string>;

    std::uint64_t tick_ = 0;
    std::unordered_map<std::string, Stats> stats_;
    std::set<Rank> ranking_;  // lowest frequency, then oldest, first
};

}  // namespace miniredis
