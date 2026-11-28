#include "miniredis/command_factory.h"
#include "miniredis/commands.h"
#include "miniredis/persistence.h"
#include "miniredis/server.h"

#include <filesystem>
#include <memory>
#include <string>

#include "temp_dir.h"
#include "test_client.h"
#include <gtest/gtest.h>

using namespace miniredis;

namespace {

/// A server with the same wiring as main(): recover from the log, then log new commands.
class Node {
public:
    explicit Node(const std::string& logPath) {
        recovered = replayLog(AppendOnlyLog::load(logPath), store);
        log = std::make_unique<AppendOnlyLog>(logPath);
        registerPublish(factory, broker);
        registerPersistence(factory, *log);
        server = std::make_unique<Server>(store, factory, broker, 4);
        port = server->start(0);
    }

    Store store;
    Broker broker;
    CommandFactory factory;
    std::unique_ptr<AppendOnlyLog> log;
    std::unique_ptr<Server> server;
    ReplayResult recovered;
    std::uint16_t port = 0;
};

}  // namespace

TEST(ReplayTest, RebuildsTheStateFromTheCommands) {
    Store store;
    auto result =
        replayLog({"SET a 1", "SET b 2", "DEL a", "SET c 3", "SET c 4", "EXPIRE b 100"}, store);
    EXPECT_EQ(result.applied, 6u);
    EXPECT_EQ(result.skipped, 0u);
    EXPECT_FALSE(store.get("a").has_value());
    EXPECT_EQ(*store.get("b"), "2");
    EXPECT_EQ(*store.get("c"), "4");
}

TEST(ReplayTest, InvalidLinesAreSkippedAndCounted) {
    Store store;
    auto result = replayLog({"SET a 1", "NONSENSE", "SET b", "SET c 3"}, store);
    EXPECT_EQ(result.applied, 2u);
    EXPECT_EQ(result.skipped, 2u);
    EXPECT_EQ(store.size(), 2u);
}

TEST(PersistenceTest, OnlyModifyingCommandsAreLogged) {
    TempDir dir;
    Store store;
    Broker broker;
    CommandFactory factory;
    AppendOnlyLog log(dir.file("a.aof"));
    registerPublish(factory, broker);
    registerPersistence(factory, log);

    factory.create("SET a 1")->execute(store);
    factory.create("GET a")->execute(store);
    factory.create("EXPIRE a 60")->execute(store);
    factory.create("PUBLISH ch hi")->execute(store);
    factory.create("DEL a")->execute(store);

    EXPECT_EQ(AppendOnlyLog::load(dir.file("a.aof")),
              (std::vector<std::string>{"SET a 1", "EXPIRE a 60", "DEL a"}));
}

TEST(PersistenceTest, InvalidCommandsAreNotLogged) {
    TempDir dir;
    CommandFactory factory;
    AppendOnlyLog log(dir.file("a.aof"));
    registerPersistence(factory, log);

    EXPECT_THROW(factory.create("SET a"), ParseError);
    EXPECT_THROW(factory.create("EXPIRE a soon"), ParseError);
    EXPECT_TRUE(AppendOnlyLog::load(dir.file("a.aof")).empty());
}

TEST(PersistenceTest, CommandNamesAreNormalizedInTheLog) {
    TempDir dir;
    Store store;
    CommandFactory factory;
    AppendOnlyLog log(dir.file("a.aof"));
    registerPersistence(factory, log);

    factory.create("  set   a    1 ")->execute(store);
    EXPECT_EQ(AppendOnlyLog::load(dir.file("a.aof")), (std::vector<std::string>{"set a 1"}));
}

TEST(PersistenceTest, CompactCommandRewritesTheLog) {
    TempDir dir;
    Store store;
    CommandFactory factory;
    AppendOnlyLog log(dir.file("a.aof"));
    registerPersistence(factory, log);
    for (int i = 0; i < 20; ++i) {
        factory.create("SET k " + std::to_string(i))->execute(store);
    }
    EXPECT_EQ(factory.create("COMPACT")->execute(store), "OK");
    EXPECT_EQ(AppendOnlyLog::load(dir.file("a.aof")), (std::vector<std::string>{"SET k 19"}));
    EXPECT_THROW(factory.create("COMPACT now"), ParseError);
}

