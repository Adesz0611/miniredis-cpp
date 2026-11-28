#include "miniredis/append_only_log.h"
#include "miniredis/commands.h"
#include "miniredis/persistence.h"

#include <filesystem>
#include <signal.h>
#include <string>
#include <sys/resource.h>
#include <thread>
#include <vector>

#include "manual_clock.h"
#include "temp_dir.h"
#include <gtest/gtest.h>

using namespace miniredis;

TEST(AppendOnlyLogTest, LoadOfMissingFileIsEmpty) {
    TempDir dir;
    EXPECT_TRUE(AppendOnlyLog::load(dir.file("none.aof")).empty());
}

TEST(AppendOnlyLogTest, ExecuteWritesTheLineAndRunsTheCommand) {
    TempDir dir;
    Store store;
    {
        AppendOnlyLog log(dir.file("a.aof"));
        SetCommand set("k", "v");
        EXPECT_EQ(log.execute("SET k v", set, store), "OK");
    }
    EXPECT_EQ(*store.get("k"), "v");
    EXPECT_EQ(readFile(dir.file("a.aof")), "SET k v\n");
}

TEST(AppendOnlyLogTest, LoadReturnsEntriesInOrder) {
    TempDir dir;
    Store store;
    {
        AppendOnlyLog log(dir.file("a.aof"));
        log.execute("SET a 1", SetCommand("a", "1"), store);
        log.execute("DEL a", DelCommand("a"), store);
        log.execute("SET b 2", SetCommand("b", "2"), store);
    }
    EXPECT_EQ(AppendOnlyLog::load(dir.file("a.aof")),
              (std::vector<std::string>{"SET a 1", "DEL a", "SET b 2"}));
}

TEST(AppendOnlyLogTest, ReopeningKeepsEarlierEntries) {
    TempDir dir;
    Store store;
    {
        AppendOnlyLog log(dir.file("a.aof"));
        log.execute("SET a 1", SetCommand("a", "1"), store);
    }
    {
        AppendOnlyLog log(dir.file("a.aof"));
        log.execute("SET b 2", SetCommand("b", "2"), store);
    }
    EXPECT_EQ(AppendOnlyLog::load(dir.file("a.aof")).size(), 2u);
}

TEST(AppendOnlyLogTest, SyncAlwaysWorks) {
    TempDir dir;
    Store store;
    AppendOnlyLog log(dir.file("a.aof"), AppendOnlyLog::Sync::Always);
    EXPECT_EQ(log.execute("SET a 1", SetCommand("a", "1"), store), "OK");
}

TEST(AppendOnlyLogTest, TornLastLineIsDroppedAndRemovedFromFile) {
    TempDir dir;
    writeFile(dir.file("a.aof"), "SET a 1\nSET b");
    EXPECT_EQ(AppendOnlyLog::load(dir.file("a.aof")), (std::vector<std::string>{"SET a 1"}));
    EXPECT_EQ(readFile(dir.file("a.aof")), "SET a 1\n");
}

TEST(AppendOnlyLogTest, EntriesAfterATornLineDoNotGetGluedToIt) {
    TempDir dir;
    writeFile(dir.file("a.aof"), "SET a 1\nSET b");
    AppendOnlyLog::load(dir.file("a.aof"));

    Store store;
    {
        AppendOnlyLog log(dir.file("a.aof"));
        log.execute("SET c 3", SetCommand("c", "3"), store);
    }
    EXPECT_EQ(AppendOnlyLog::load(dir.file("a.aof")),
              (std::vector<std::string>{"SET a 1", "SET c 3"}));
}

TEST(AppendOnlyLogTest, FileWithoutAnyNewlineLoadsAsEmpty) {
    TempDir dir;
    writeFile(dir.file("a.aof"), "SET a");
    EXPECT_TRUE(AppendOnlyLog::load(dir.file("a.aof")).empty());
    EXPECT_EQ(readFile(dir.file("a.aof")), "");
}

