#include "miniredis/append_only_log.h"
#include "miniredis/command_factory.h"
#include "miniredis/commands.h"
#include "miniredis/persistence.h"
#include "miniredis/policy_factory.h"
#include "miniredis/pubsub.h"
#include "miniredis/server.h"
#include "miniredis/store.h"

#include <CLI/CLI.hpp>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <string>

int main(int argc, char** argv) {
    std::uint16_t port = 6380;
    std::size_t threads = 8;
    std::size_t capacity = 0;
    std::string policy = "lru";
    std::string aofPath;
    bool aofFsync = false;

    CLI::App app{"In-memory key-value server"};
    app.add_option("-p,--port", port, "TCP port on 127.0.0.1 (0 picks a free one)")
        ->capture_default_str();
    app.add_option("-t,--threads", threads,
                   "Worker threads, which is also the maximum number of clients")
        ->check(CLI::Range(1, 1024))
        ->capture_default_str();
    app.add_option("-c,--capacity", capacity, "Maximum number of keys, 0 means unlimited")
        ->capture_default_str();
    app.add_option("--policy", policy, "Eviction policy used when the capacity is reached")
        ->check(CLI::IsMember({"lru", "lfu"}, CLI::ignore_case))
        ->capture_default_str();
    app.add_option("--aof", aofPath,
                   "Append-only file: commands are logged there and replayed at start");
    app.add_flag("--aof-fsync", aofFsync,
                 "fsync the append-only file after every command (slower, safer)")
        ->needs("--aof");
    CLI11_PARSE(app, argc, argv);

    // Block the signals before any thread is created, so every thread inherits the mask
    // and only sigwait() below receives them.
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &signals, nullptr);

    try {
        miniredis::Store store =
            capacity > 0 ? miniredis::Store(capacity, miniredis::makeEvictionPolicy(policy))
                         : miniredis::Store();
        miniredis::Broker broker;
        miniredis::CommandFactory factory;
        miniredis::registerPublish(factory, broker);

        std::unique_ptr<miniredis::AppendOnlyLog> log;
        if (!aofPath.empty()) {
            auto recovered = miniredis::replayLog(miniredis::AppendOnlyLog::load(aofPath), store);
            std::cout << "recovered " << recovered.applied << " commands from " << aofPath;
            if (recovered.skipped > 0) {
                std::cout << " (" << recovered.skipped << " invalid lines skipped)";
            }
            std::cout << std::endl;
            log = std::make_unique<miniredis::AppendOnlyLog>(
                aofPath, aofFsync ? miniredis::AppendOnlyLog::Sync::Always
                                  : miniredis::AppendOnlyLog::Sync::Never);
            miniredis::registerPersistence(factory, *log);
        }

        miniredis::Server server(store, factory, broker, threads);

        std::cout << "listening on 127.0.0.1:" << server.start(port) << std::endl;

        int received = 0;
        sigwait(&signals, &received);
        server.stop();
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
