#include "miniredis/command_factory.h"
#include "miniredis/server.h"
#include "miniredis/store.h"

#include <string>
#include <thread>
#include <vector>

#include "test_client.h"
#include <gtest/gtest.h>

using namespace miniredis;

class ServerTest : public ::testing::Test {
protected:
    ServerTest() : server(store, factory, 8) { port = server.start(0); }

    Store store;
    CommandFactory factory;
    Server server;
    std::uint16_t port = 0;
};

TEST_F(ServerTest, SetThenGet) {
    TestClient c(port);
    EXPECT_EQ(c.command("SET a 1"), "OK");
    EXPECT_EQ(c.command("GET a"), "1");
    EXPECT_EQ(c.command("GET missing"), "(nil)");
}

TEST_F(ServerTest, ErrorsAreReportedAndConnectionStaysOpen) {
    TestClient c(port);
    EXPECT_EQ(c.command("FLY a"), "ERR unknown command 'FLY'");
    EXPECT_EQ(c.command("SET a"), "ERR wrong number of arguments for SET");
    EXPECT_EQ(c.command("SET a 1"), "OK");
}

TEST_F(ServerTest, CommandSplitAcrossPacketsIsAssembled) {
    TestClient c(port);
    c.send("SE");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    c.send("T a 1\n");
    EXPECT_EQ(c.readLine(), "OK");
}

TEST_F(ServerTest, PipelinedCommandsGetOrderedReplies) {
    TestClient c(port);
    c.send("SET a 1\nGET a\nDEL a\nGET a\n");
    EXPECT_EQ(c.readLine(), "OK");
    EXPECT_EQ(c.readLine(), "1");
    EXPECT_EQ(c.readLine(), "1");
    EXPECT_EQ(c.readLine(), "(nil)");
}

TEST_F(ServerTest, CarriageReturnAndBlankLinesAreTolerated) {
    TestClient c(port);
    c.send("\n\r\nSET a 1\r\n");
    EXPECT_EQ(c.readLine(), "OK");
}

TEST_F(ServerTest, QuitClosesConnection) {
    TestClient c(port);
    EXPECT_EQ(c.command("QUIT"), "OK");
    EXPECT_TRUE(c.closedByServer());
}

TEST_F(ServerTest, OversizedLineIsRejected) {
    TestClient c(port);
    c.send(std::string(70 * 1024, 'x'));
    EXPECT_EQ(c.readLine(), "ERR line too long");
    EXPECT_TRUE(c.closedByServer());
}

TEST_F(ServerTest, ClientsShareTheStore) {
    TestClient a(port);
    TestClient b(port);
    EXPECT_EQ(a.command("SET shared 42"), "OK");
    EXPECT_EQ(b.command("GET shared"), "42");
}

TEST_F(ServerTest, ManyConcurrentClients) {
    constexpr int kClients = 8;
    constexpr int kOps = 100;
    std::vector<std::thread> threads;
    std::atomic<int> failures{0};
    for (int t = 0; t < kClients; ++t) {
        threads.emplace_back([&, t] {
            TestClient c(port);
            for (int i = 0; i < kOps; ++i) {
                std::string key = "k" + std::to_string(t) + "_" + std::to_string(i);
                if (c.command("SET " + key + " v") != "OK" || c.command("GET " + key) != "v") {
                    ++failures;
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(failures, 0);
    EXPECT_EQ(store.size(), static_cast<std::size_t>(kClients * kOps));
}

TEST(ServerLifecycleTest, StopDoesNotHangWithIdleClientConnected) {
    Store store;
    CommandFactory factory;
    Server server(store, factory, 2);
    TestClient idle(server.start(0));
    ASSERT_EQ(idle.command("SET a 1"), "OK");
    server.stop();
    EXPECT_TRUE(idle.closedByServer());
}

TEST(ServerLifecycleTest, StartingTwiceThrows) {
    Store store;
    CommandFactory factory;
    Server server(store, factory, 1);
    server.start(0);
    EXPECT_THROW(server.start(0), std::logic_error);
}