TEST(AppendOnlyLogTest, BlankLinesAreIgnored) {
    TempDir dir;
    writeFile(dir.file("a.aof"), "SET a 1\n\n\nSET b 2\n");
    EXPECT_EQ(AppendOnlyLog::load(dir.file("a.aof")).size(), 2u);
}

TEST(AppendOnlyLogTest, OpeningInMissingDirectoryThrows) {
    TempDir dir;
    EXPECT_THROW(AppendOnlyLog(dir.file("no/such/dir/a.aof")), std::system_error);
}

TEST(AppendOnlyLogTest, FailedWriteMeansTheCommandIsNotApplied) {
    if (!std::filesystem::exists("/dev/full")) {
        GTEST_SKIP() << "/dev/full is not available";
    }
    Store store;
    AppendOnlyLog log("/dev/full");
    EXPECT_THROW(log.execute("SET a 1", SetCommand("a", "1"), store), std::system_error);
    EXPECT_FALSE(store.get("a").has_value());
}

TEST(AppendOnlyLogTest, RewriteKeepsOnlyTheCurrentState) {
    TempDir dir;
    Store store;
    AppendOnlyLog log(dir.file("a.aof"));
    for (int i = 0; i < 50; ++i) {
        log.execute("SET k " + std::to_string(i), SetCommand("k", std::to_string(i)), store);
    }
    log.execute("SET gone 1", SetCommand("gone", "1"), store);
    log.execute("DEL gone", DelCommand("gone"), store);

    log.rewrite(store);
    EXPECT_EQ(AppendOnlyLog::load(dir.file("a.aof")), (std::vector<std::string>{"SET k 49"}));
}

TEST(AppendOnlyLogTest, RewriteStoresTheRemainingTtl) {
    TempDir dir;
    ManualClock clock;
    Store store(clock);
    AppendOnlyLog log(dir.file("a.aof"));
    store.set("k", "v");
    store.expire("k", std::chrono::seconds(10));
    clock.advance(std::chrono::milliseconds(2500));

    log.rewrite(store);
    EXPECT_EQ(AppendOnlyLog::load(dir.file("a.aof")),
              (std::vector<std::string>{"SET k v", "EXPIRE k 8"}));
}

TEST(AppendOnlyLogTest, RewriteSkipsExpiredKeys) {
    TempDir dir;
    ManualClock clock;
    Store store(clock);
    AppendOnlyLog log(dir.file("a.aof"));
    store.set("old", "v");
    store.expire("old", std::chrono::seconds(1));
    store.set("new", "v");
    clock.advance(std::chrono::seconds(2));

    log.rewrite(store);
    EXPECT_EQ(AppendOnlyLog::load(dir.file("a.aof")), (std::vector<std::string>{"SET new v"}));
}

TEST(AppendOnlyLogTest, AppendingStillWorksAfterRewrite) {
    TempDir dir;
    Store store;
    AppendOnlyLog log(dir.file("a.aof"));
    log.execute("SET a 1", SetCommand("a", "1"), store);
    log.rewrite(store);
    log.execute("SET b 2", SetCommand("b", "2"), store);

    auto lines = AppendOnlyLog::load(dir.file("a.aof"));
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "SET a 1");
    EXPECT_EQ(lines[1], "SET b 2");
    EXPECT_FALSE(std::filesystem::exists(dir.file("a.aof.tmp")));
}

TEST(AppendOnlyLogTest, FailedRewriteKeepsTheOldLog) {
    TempDir dir;
    Store store;
    AppendOnlyLog log(dir.file("a.aof"));
    log.execute("SET a 1", SetCommand("a", "1"), store);

    // A directory in the way of the temporary file makes the rewrite fail.
    std::filesystem::create_directory(dir.file("a.aof.tmp"));
    EXPECT_THROW(log.rewrite(store), std::system_error);
    EXPECT_EQ(AppendOnlyLog::load(dir.file("a.aof")), (std::vector<std::string>{"SET a 1"}));
}

