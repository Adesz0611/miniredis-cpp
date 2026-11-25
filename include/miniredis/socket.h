#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <sys/types.h>

namespace miniredis {

/// Owning wrapper around a TCP socket file descriptor. Closes it on destruction.
/// Functions that fail unexpectedly throw std::system_error.
class Socket {
public:
    Socket() = default;
    explicit Socket(int fd) : fd_(fd) {}
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    /// Listens on 127.0.0.1. Port 0 lets the OS pick a free port.
    static Socket listenLoopback(std::uint16_t port, int backlog = 128);
    static Socket connectLoopback(std::uint16_t port);

    bool valid() const { return fd_ >= 0; }
    std::uint16_t localPort() const;

    /// Waits up to @p timeoutMs for a pending connection. Returns nullopt on timeout.
    std::optional<Socket> acceptFor(int timeoutMs);

    /// True if data (or EOF) can be read without blocking.
    bool waitReadable(int timeoutMs) const;

    /// Returns the number of bytes read, 0 on EOF and -1 on error.
    ssize_t recv(char* buffer, std::size_t length);

    /// Returns false if the peer is gone.
    bool sendAll(std::string_view data);

    /// Signals EOF to the peer while keeping the receive side open.
    void shutdownWrite();

    void close();

private:
    int fd_ = -1;
};

}  // namespace miniredis
