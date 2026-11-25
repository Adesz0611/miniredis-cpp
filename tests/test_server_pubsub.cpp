#include "miniredis/command_factory.h"
#include "miniredis/commands.h"
#include "miniredis/server.h"
#include "miniredis/store.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "test_client.h"
#include <gtest/gtest.h>

using namespace miniredis;

class PubSubServerTest : public ::testing::Test {
protected:
    PubSubServerTest() : server(store, factory, broker, 8) {
        registerPublish(factory, broker);
        port = server.start(0);
    }

    // Polls until the server has noticed that subscribers left.
    bool waitForSubscriberCount(const std::string& channel, std::size_t expected) {
        for (int i = 0; i < 100; ++i) {
            if (broker.subscriberCount(channel) == expected) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
    }

    Store store;
    Broker broker;
    CommandFactory factory;
    Server server;
    std::uint16_t port = 0;
};

TEST_F(PubSubServerTest, SubscribeIsAcknowledged) {
    TestClient c(port);
    EXPECT_EQ(c.command("SUBSCRIBE news"), "subscribed news 1");
}

TEST_F(PubSubServerTest, SubscribeToSeveralChannelsAcknowledgesEach) {
    TestClient c(port);
    c.send("SUBSCRIBE a b\n");
    EXPECT_EQ(c.readLine(), "subscribed a 1");
    EXPECT_EQ(c.readLine(), "subscribed b 2");
}

TEST_F(PubSubServerTest, PublishedMessageReachesSubscriber) {
    TestClient sub(port);
    TestClient pub(port);
    ASSERT_EQ(sub.command("SUBSCRIBE news"), "subscribed news 1");

    EXPECT_EQ(pub.command("PUBLISH news hello   big world"), "1");
    EXPECT_EQ(sub.readLine(), "message news hello big world");
}

TEST_F(PubSubServerTest, PublishWithoutSubscribersReturnsZero) {
    TestClient pub(port);
    EXPECT_EQ(pub.command("PUBLISH nobody hi"), "0");
}

TEST_F(PubSubServerTest, PublishRequiresChannelAndMessage) {
    TestClient c(port);
    EXPECT_EQ(c.command("PUBLISH news"), "ERR wrong number of arguments for PUBLISH");
}

TEST_F(PubSubServerTest, SubscriberOnlyGetsItsChannels) {
    TestClient sub(port);
    TestClient pub(port);
    ASSERT_EQ(sub.command("SUBSCRIBE a"), "subscribed a 1");
    EXPECT_EQ(pub.command("PUBLISH b x"), "0");
    EXPECT_EQ(pub.command("PUBLISH a y"), "1");
    EXPECT_EQ(sub.readLine(), "message a y");
}

TEST_F(PubSubServerTest, SubscribedStateRejectsRegularCommands) {
    TestClient c(port);
    ASSERT_EQ(c.command("SUBSCRIBE news"), "subscribed news 1");
    EXPECT_EQ(c.command("SET a 1"), "ERR only SUBSCRIBE, UNSUBSCRIBE and QUIT are allowed");
    EXPECT_EQ(c.command("PUBLISH news x"), "ERR only SUBSCRIBE, UNSUBSCRIBE and QUIT are allowed");
}

TEST_F(PubSubServerTest, LastUnsubscribeReturnsToNormalState) {
    TestClient c(port);
    ASSERT_EQ(c.command("SUBSCRIBE news"), "subscribed news 1");
    EXPECT_EQ(c.command("UNSUBSCRIBE news"), "unsubscribed news 0");
    EXPECT_EQ(c.command("SET a 1"), "OK");
}

TEST_F(PubSubServerTest, ConnectionStaysSubscribedWhileChannelsRemain) {
    TestClient c(port);
    c.send("SUBSCRIBE a b\n");
    c.readLine();
    c.readLine();
    EXPECT_EQ(c.command("UNSUBSCRIBE a"), "unsubscribed a 1");
    EXPECT_EQ(c.command("SET k v"), "ERR only SUBSCRIBE, UNSUBSCRIBE and QUIT are allowed");
}

TEST_F(PubSubServerTest, UnsubscribeWithoutArgumentsLeavesAllChannels) {
    TestClient c(port);
    c.send("SUBSCRIBE a b\n");
    c.readLine();
    c.readLine();
    c.send("UNSUBSCRIBE\n");
    EXPECT_EQ(c.readLine(), "unsubscribed a 1");
    EXPECT_EQ(c.readLine(), "unsubscribed b 0");
    EXPECT_EQ(c.command("SET a 1"), "OK");
}

TEST_F(PubSubServerTest, UnsubscribedClientGetsNoMoreMessages) {
    TestClient sub(port);
    TestClient pub(port);
    ASSERT_EQ(sub.command("SUBSCRIBE news"), "subscribed news 1");
    ASSERT_EQ(sub.command("UNSUBSCRIBE news"), "unsubscribed news 0");
    EXPECT_EQ(pub.command("PUBLISH news x"), "0");
}

TEST_F(PubSubServerTest, UnsubscribeOutsideSubscribedStateIsAnError) {
    TestClient c(port);
    EXPECT_EQ(c.command("UNSUBSCRIBE news"), "ERR not subscribed");
}

TEST_F(PubSubServerTest, SubscribeWithoutChannelIsAnError) {
    TestClient c(port);
    EXPECT_EQ(c.command("SUBSCRIBE"), "ERR wrong number of arguments for SUBSCRIBE");
    EXPECT_EQ(c.command("SET a 1"), "OK");
}

TEST_F(PubSubServerTest, QuitWorksInSubscribedState) {
    TestClient c(port);
    ASSERT_EQ(c.command("SUBSCRIBE news"), "subscribed news 1");
    EXPECT_EQ(c.command("QUIT"), "OK");
    EXPECT_TRUE(c.closedByServer());
    EXPECT_TRUE(waitForSubscriberCount("news", 0));
}

TEST_F(PubSubServerTest, AbruptDisconnectRemovesSubscription) {
    {
        TestClient c(port);
        ASSERT_EQ(c.command("SUBSCRIBE news"), "subscribed news 1");
        ASSERT_EQ(broker.subscriberCount("news"), 1u);
    }
    EXPECT_TRUE(waitForSubscriberCount("news", 0));
    TestClient pub(port);
    EXPECT_EQ(pub.command("PUBLISH news x"), "0");
}

TEST_F(PubSubServerTest, EverySubscriberGetsEveryMessageInOrder) {
    constexpr int kSubscribers = 3;
    constexpr int kMessages = 50;
    std::vector<std::unique_ptr<TestClient>> subscribers;
    for (int i = 0; i < kSubscribers; ++i) {
        subscribers.push_back(std::make_unique<TestClient>(port));
        ASSERT_EQ(subscribers.back()->command("SUBSCRIBE feed"), "subscribed feed 1");
    }

    TestClient pub(port);
    for (int m = 0; m < kMessages; ++m) {
        ASSERT_EQ(pub.command("PUBLISH feed m" + std::to_string(m)), std::to_string(kSubscribers));
    }
    for (auto& sub : subscribers) {
        for (int m = 0; m < kMessages; ++m) {
            ASSERT_EQ(sub->readLine(), "message feed m" + std::to_string(m));
        }
    }
}

TEST_F(PubSubServerTest, ConcurrentPublishersDeliverEverything) {
    constexpr int kPublishers = 4;
    constexpr int kMessages = 50;
    TestClient sub(port);
    ASSERT_EQ(sub.command("SUBSCRIBE feed"), "subscribed feed 1");

    std::vector<std::thread> threads;
    for (int p = 0; p < kPublishers; ++p) {
        threads.emplace_back([this] {
            TestClient pub(port);
            for (int m = 0; m < kMessages; ++m) {
                pub.command("PUBLISH feed x");
            }
        });
    }
    for (auto& t : threads) t.join();

    for (int i = 0; i < kPublishers * kMessages; ++i) {
        ASSERT_EQ(sub.readLine(), "message feed x") << "message " << i;
    }
}
