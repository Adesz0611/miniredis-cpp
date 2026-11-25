#include "miniredis/connection.h"

namespace miniredis {

namespace {

constexpr int kPollIntervalMs = 100;
constexpr std::size_t kMaxLineLength = 64 * 1024;
constexpr std::size_t kMaxDiscardBytes = 1024 * 1024;

}  // namespace

Connection::Connection(Socket socket, Store& store, const CommandFactory& factory, Broker& broker)
    : socket_(std::move(socket)), store_(store), factory_(factory), broker_(broker) {}

void Connection::serve(const std::atomic<bool>& running) {
    readLoop(running);
    broker_.unsubscribeAll(*this);
}

void Connection::readLoop(const std::atomic<bool>& running) {
    std::string buffer;
    char chunk[4096];
    while (running) {
        if (!socket_.waitReadable(kPollIntervalMs)) {
            continue;
        }
        ssize_t n = socket_.recv(chunk, sizeof(chunk));
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
            if (!handleLine(line)) {
                return;
            }
        }
        if (buffer.size() > kMaxLineLength) {
            reject("ERR line too long", running);
            return;
        }
    }
}

bool Connection::handleLine(const std::string& line) {
    if (line.empty()) {
        return true;
    }
    if (line == "QUIT") {
        reply("OK");
        return false;
    }
    return state_->handle(*this, line);
}

void Connection::reject(const std::string& message, const std::atomic<bool>& running) {
    reply(message);
    socket_.shutdownWrite();
    char sink[4096];
    std::size_t discarded = 0;
    while (running && discarded < kMaxDiscardBytes && socket_.waitReadable(kPollIntervalMs)) {
        ssize_t n = socket_.recv(sink, sizeof(sink));
        if (n <= 0) {
            return;
        }
        discarded += static_cast<std::size_t>(n);
    }
}

bool Connection::reply(const std::string& line) {
    std::lock_guard<std::mutex> lock(writeMutex_);
    return socket_.sendAll(line + "\n");
}

std::string Connection::execute(const std::string& line) {
    try {
        return factory_.create(line)->execute(store_);
    } catch (const ParseError& e) {
        return std::string("ERR ") + e.what();
    }
}

void Connection::onMessage(const std::string& channel, const std::string& message) {
    reply("message " + channel + " " + message);
}

bool Connection::subscribe(const std::string& channel) {
    // The write lock is held across registration and the ack, so a message that is
    // published right after registration cannot overtake the ack.
    std::lock_guard<std::mutex> lock(writeMutex_);
    if (channels_.insert(channel).second) {
        broker_.subscribe(channel, shared_from_this());
    }
    return socket_.sendAll("subscribed " + channel + " " + std::to_string(channels_.size()) + "\n");
}

bool Connection::unsubscribe(const std::string& channel) {
    std::lock_guard<std::mutex> lock(writeMutex_);
    if (channels_.erase(channel) > 0) {
        broker_.unsubscribe(channel, *this);
    }
    return socket_.sendAll("unsubscribed " + channel + " " + std::to_string(channels_.size()) +
                           "\n");
}

std::vector<std::string> Connection::subscribedChannels() const {
    return std::vector<std::string>(channels_.begin(), channels_.end());
}

}  // namespace miniredis
