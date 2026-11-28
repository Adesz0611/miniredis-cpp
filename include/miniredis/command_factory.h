#pragma once

#include "miniredis/command.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace miniredis {

class ParseError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// Builds Command objects from protocol lines such as "SET key value".
class CommandFactory {
public:
    using Args = std::vector<std::string>;
    using Creator = std::function<std::unique_ptr<Command>(const Args& args)>;

    /// Receives the words of the line (name first) and the created command, and returns the
    /// command to run, either unchanged or wrapped.
    using Decorator = std::function<std::unique_ptr<Command>(const Args& words,
                                                             std::unique_ptr<Command> command)>;

    /// Registers the built-in commands.
    CommandFactory();

    /// Adds or replaces a command. The name is case-insensitive.
    void registerCommand(const std::string& name, Creator creator);

    /// Applied to every command that is created afterwards. Set it before serving requests.
    void setDecorator(Decorator decorator);

    /// Throws ParseError for empty lines, unknown commands and wrong argument counts.
    std::unique_ptr<Command> create(const std::string& line) const;

private:
    std::unordered_map<std::string, Creator> creators_;
    Decorator decorator_;
};

}  // namespace miniredis
