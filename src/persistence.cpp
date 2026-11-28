#include "miniredis/persistence.h"

#include "miniredis/text.h"

namespace miniredis {

namespace {

class CompactCommand : public Command {
public:
    explicit CompactCommand(AppendOnlyLog& log) : log_(log) {}

    std::string execute(Store& store) const override {
        log_.rewrite(store);
        return "OK";
    }

private:
    AppendOnlyLog& log_;
};

std::string join(const std::vector<std::string>& words) {
    std::string line;
    for (const auto& word : words) {
        if (!line.empty()) {
            line += ' ';
        }
        line += word;
    }
    return line;
}

}  // namespace

LoggedCommand::LoggedCommand(std::unique_ptr<Command> inner, std::string line, AppendOnlyLog& log)
    : inner_(std::move(inner)), line_(std::move(line)), log_(log) {}

std::string LoggedCommand::execute(Store& store) const {
    return log_.execute(line_, *inner_, store);
}

void registerPersistence(CommandFactory& factory, AppendOnlyLog& log) {
    factory.setDecorator([&log](const CommandFactory::Args& words,
                                std::unique_ptr<Command> command) -> std::unique_ptr<Command> {
        const std::string name = toUpper(words.front());
        if (name == "SET" || name == "DEL" || name == "EXPIRE") {
            return std::make_unique<LoggedCommand>(std::move(command), join(words), log);
        }
        return command;
    });

    factory.registerCommand("COMPACT", [&log](const CommandFactory::Args& args) {
        if (!args.empty()) {
            throw ParseError("wrong number of arguments for COMPACT");
        }
        return std::make_unique<CompactCommand>(log);
    });
}

ReplayResult replayLog(const std::vector<std::string>& lines, Store& store) {
    const CommandFactory factory;
    ReplayResult result;
    for (const auto& line : lines) {
        try {
            factory.create(line)->execute(store);
            ++result.applied;
        } catch (const ParseError&) {
            ++result.skipped;
        }
    }
    return result;
}

}  // namespace miniredis
