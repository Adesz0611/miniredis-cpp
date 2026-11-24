#pragma once

#include "miniredis/store.h"

#include <string>

namespace miniredis {

/// A single executable protocol command. Returns the reply line to send back.
class Command {
public:
    virtual ~Command() = default;
    virtual std::string execute(Store& store) const = 0;
};

}  // namespace miniredis
