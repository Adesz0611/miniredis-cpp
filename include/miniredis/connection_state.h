#pragma once

#include <string>

namespace miniredis {

class Connection;

/// State: decides how a connection interprets the lines it receives.
/// States hold no data, so the same instance is shared by all connections.
class ConnectionState {
public:
    virtual ~ConnectionState() = default;

    /// Returns false if the connection should be closed.
    virtual bool handle(Connection& connection, const std::string& line) const = 0;
};

/// Regular commands, plus SUBSCRIBE to enter the subscribed state.
const ConnectionState& normalState();

/// Only SUBSCRIBE, UNSUBSCRIBE and QUIT; leaves for the normal state after the last unsubscribe.
const ConnectionState& subscribedState();

}  // namespace miniredis
