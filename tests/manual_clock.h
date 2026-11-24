#pragma once

#include "miniredis/clock.h"

class ManualClock : public miniredis::Clock {
public:
    TimePoint now() const override { return now_; }
    void advance(std::chrono::milliseconds d) { now_ += d; }

private:
    TimePoint now_{};
};
