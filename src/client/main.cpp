#include "feed_handler.h"
#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <termios.h>
#include <unistd.h>

static FeedHandler* g_handler = nullptr;

void signal_handler(int) {
    fprintf(stderr, "\n[client] shutting down...\n");
    if (g_handler) g_handler->stop();
}

int main(int argc, char* argv[]) {
    const char* host = "127.0.0.1";
    uint16_t    port = 9876;

    if (argc > 1) host = argv[1];
    if (argc > 2) port = static_cast<uint16_t>(atoi(argv[2]));

    FeedHandler handler(host, port);
    g_handler = &handler;

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    handler.start();
    handler.run();  // blocks

    return 0;
}