TEST(AppendOnlyLogTest, ConcurrentWritersNeverInterleaveEntries) {
    TempDir dir;
    Store store;
    constexpr int kThreads = 8;
    constexpr int kPerThread = 200;
    {
        AppendOnlyLog log(dir.file("a.aof"));
        std::vector<std::thread> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&, t] {
                for (int i = 0; i < kPerThread; ++i) {
                    std::string key = "k" + std::to_string(t) + "_" + std::to_string(i);
                    log.execute("SET " + key + " v", SetCommand(key, "v"), store);
                }
            });
        }
        for (auto& th : threads) th.join();
    }
    auto lines = AppendOnlyLog::load(dir.file("a.aof"));
    EXPECT_EQ(lines.size(), static_cast<std::size_t>(kThreads * kPerThread));
    for (const auto& line : lines) {
        EXPECT_EQ(line.rfind("SET k", 0), 0u) << line;
        EXPECT_EQ(line.substr(line.size() - 2), " v") << line;
    }
}

TEST(AppendOnlyLogTest, LogOrderMatchesApplyOrderForTheSameKey) {
    TempDir dir;
    Store store;
    {
        AppendOnlyLog log(dir.file("a.aof"));
        std::vector<std::thread> threads;
        for (int t = 0; t < 8; ++t) {
            threads.emplace_back([&, t] {
                for (int i = 0; i < 200; ++i) {
                    std::string value = std::to_string(t * 1000 + i);
                    log.execute("SET shared " + value, SetCommand("shared", value), store);
                }
            });
        }
        for (auto& th : threads) th.join();
    }
    // If the log order differed from the apply order, replaying it would end with another value.
    Store replayed;
    replayLog(AppendOnlyLog::load(dir.file("a.aof")), replayed);
    EXPECT_EQ(*replayed.get("shared"), *store.get("shared"));
}

TEST(AppendOnlyLogTest, LoadIgnoresFilesThatAreNotRegular) {
    if (!std::filesystem::exists("/dev/zero")) {
        GTEST_SKIP() << "/dev/zero is not available";
    }
    EXPECT_TRUE(AppendOnlyLog::load("/dev/zero").empty());
    EXPECT_TRUE(AppendOnlyLog::load("/dev/null").empty());
}

namespace {

/// Limits the size of files this process may write, so a write can stop halfway.
class FileSizeLimit {
public:
    explicit FileSizeLimit(rlim_t bytes) {
        getrlimit(RLIMIT_FSIZE, &old_);
        oldHandler_ = signal(SIGXFSZ, SIG_IGN);
        rlimit limit = old_;
        limit.rlim_cur = bytes;
        setrlimit(RLIMIT_FSIZE, &limit);
    }
    ~FileSizeLimit() {
        setrlimit(RLIMIT_FSIZE, &old_);
        signal(SIGXFSZ, oldHandler_);
    }

private:
    rlimit old_{};
    void (*oldHandler_)(int) = SIG_DFL;
};

}  // namespace

TEST(AppendOnlyLogTest, HalfWrittenEntryIsRolledBack) {
    TempDir dir;
    Store store;
    AppendOnlyLog log(dir.file("a.aof"));
    log.execute("SET a 1", SetCommand("a", "1"), store);
    {
        // 8 bytes are already in the file; this entry only fits partially.
        FileSizeLimit limit(20);
        const std::string line = "SET longkey " + std::string(30, 'x');
        EXPECT_THROW(log.execute(line, SetCommand("longkey", std::string(30, 'x')), store),
                     std::system_error);
    }
    EXPECT_EQ(readFile(dir.file("a.aof")), "SET a 1\n");
    EXPECT_FALSE(store.get("longkey").has_value());

    // The next entry starts on a clean line instead of being glued to a fragment.
    log.execute("DEL a", DelCommand("a"), store);
    EXPECT_EQ(AppendOnlyLog::load(dir.file("a.aof")),
              (std::vector<std::string>{"SET a 1", "DEL a"}));
}