TEST(RecoveryTest, DataSurvivesARestart) {
    TempDir dir;
    const std::string path = dir.file("data.aof");
    {
        Node first(path);
        TestClient c(first.port);
        EXPECT_EQ(c.command("SET name adesz"), "OK");
        EXPECT_EQ(c.command("SET tmp 1"), "OK");
        EXPECT_EQ(c.command("DEL tmp"), "1");
        EXPECT_EQ(c.command("SET city gyor"), "OK");
    }
    Node second(path);
    EXPECT_EQ(second.recovered.applied, 4u);
    TestClient c(second.port);
    EXPECT_EQ(c.command("GET name"), "adesz");
    EXPECT_EQ(c.command("GET city"), "gyor");
    EXPECT_EQ(c.command("GET tmp"), "(nil)");
}

TEST(RecoveryTest, MultiWordValuesSurviveARestartAndACompaction) {
    TempDir dir;
    const std::string path = dir.file("data.aof");
    {
        Node first(path);
        TestClient c(first.port);
        EXPECT_EQ(c.command("SET motto keep it simple"), "OK");
        EXPECT_EQ(c.command("COMPACT"), "OK");
    }
    Node second(path);
    TestClient c(second.port);
    EXPECT_EQ(c.command("GET motto"), "keep it simple");
}

TEST(RecoveryTest, CrashDuringAWriteLosesOnlyTheUnfinishedCommand) {
    TempDir dir;
    const std::string path = dir.file("data.aof");
    {
        Node first(path);
        TestClient c(first.port);
        c.command("SET a 1");
        c.command("SET b 2");
    }
    // The process died halfway through writing the third entry.
    writeFile(path, readFile(path) + "SET c 3");

    {
        Node second(path);
        EXPECT_EQ(second.recovered.applied, 2u);
        TestClient c(second.port);
        EXPECT_EQ(c.command("GET a"), "1");
        EXPECT_EQ(c.command("GET c"), "(nil)");
        EXPECT_EQ(c.command("SET d 4"), "OK");
    }
    Node third(path);
    EXPECT_EQ(third.recovered.applied, 3u);
    EXPECT_EQ(third.recovered.skipped, 0u);
    TestClient c(third.port);
    EXPECT_EQ(c.command("GET d"), "4");
}

TEST(RecoveryTest, CompactedLogRecoversTheSameState) {
    TempDir dir;
    const std::string path = dir.file("data.aof");
    {
        Node first(path);
        TestClient c(first.port);
        for (int i = 0; i < 30; ++i) {
            c.command("SET counter " + std::to_string(i));
        }
        c.command("SET keep yes");
        c.command("SET drop yes");
        c.command("DEL drop");
        EXPECT_EQ(c.command("COMPACT"), "OK");
        EXPECT_EQ(c.command("SET after compact"), "OK");
    }
    Node second(path);
    EXPECT_EQ(second.recovered.applied, 3u);
    TestClient c(second.port);
    EXPECT_EQ(c.command("GET counter"), "29");
    EXPECT_EQ(c.command("GET keep"), "yes");
    EXPECT_EQ(c.command("GET drop"), "(nil)");
    EXPECT_EQ(c.command("GET after"), "compact");
}

TEST(RecoveryTest, ExpiryIsRecovered) {
    TempDir dir;
    const std::string path = dir.file("data.aof");
    {
        Node first(path);
        TestClient c(first.port);
        c.command("SET session abc");
        EXPECT_EQ(c.command("EXPIRE session 3600"), "1");
    }
    Node second(path);
    TestClient c(second.port);
    EXPECT_EQ(c.command("GET session"), "abc");
    EXPECT_EQ(c.command("EXPIRE session 1"), "1");
}

TEST(RecoveryTest, FailingDiskIsReportedAndTheCommandIsNotApplied) {
    if (!std::filesystem::exists("/dev/full")) {
        GTEST_SKIP() << "/dev/full is not available";
    }
    Node node("/dev/full");
    TestClient c(node.port);
    EXPECT_EQ(c.command("SET a 1").rfind("ERR", 0), 0u);
    EXPECT_EQ(c.command("GET a"), "(nil)");
    EXPECT_EQ(c.command("PUBLISH ch hi"), "0");
}
