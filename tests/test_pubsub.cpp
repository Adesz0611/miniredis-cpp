#include "miniredis/pubsub.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace miniredis;

namespace {

class RecordingSubscriber : public Subscriber {
public:
    void onMessage(const std::string& channel, const std::string& message) override {
        std::lock_guard<std::mutex> lock(mutex_);
        received_.push_back(channel + ":" + message);
    }

    std::vector<std::string> received() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return received_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::string> received_;
};

}  // namespace

TEST(BrokerTest, PublishWithoutSubscribersReachesNobody) {
    Broker b;
    EXPECT_EQ(b.publish("news", "hi"), 0u);
}

TEST(BrokerTest, SubscriberReceivesPublishedMessage) {
    Broker b;
    auto sub = std::make_shared<RecordingSubscriber>();
    b.subscribe("news", sub);
    EXPECT_EQ(b.publish("news", "hi"), 1u);
    EXPECT_EQ(sub->received(), (std::vector<std::string>{"news:hi"}));
}

TEST(BrokerTest, MessagesStayWithinTheirChannel) {
    Broker b;
    auto sub = std::make_shared<RecordingSubscriber>();
    b.subscribe("a", sub);
    EXPECT_EQ(b.publish("b", "x"), 0u);
    EXPECT_TRUE(sub->received().empty());
}

TEST(BrokerTest, AllSubscribersOfAChannelReceiveTheMessage) {
    Broker b;
    auto one = std::make_shared<RecordingSubscriber>();
    auto two = std::make_shared<RecordingSubscriber>();
    b.subscribe("c", one);
    b.subscribe("c", two);
    EXPECT_EQ(b.publish("c", "m"), 2u);
    EXPECT_EQ(one->received().size(), 1u);
    EXPECT_EQ(two->received().size(), 1u);
}

TEST(BrokerTest, SubscribingTwiceDeliversOnce) {
    Broker b;
    auto sub = std::make_shared<RecordingSubscriber>();
    b.subscribe("c", sub);
    b.subscribe("c", sub);
    EXPECT_EQ(b.publish("c", "m"), 1u);
    EXPECT_EQ(sub->received().size(), 1u);
}

TEST(BrokerTest, UnsubscribeStopsDelivery) {
    Broker b;
    auto sub = std::make_shared<RecordingSubscriber>();
    b.subscribe("c", sub);
    b.unsubscribe("c", *sub);
    EXPECT_EQ(b.publish("c", "m"), 0u);
    EXPECT_TRUE(sub->received().empty());
}

TEST(BrokerTest, UnsubscribeKeepsOtherSubscribers) {
    Broker b;
    auto one = std::make_shared<RecordingSubscriber>();
    auto two = std::make_shared<RecordingSubscriber>();
    b.subscribe("c", one);
    b.subscribe("c", two);
    b.unsubscribe("c", *one);
    EXPECT_EQ(b.publish("c", "m"), 1u);
    EXPECT_EQ(two->received().size(), 1u);
}

TEST(BrokerTest, UnsubscribeAllCoversEveryChannel) {
    Broker b;
    auto sub = std::make_shared<RecordingSubscriber>();
    b.subscribe("a", sub);
    b.subscribe("b", sub);
    b.unsubscribeAll(*sub);
    EXPECT_EQ(b.subscriberCount("a"), 0u);
    EXPECT_EQ(b.subscriberCount("b"), 0u);
}

TEST(BrokerTest, DestroyedSubscriberIsSkipped) {
    Broker b;
    auto stay = std::make_shared<RecordingSubscriber>();
    b.subscribe("c", stay);
    {
        auto gone = std::make_shared<RecordingSubscriber>();
        b.subscribe("c", gone);
        EXPECT_EQ(b.subscriberCount("c"), 2u);
    }
    EXPECT_EQ(b.subscriberCount("c"), 1u);
    EXPECT_EQ(b.publish("c", "m"), 1u);
}

TEST(BrokerTest, ConcurrentPublishAndSubscribeLoseNoMessages) {
    Broker b;
    constexpr int kPublishers = 4;
    constexpr int kMessages = 1000;
    auto fixed1 = std::make_shared<RecordingSubscriber>();
    auto fixed2 = std::make_shared<RecordingSubscriber>();
    b.subscribe("c", fixed1);
    b.subscribe("c", fixed2);

    std::atomic<bool> stop{false};
    std::thread churn([&] {
        while (!stop) {
            auto temp = std::make_shared<RecordingSubscriber>();
            b.subscribe("c", temp);
            b.unsubscribe("c", *temp);
        }
    });

    std::vector<std::thread> publishers;
    for (int t = 0; t < kPublishers; ++t) {
        publishers.emplace_back([&] {
            for (int i = 0; i < kMessages; ++i) {
                b.publish("c", "m");
            }
        });
    }
    for (auto& p : publishers) p.join();
    stop = true;
    churn.join();

    EXPECT_EQ(fixed1->received().size(), static_cast<std::size_t>(kPublishers * kMessages));
    EXPECT_EQ(fixed2->received().size(), static_cast<std::size_t>(kPublishers * kMessages));
}
