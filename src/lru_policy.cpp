#include "miniredis/lru_policy.h"

namespace miniredis {

void LruPolicy::onInsert(const std::string& key) {
    if (position_.count(key) > 0) {
        onAccess(key);
        return;
    }
    order_.push_front(key);
    position_[key] = order_.begin();
}

void LruPolicy::onAccess(const std::string& key) {
    auto it = position_.find(key);
    if (it != position_.end()) {
        order_.splice(order_.begin(), order_, it->second);
    }
}

void LruPolicy::onRemove(const std::string& key) {
    auto it = position_.find(key);
    if (it != position_.end()) {
        order_.erase(it->second);
        position_.erase(it);
    }
}

std::optional<std::string> LruPolicy::victim() const {
    if (order_.empty()) {
        return std::nullopt;
    }
    return order_.back();
}

}  // namespace miniredis
