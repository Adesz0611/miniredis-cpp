#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace miniredis {

/// Thread-safe in-memory key-value storage.
class Store {
public:
    void set(const std::string& key, std::string value);
    std::optional<std::string> get(const std::string& key) const;

    /// Returns true if the key existed.
    bool del(const std::string& key);

    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> data_;
};

}  // namespace miniredis
