#pragma once

#include "miniredis/command_factory.h"
#include "miniredis/pubsub.h"
#include "miniredis/socket.h"
#include "miniredis/store.h"
#include "miniredis/thread_pool.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>

namespace miniredis {

/// Line-based TCP server. Every connection occupies one pool thread while it is open,
/// so @p threads is also the number of clients served concurrently.
class Server {
public:
    /// The store, factory and broker must outlive the server.
    Server(Store& store, const CommandFactory& factory, Broker& broker, std::size_t threads);
    ~Server();

    /// Listens on 127.0.0.1:port (0 picks a free port) and returns the bound port.
    std::uint16_t start(std::uint16_t port);
    void stop();

private:
    void acceptLoop();

    Store& store_;
    const CommandFactory& factory_;
    Broker& broker_;
    ThreadPool pool_;
    Socket listener_;
    std::thread acceptor_;
    std::atomic<bool> running_{false};
};

}  // namespace miniredis
