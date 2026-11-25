#include "miniredis/command_factory.h"
#include "miniredis/server.h"
#include "miniredis/store.h"

#include <cstdint>
#include <exception>
#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <string>

int main(int argc, char** argv) {
    std::uint16_t port = 6380;
    if (argc > 1) {
        try {
            port = static_cast<std::uint16_t>(std::stoul(argv[1]));
        } catch (const std::exception&) {
            std::cerr << "usage: " << argv[0] << " [port]\n";
            return 2;
        }
    }

    // Block the signals before any thread is created, so every thread inherits the mask
    // and only sigwait() below receives them.
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &signals, nullptr);

    try {
        miniredis::Store store;
        miniredis::CommandFactory factory;
        miniredis::Server server(store, factory, 8);

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
