#include "miniredis/command_factory.h"

#include <gtest/gtest.h>

using namespace miniredis;

TEST(CommandFactoryTest, ParsesSetGetDel) {
    CommandFactory f;
    Store s;
    EXPECT_EQ(f.create("SET a 1")->execute(s), "OK");
    EXPECT_EQ(f.create("GET a")->execute(s), "1");
    EXPECT_EQ(f.create("DEL a")->execute(s), "1");
}

TEST(CommandFactoryTest, CommandNamesAreCaseInsensitive) {
    CommandFactory f;
    Store s;
    f.create("set a 1")->execute(s);
    EXPECT_EQ(f.create("gEt a")->execute(s), "1");
}

TEST(CommandFactoryTest, ExtraWhitespaceIsIgnored) {
    CommandFactory f;
    Store s;
    EXPECT_EQ(f.create("  SET   a    1  ")->execute(s), "OK");
}

TEST(CommandFactoryTest, RejectsEmptyLine) {
    CommandFactory f;
    EXPECT_THROW(f.create(""), ParseError);
    EXPECT_THROW(f.create("   "), ParseError);
}

TEST(CommandFactoryTest, RejectsUnknownCommand) {
    CommandFactory f;
    EXPECT_THROW(f.create("FLY a"), ParseError);
}

TEST(CommandFactoryTest, RejectsWrongArgumentCount) {
    CommandFactory f;
    EXPECT_THROW(f.create("SET a"), ParseError);
    EXPECT_THROW(f.create("GET"), ParseError);
    EXPECT_THROW(f.create("DEL a b"), ParseError);
}

TEST(CommandFactoryTest, CustomCommandCanBeRegistered) {
    struct Ping : Command {
        std::string execute(Store&) const override { return "PONG"; }
    };
    CommandFactory f;
    f.registerCommand("PING", [](const CommandFactory::Args&) { return std::make_unique<Ping>(); });
    Store s;
    EXPECT_EQ(f.create("ping")->execute(s), "PONG");
}

TEST(CommandFactoryTest, ParsesExpire) {
    CommandFactory f;
    Store s;
    s.set("a", "1");
    EXPECT_EQ(f.create("EXPIRE a 30")->execute(s), "1");
}

TEST(CommandFactoryTest, RejectsInvalidExpireArguments) {
    CommandFactory f;
    EXPECT_THROW(f.create("EXPIRE a"), ParseError);
    EXPECT_THROW(f.create("EXPIRE a soon"), ParseError);
    EXPECT_THROW(f.create("EXPIRE a 10s"), ParseError);
    EXPECT_THROW(f.create("EXPIRE a 0"), ParseError);
    EXPECT_THROW(f.create("EXPIRE a -5"), ParseError);
}
