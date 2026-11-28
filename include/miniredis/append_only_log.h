#pragma once

#include "miniredis/command.h"
#include "miniredis/store.h"

#include <mutex>
#include <string>
#include <vector>

namespace miniredis {

/// Append-only file with one command per line. Commands are written before they are
/// executed (write-ahead), so a command that could not be logged is never applied.
/// I/O failures throw std::system_error.
class AppendOnlyLog {
public:
    enum class Sync {
        Never,   // survives a process crash, not necessarily a power loss
        Always,  // fsync after every entry
    };

    /// Opens the log for appending and creates it if needed.
    explicit AppendOnlyLog(std::string path, Sync sync = Sync::Never);
    ~AppendOnlyLog();

    AppendOnlyLog(const AppendOnlyLog&) = delete;
    AppendOnlyLog& operator=(const AppendOnlyLog&) = delete;

    /// Reads all complete entries of the file at @p path (none if it does not exist or is
    /// not a regular file).
    /// A last line without newline is a write that a crash cut short; it is removed
    /// from the file so that later entries do not get glued to it.
    static std::vector<std::string> load(const std::string& path);

    /// Appends @p line, then runs @p command. Both happen under one lock, so the order of
    /// the entries in the file is the order in which the commands were applied.
    std::string execute(const std::string& line, const Command& command, Store& store);

    /// Replaces the file with the minimal list of commands that rebuilds the current store.
    /// The new file is written aside and renamed over the old one, so a failure keeps the
    /// old log intact.
    void rewrite(const Store& store);

private:
    void openFile();
    void append(const std::string& line);

    std::string path_;
    Sync sync_;
    int fd_ = -1;
    std::mutex mutex_;
};

}  // namespace miniredis
