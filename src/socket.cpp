#include "miniredis/socket.h"

#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace miniredis {

namespace {

[[noreturn]] void throwErrno(const char* what) {
    throw std::system_error(errno, std::generic_category(), what);
}

sockaddr_in loopbackAddress(std::uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return addr;
}

}  // namespace

Socket::~Socket() { close(); }

Socket::Socket(Socket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
}

void Socket::shutdownWrite() {
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_WR);
    }
}

void Socket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

Socket Socket::listenLoopback(std::uint16_t port, int backlog) {
    Socket sock(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (!sock.valid()) {
        throwErrno("socket");
    }
    int yes = 1;
    ::setsockopt(sock.fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr = loopbackAddress(port);
    if (::bind(sock.fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throwErrno("bind");
    }
    if (::listen(sock.fd_, backlog) < 0) {
        throwErrno("listen");
    }
    return sock;
}

Socket Socket::connectLoopback(std::uint16_t port) {
    Socket sock(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (!sock.valid()) {
        throwErrno("socket");
    }
    sockaddr_in addr = loopbackAddress(port);
    if (::connect(sock.fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throwErrno("connect");
    }
    return sock;
}

std::uint16_t Socket::localPort() const {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (::getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &len) < 0) {
        throwErrno("getsockname");
    }
    return ntohs(addr.sin_port);
}

bool Socket::waitReadable(int timeoutMs) const {
    pollfd pfd{fd_, POLLIN, 0};
    int ready = ::poll(&pfd, 1, timeoutMs);
    if (ready < 0 && errno != EINTR) {
        throwErrno("poll");
    }
    return ready > 0;
}

std::optional<Socket> Socket::acceptFor(int timeoutMs) {
    if (!waitReadable(timeoutMs)) {
        return std::nullopt;
    }
    int fd = ::accept4(fd_, nullptr, nullptr, SOCK_CLOEXEC);
    if (fd < 0) {
        if (errno == EINTR || errno == EAGAIN || errno == ECONNABORTED) {
            return std::nullopt;
        }
        throwErrno("accept");
    }
    return Socket(fd);
}

ssize_t Socket::recv(char* buffer, std::size_t length) {
    ssize_t n;
    do {
        n = ::recv(fd_, buffer, length, 0);
    } while (n < 0 && errno == EINTR);
    return n;
}

bool Socket::sendAll(std::string_view data) {
    while (!data.empty()) {
        ssize_t n = ::send(fd_, data.data(), data.size(), MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        data.remove_prefix(static_cast<std::size_t>(n));
    }
    return true;
}

}  // namespace miniredis
