#include "miniredis/command_factory.h"

#include "miniredis/commands.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>

namespace miniredis {

namespace {

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

void requireArgs(const CommandFactory::Args& args, std::size_t expected, const char* name) {
    if (args.size() != expected) {
        throw ParseError(std::string("wrong number of arguments for ") + name);
    }
}

std::chrono::seconds parseSeconds(const std::string& text) {
    long long value = 0;
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc() || end != text.data() + text.size()) {
        throw ParseError("value is not an integer");
    }
    if (value <= 0) {
        throw ParseError("invalid expire time");
    }
    return std::chrono::seconds(value);
}

}  // namespace

CommandFactory::CommandFactory() {
    registerCommand("SET", [](const Args& a) {
        requireArgs(a, 2, "SET");
        return std::make_unique<SetCommand>(a[0], a[1]);
    });
    registerCommand("GET", [](const Args& a) {
        requireArgs(a, 1, "GET");
        return std::make_unique<GetCommand>(a[0]);
    });
    registerCommand("EXPIRE", [](const Args& a) {
        requireArgs(a, 2, "EXPIRE");
        return std::make_unique<ExpireCommand>(a[0], parseSeconds(a[1]));
    });
    registerCommand("DEL", [](const Args& a) {
        requireArgs(a, 1, "DEL");
        return std::make_unique<DelCommand>(a[0]);
    });
}

void CommandFactory::registerCommand(const std::string& name, Creator creator) {
    creators_[toUpper(name)] = std::move(creator);
}

std::unique_ptr<Command> CommandFactory::create(const std::string& line) const {
    std::istringstream in(line);
    std::string name;
    if (!(in >> name)) {
        throw ParseError("empty command");
    }

    auto it = creators_.find(toUpper(name));
    if (it == creators_.end()) {
        throw ParseError("unknown command '" + name + "'");
    }

    Args args;
    for (std::string token; in >> token;) {
        args.push_back(std::move(token));
    }
    return it->second(args);
}

}  // namespace miniredis
