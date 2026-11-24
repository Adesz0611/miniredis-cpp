#include "miniredis/commands.h"

#include <utility>

namespace miniredis {

SetCommand::SetCommand(std::string key, std::string value)
    : key_(std::move(key)), value_(std::move(value)) {}

std::string SetCommand::execute(Store& store) const {
    store.set(key_, value_);
    return "OK";
}

GetCommand::GetCommand(std::string key) : key_(std::move(key)) {}

std::string GetCommand::execute(Store& store) const {
    auto value = store.get(key_);
    return value ? *value : "(nil)";
}

DelCommand::DelCommand(std::string key) : key_(std::move(key)) {}

std::string DelCommand::execute(Store& store) const { return store.del(key_) ? "1" : "0"; }

}  // namespace miniredis
