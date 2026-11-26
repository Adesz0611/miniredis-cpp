#include "miniredis/policy_factory.h"

#include "miniredis/lfu_policy.h"
#include "miniredis/lru_policy.h"
#include "miniredis/text.h"

#include <stdexcept>

namespace miniredis {

std::unique_ptr<EvictionPolicy> makeEvictionPolicy(const std::string& name) {
    const std::string key = toUpper(name);
    if (key == "LRU") {
        return std::make_unique<LruPolicy>();
    }
    if (key == "LFU") {
        return std::make_unique<LfuPolicy>();
    }
    throw std::invalid_argument("unknown eviction policy '" + name + "'");
}

}  // namespace miniredis
