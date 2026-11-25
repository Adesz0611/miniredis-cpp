#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace miniredis {

/// Observer: receives messages published to the channels it is subscribed to.
class Subscriber {
public:
    virtual ~Subscriber() = default;

    /// Called on the publisher's thread, without any broker lock held.
    virtual void onMessage(const std::string& channel, const std::string& message) = 0;
};

/// Subject: keeps channels with their subscribers and delivers published messages.
/// Subscribers are held weakly, so a destroyed subscriber is simply skipped.
class Broker {
public:
    /// Subscribing the same subscriber to a channel twice has no further effect.
    void subscribe(const std::string& channel, const std::shared_ptr<Subscriber>& subscriber);
    void unsubscribe(const std::string& channel, const Subscriber& subscriber);
    void unsubscribeAll(const Subscriber& subscriber);

    /// Delivers the message to every live subscriber and returns how many received it.
    std::size_t publish(const std::string& channel, const std::string& message);

    std::size_t subscriberCount(const std::string& channel) const;

private:
    using Subscribers = std::vector<std::weak_ptr<Subscriber>>;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Subscribers> channels_;
};

}  // namespace miniredis
