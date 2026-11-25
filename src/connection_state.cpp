#include "miniredis/connection_state.h"

#include "miniredis/connection.h"
#include "miniredis/text.h"

namespace miniredis {

namespace {

bool subscribeTo(Connection& connection, const std::vector<std::string>& words) {
    for (std::size_t i = 1; i < words.size(); ++i) {
        if (!connection.subscribe(words[i])) {
            return false;
        }
    }
    return true;
}

bool unsubscribeFrom(Connection& connection, const std::vector<std::string>& words) {
    std::vector<std::string> channels(words.begin() + 1, words.end());
    if (channels.empty()) {
        channels = connection.subscribedChannels();
    }
    for (const auto& channel : channels) {
        if (!connection.unsubscribe(channel)) {
            return false;
        }
    }
    return true;
}

class NormalState : public ConnectionState {
public:
    bool handle(Connection& connection, const std::string& line) const override {
        auto words = splitWords(line);
        const std::string name = toUpper(words.front());

        if (name == "SUBSCRIBE") {
            if (words.size() < 2) {
                return connection.reply("ERR wrong number of arguments for SUBSCRIBE");
            }
            connection.setState(subscribedState());
            return subscribeTo(connection, words);
        }
        if (name == "UNSUBSCRIBE") {
            return connection.reply("ERR not subscribed");
        }
        return connection.reply(connection.execute(line));
    }
};

class SubscribedState : public ConnectionState {
public:
    bool handle(Connection& connection, const std::string& line) const override {
        auto words = splitWords(line);
        const std::string name = toUpper(words.front());

        if (name == "SUBSCRIBE") {
            if (words.size() < 2) {
                return connection.reply("ERR wrong number of arguments for SUBSCRIBE");
            }
            return subscribeTo(connection, words);
        }
        if (name == "UNSUBSCRIBE") {
            bool ok = unsubscribeFrom(connection, words);
            if (connection.subscriptionCount() == 0) {
                connection.setState(normalState());
            }
            return ok;
        }
        return connection.reply("ERR only SUBSCRIBE, UNSUBSCRIBE and QUIT are allowed");
    }
};

}  // namespace

const ConnectionState& normalState() {
    static const NormalState state;
    return state;
}

const ConnectionState& subscribedState() {
    static const SubscribedState state;
    return state;
}

}  // namespace miniredis
