#include "miniredis/lfu_policy.h"

namespace miniredis {

void LfuPolicy::onInsert(const std::string& key) {
    if (stats_.count(key) > 0) {
        onAccess(key);
        return;
    }
    Stats stats{1, ++tick_};
    stats_[key] = stats;
    ranking_.emplace(stats.frequency, stats.lastUsed, key);
}

void LfuPolicy::onAccess(const std::string& key) {
    auto it = stats_.find(key);
    if (it == stats_.end()) {
        return;
    }
    Stats& stats = it->second;
    ranking_.erase(Rank{stats.frequency, stats.lastUsed, key});
    ++stats.frequency;
    stats.lastUsed = ++tick_;
    ranking_.emplace(stats.frequency, stats.lastUsed, key);
}

void LfuPolicy::onRemove(const std::string& key) {
    auto it = stats_.find(key);
    if (it == stats_.end()) {
        return;
    }
    ranking_.erase(Rank{it->second.frequency, it->second.lastUsed, key});
    stats_.erase(it);
}

std::optional<std::string> LfuPolicy::victim() const {
    if (ranking_.empty()) {
        return std::nullopt;
    }
    return std::get<2>(*ranking_.begin());
}

}  // namespace miniredis
