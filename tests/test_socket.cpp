#include "miniredis/socket.h"

#include <string>
#include <utility>

#include <gtest/gtest.h>

using miniredis::Socket;

TEST(SocketTest, DefaultSocketIsInvalid) {
    Socket s;
    EXPECT_FALSE(s.valid());
}

TEST(SocketTest, MovedFromSocketIsInvalid) {
    Socket a = Socket::listenLoopback(0);
    ASSERT_TRUE(a.valid());
    Socket b = std::move(a);
    EXPECT_FALSE(a.valid());
    EXPECT_TRUE(b.valid());
}

TEST(SocketTest, ListenOnPortZeroPicksFreePort) {
    Socket s = Socket::listenLoopback(0);
    EXPECT_NE(s.localPort(), 0);
}

TEST(SocketTest, AcceptTimesOutWithoutClient) {
    Socket listener = Socket::listenLoopback(0);
    EXPECT_FALSE(listener.acceptFor(10).has_value());
}

TEST(SocketTest, SendAndReceiveRoundTrip) {
    Socket listener = Socket::listenLoopback(0);
    Socket client = Socket::connectLoopback(listener.localPort());
    auto server = listener.acceptFor(1000);
    ASSERT_TRUE(server.has_value());

    ASSERT_TRUE(client.sendAll("hello"));
    ASSERT_TRUE(server->waitReadable(1000));
    char buf[16];
    ssize_t n = server->recv(buf, sizeof(buf));
    ASSERT_EQ(n, 5);
    EXPECT_EQ(std::string(buf, 5), "hello");
}

TEST(SocketTest, ReceiveReturnsZeroAfterPeerCloses) {
    Socket listener = Socket::listenLoopback(0);
    Socket client = Socket::connectLoopback(listener.localPort());
    auto server = listener.acceptFor(1000);
    ASSERT_TRUE(server.has_value());

    client.close();
    char buf[8];
    EXPECT_EQ(server->recv(buf, sizeof(buf)), 0);
}

TEST(SocketTest, ConnectToClosedPortThrows) {
    std::uint16_t port;
    {
        Socket s = Socket::listenLoopback(0);
        port = s.localPort();
    }
    EXPECT_THROW(Socket::connectLoopback(port), std::system_error);
}
