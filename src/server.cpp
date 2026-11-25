#include "miniredis/server.h"

#include "miniredis/connection.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <system_error>

namespace miniredis {

namespace {

constexpr int kPollIntervalMs = 100;

}  // namespace

Server::Server(Store& store, const CommandFactory& factory, Broker& broker, std::size_t threads)
    : store_(store), factory_(factory), broker_(broker), pool_(threads) {}

Server::~Server() { stop(); }

std::uint16_t Server::start(std::uint16_t port) {
    if (running_) {
        throw std::logic_error("server already running");
    }
    listener_ = Socket::listenLoopback(port);
    running_ = true;
    acceptor_ = std::thread([this] { acceptLoop(); });
    return listener_.localPort();
}

void Server::stop() {
    running_ = false;
    if (acceptor_.joinable()) {
        acceptor_.join();
    }
    listener_.close();
}

void Server::acceptLoop() {
    while (running_) {
        try {
            auto client = listener_.acceptFor(kPollIntervalMs);
            if (!client) {
                continue;
            }
            // std::function requires a copyable callable, a Socket is move-only.
            auto shared = std::make_shared<Socket>(std::move(*client));
            pool_.submit([this, shared] {
                try {
                    auto connection =
                        std::make_shared<Connection>(std::move(*shared), store_, factory_, broker_);
                    connection->serve(running_);
                } catch (const std::exception& e) {
                    std::cerr << "connection error: " << e.what() << '\n';
                }
            });
        } catch (const std::system_error& e) {
            std::cerr << "accept error: " << e.what() << '\n';
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
        }
    }
}

}  // namespace miniredis
