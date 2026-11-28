#include "miniredis/append_only_log.h"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace miniredis {

namespace {

[[noreturn]] void throwErrno(const std::string& what) {
    throw std::system_error(errno, std::generic_category(), what);
}

void writeAll(int fd, const std::string& data) {
    std::size_t done = 0;
    while (done < data.size()) {
        ssize_t n = ::write(fd, data.data() + done, data.size() - done);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throwErrno("write");
        }
        done += static_cast<std::size_t>(n);
    }
}

class FileDescriptor {
public:
    explicit FileDescriptor(int fd) : fd_(fd) {}
    ~FileDescriptor() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    int get() const { return fd_; }

    /// Gives up ownership without closing.
    int release() { return std::exchange(fd_, -1); }

private:
    int fd_;
};

// Makes a rename durable. Best effort: a failure here does not undo the rename.
void syncDirectoryOf(const std::string& path) {
    std::filesystem::path dir = std::filesystem::path(path).parent_path();
    if (dir.empty()) {
        dir = ".";
    }
    FileDescriptor fd(::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (fd.get() >= 0) {
        ::fsync(fd.get());
    }
}

}  // namespace

AppendOnlyLog::AppendOnlyLog(std::string path, Sync sync) : path_(std::move(path)), sync_(sync) {
    openFile();
}

AppendOnlyLog::~AppendOnlyLog() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

void AppendOnlyLog::openFile() {
    fd_ = ::open(path_.c_str(), O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
    if (fd_ < 0) {
        throwErrno("open " + path_);
    }
}

std::vector<std::string> AppendOnlyLog::load(const std::string& path) {
    // Device files such as /dev/zero never end, and a FIFO would block. Neither can hold a log.
    if (!std::filesystem::is_regular_file(path)) {
        return {};
    }

    std::string content;
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return {};
        }
        content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    const std::size_t lastNewline = content.rfind('\n');
    const std::size_t complete = lastNewline == std::string::npos ? 0 : lastNewline + 1;
    if (complete != content.size()) {
        std::filesystem::resize_file(path, complete);
    }

    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < complete) {
        const std::size_t end = content.find('\n', start);
        if (end > start) {
            lines.push_back(content.substr(start, end - start));
        }
        start = end + 1;
    }
    return lines;
}

void AppendOnlyLog::append(const std::string& line) {
    const off_t before = ::lseek(fd_, 0, SEEK_END);
    try {
        writeAll(fd_, line + "\n");
        if (sync_ == Sync::Always && ::fsync(fd_) < 0) {
            throwErrno("fsync");
        }
    } catch (...) {
        // Take back what was written of this entry. Otherwise a half-written line would stay
        // in the middle of the file, and the next entry would be glued to it.
        if (before >= 0 && ::ftruncate(fd_, before) < 0) {
            // Nothing more can be done; load() cannot repair a damaged line in the middle.
        }
        throw;
    }
}

std::string AppendOnlyLog::execute(const std::string& line, const Command& command, Store& store) {
    std::lock_guard<std::mutex> lock(mutex_);
    append(line);
    return command.execute(store);
}

void AppendOnlyLog::rewrite(const Store& store) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string content;
    for (const auto& item : store.snapshot()) {
        content += "SET " + item.key + " " + item.value + "\n";
        if (item.ttl) {
            const auto seconds = std::max<long long>(1, (item.ttl->count() + 999) / 1000);
            content += "EXPIRE " + item.key + " " + std::to_string(seconds) + "\n";
        }
    }

    // The temporary file stays open and becomes the log after the rename, so there is no
    // step after the rename that could fail and leave the log without a file descriptor.
    const std::string tmpPath = path_ + ".tmp";
    FileDescriptor tmp(
        ::open(tmpPath.c_str(), O_WRONLY | O_APPEND | O_CREAT | O_TRUNC | O_CLOEXEC, 0644));
    if (tmp.get() < 0) {
        throwErrno("open " + tmpPath);
    }
    try {
        writeAll(tmp.get(), content);
        if (::fsync(tmp.get()) < 0) {
            throwErrno("fsync");
        }
        if (::rename(tmpPath.c_str(), path_.c_str()) < 0) {
            throwErrno("rename " + tmpPath);
        }
    } catch (...) {
        std::remove(tmpPath.c_str());
        throw;
    }
    syncDirectoryOf(path_);

    ::close(fd_);
    fd_ = tmp.release();
}

}  // namespace miniredis
