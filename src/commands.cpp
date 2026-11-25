#include "miniredis/commands.h"

#include "miniredis/command_factory.h"
#include "miniredis/pubsub.h"

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

ExpireCommand::ExpireCommand(std::string key, std::chrono::seconds ttl)
    : key_(std::move(key)), ttl_(ttl) {}

std::string ExpireCommand::execute(Store& store) const {
    return store.expire(key_, ttl_) ? "1" : "0";
}

PublishCommand::PublishCommand(Broker& broker, std::string channel, std::string message)
    : broker_(broker), channel_(std::move(channel)), message_(std::move(message)) {}

std::string PublishCommand::execute(Store&) const {
    return std::to_string(broker_.publish(channel_, message_));
}

void registerPublish(CommandFactory& factory, Broker& broker) {
    factory.registerCommand("PUBLISH", [&broker](const CommandFactory::Args& args) {
        if (args.size() < 2) {
            throw ParseError("wrong number of arguments for PUBLISH");
        }
        std::string message = args[1];
        for (std::size_t i = 2; i < args.size(); ++i) {
            message += ' ';
            message += args[i];
        }
        return std::make_unique<PublishCommand>(broker, args[0], std::move(message));
    });
}

}  // namespace miniredis
