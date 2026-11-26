#pragma once

#include "miniredis/eviction_policy.h"

#include <memory>
#include <string>

namespace miniredis {

/// Creates the policy with the given name ("lru" or "lfu", case-insensitive).
/// Throws std::invalid_argument for unknown names.
std::unique_ptr<EvictionPolicy> makeEvictionPolicy(const std::string& name);

}  // namespace miniredis
