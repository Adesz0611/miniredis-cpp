#pragma once

#include "miniredis/command.h"

#include <string>

namespace miniredis {

class SetCommand : public Command {
public:
    SetCommand(std::string key, std::string value);
    std::string execute(Store& store) const override;

private:
    std::string key_;
    std::string value_;
};

class GetCommand : public Command {
public:
    explicit GetCommand(std::string key);
    std::string execute(Store& store) const override;

private:
    std::string key_;
};

class DelCommand : public Command {
public:
    explicit DelCommand(std::string key);
    std::string execute(Store& store) const override;

private:
    std::string key_;
};

}  // namespace miniredis
