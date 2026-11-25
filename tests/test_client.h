#pragma once

#include "miniredis/socket.h"

#include <cstdint>
#include <string>

/// Minimal blocking line-based client for integration tests.
class TestClient {
public:
    explicit TestClient(std::uint16_t port) : sock_(miniredis::Socket::connectLoopback(port)) {}

    void send(const std::string& data) { sock_.sendAll(data); }

    /// Reads one reply line (without the newline). Empty string on timeout or EOF.
    std::string readLine(int timeoutMs = 2000) {
        for (;;) {
            auto pos = buffer_.find('\n');
            if (pos != std::string::npos) {
                std::string line = buffer_.substr(0, pos);
                buffer_.erase(0, pos + 1);
                return line;
            }
            if (!sock_.waitReadable(timeoutMs)) {
                return "";
            }
            char chunk[1024];
            ssize_t n = sock_.recv(chunk, sizeof(chunk));
            if (n <= 0) {
                return "";
            }
            buffer_.append(chunk, static_cast<std::size_t>(n));
        }
    }

    std::string command(const std::string& line) {
        send(line + "\n");
        return readLine();
    }

    /// True if the server closed the connection.
    bool closedByServer(int timeoutMs = 2000) {
        char c;
        return sock_.waitReadable(timeoutMs) && sock_.recv(&c, 1) == 0;
    }

private:
    miniredis::Socket sock_;
    std::string buffer_;
};
