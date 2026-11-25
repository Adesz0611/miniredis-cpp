#pragma once

#include "miniredis/command.h"

#include <chrono>
#include <string>

namespace miniredis {

class Broker;
class CommandFactory;

class SetCommand : public Command {
public:
    SetCommand(std::string key, std::string value);
    std::string execute(Store& store) const override;

private:
    std::string key_;
    std::string value_;
};

class GetCommand : public Command {
public:
    explicit GetCommand(std::string key);
    std::string execute(Store& store) const override;

private:
    std::string key_;
};

class DelCommand : public Command {
public:
    explicit DelCommand(std::string key);
    std::string execute(Store& store) const override;

private:
    std::string key_;
};

class ExpireCommand : public Command {
public:
    ExpireCommand(std::string key, std::chrono::seconds ttl);
    std::string execute(Store& store) const override;

private:
    std::string key_;
    std::chrono::seconds ttl_;
};

/// Replies with the number of subscribers that received the message.
class PublishCommand : public Command {
public:
    PublishCommand(Broker& broker, std::string channel, std::string message);
    std::string execute(Store&) const override;

private:
    Broker& broker_;
    std::string channel_;
    std::string message_;
};

/// Adds PUBLISH to the factory. PUBLISH needs a broker, which the built-in commands do not.
void registerPublish(CommandFactory& factory, Broker& broker);

}  // namespace miniredis
