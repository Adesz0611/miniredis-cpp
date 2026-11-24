#pragma once

#include <chrono>

namespace miniredis {

/// Source of time, injected so that expiry can be tested deterministically.
class Clock {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    virtual ~Clock() = default;
    virtual TimePoint now() const = 0;
};

class SystemClock : public Clock {
public:
    TimePoint now() const override { return std::chrono::steady_clock::now(); }

    static const SystemClock& instance() {
        static const SystemClock clock;
        return clock;
    }
};

}  // namespace miniredis
