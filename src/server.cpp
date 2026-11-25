#include "miniredis/server.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <system_error>

namespace miniredis {

namespace {

constexpr int kPollIntervalMs = 100;
constexpr std::size_t kMaxLineLength = 64 * 1024;
constexpr std::size_t kMaxDiscardBytes = 1024 * 1024;

}  // namespace

Server::Server(Store& store, const CommandFactory& factory, std::size_t threads)
    : store_(store), factory_(factory), pool_(threads) {}

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
                    handleClient(std::move(*shared));
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

void Server::handleClient(Socket client) {
    std::string buffer;
    char chunk[4096];
    while (running_) {
        if (!client.waitReadable(kPollIntervalMs)) {
            continue;
        }
        ssize_t n = client.recv(chunk, sizeof(chunk));
        if (n <= 0) {
            return;
        }
        buffer.append(chunk, static_cast<std::size_t>(n));

        std::size_t newline;
        while ((newline = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, newline);
            buffer.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!processLine(client, line)) {
                return;
            }
        }
        if (buffer.size() > kMaxLineLength) {
            rejectClient(client, "ERR line too long\n");
            return;
        }
    }
}

void Server::rejectClient(Socket& client, const std::string& message) {
    client.sendAll(message);
    client.shutdownWrite();
    char sink[4096];
    std::size_t discarded = 0;
    while (running_ && discarded < kMaxDiscardBytes && client.waitReadable(kPollIntervalMs)) {
        ssize_t n = client.recv(sink, sizeof(sink));
        if (n <= 0) {
            return;
        }
        discarded += static_cast<std::size_t>(n);
    }
}

bool Server::processLine(Socket& client, const std::string& line) {
    if (line.empty()) {
        return true;
    }
    if (line == "QUIT") {
        client.sendAll("OK\n");
        return false;
    }
    std::string reply;
    try {
        reply = factory_.create(line)->execute(store_);
    } catch (const ParseError& e) {
        reply = std::string("ERR ") + e.what();
    }
    reply += '\n';
    return client.sendAll(reply);
}

}  // namespace miniredis
