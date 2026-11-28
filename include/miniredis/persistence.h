#pragma once

#include "miniredis/append_only_log.h"
#include "miniredis/command.h"
#include "miniredis/command_factory.h"
#include "miniredis/store.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace miniredis {

/// Decorator: logs the command line before running the wrapped command.
class LoggedCommand : public Command {
public:
    LoggedCommand(std::unique_ptr<Command> inner, std::string line, AppendOnlyLog& log);
    std::string execute(Store& store) const override;

private:
    std::unique_ptr<Command> inner_;
    std::string line_;
    AppendOnlyLog& log_;
};

/// Makes SET, DEL and EXPIRE go through the log and adds the COMPACT command.
/// The log must outlive the factory's use.
void registerPersistence(CommandFactory& factory, AppendOnlyLog& log);

struct ReplayResult {
    std::size_t applied = 0;
    std::size_t skipped = 0;  // lines that are not valid commands
};

/// Re-executes the logged commands on @p store. Does not write to any log.
ReplayResult replayLog(const std::vector<std::string>& lines, Store& store);

}  // namespace miniredis
