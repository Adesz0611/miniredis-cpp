#include "miniredis/command_factory.h"

#include "miniredis/commands.h"
#include "miniredis/text.h"

#include <charconv>

namespace miniredis {

namespace {

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

std::string joinWords(const CommandFactory::Args& args, std::size_t from) {
    std::string result = args[from];
    for (std::size_t i = from + 1; i < args.size(); ++i) {
        result += ' ';
        result += args[i];
    }
    return result;
}

}  // namespace

CommandFactory::CommandFactory() {
    registerCommand("SET", [](const Args& a) {
        if (a.size() < 2) {
            throw ParseError("wrong number of arguments for SET");
        }
        return std::make_unique<SetCommand>(a[0], joinWords(a, 1));
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

void CommandFactory::setDecorator(Decorator decorator) { decorator_ = std::move(decorator); }

std::unique_ptr<Command> CommandFactory::create(const std::string& line) const {
    auto words = splitWords(line);
    if (words.empty()) {
        throw ParseError("empty command");
    }

    auto it = creators_.find(toUpper(words.front()));
    if (it == creators_.end()) {
        throw ParseError("unknown command '" + words.front() + "'");
    }
    auto command = it->second(Args(words.begin() + 1, words.end()));
    if (decorator_) {
        return decorator_(words, std::move(command));
    }
    return command;
}

}  // namespace miniredis
