#pragma once

#include "miniredis/command_factory.h"
#include "miniredis/connection_state.h"
#include "miniredis/pubsub.h"
#include "miniredis/socket.h"
#include "miniredis/store.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace miniredis {

/// One client session. It runs on a single pool thread, but as a Subscriber it is also
/// written to by publisher threads, so all writes to the socket are serialized.
/// Must be owned by a std::shared_ptr.
class Connection : public Subscriber, public std::enable_shared_from_this<Connection> {
public:
    Connection(Socket socket, Store& store, const CommandFactory& factory, Broker& broker);

    /// Handles the client until it disconnects or @p running becomes false.
    void serve(const std::atomic<bool>& running);

    void onMessage(const std::string& channel, const std::string& message) override;

    // Operations used by the connection states. They are called from the serving thread.

    /// Sends one reply line. Returns false if the client is gone.
    bool reply(const std::string& line);

    /// Runs a regular command and returns the reply text (an ERR line for invalid input).
    std::string execute(const std::string& line);

    /// Subscribes and sends the acknowledgement; the ack is always sent before any message.
    bool subscribe(const std::string& channel);
    bool unsubscribe(const std::string& channel);

    std::vector<std::string> subscribedChannels() const;
    std::size_t subscriptionCount() const { return channels_.size(); }

    void setState(const ConnectionState& state) { state_ = &state; }

private:
    void readLoop(const std::atomic<bool>& running);
    bool handleLine(const std::string& line);

    /// Sends a final error, then reads and discards what the client still sends. Closing with
    /// unread data would make TCP reset the connection and the client could lose the error.
    void reject(const std::string& message, const std::atomic<bool>& running);

    Socket socket_;
    Store& store_;
    const CommandFactory& factory_;
    Broker& broker_;
    const ConnectionState* state_ = &normalState();
    std::set<std::string> channels_;
    std::mutex writeMutex_;
};

}  // namespace miniredis
