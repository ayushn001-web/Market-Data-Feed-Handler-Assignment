#include "exchange_simulator.h"
#include <cstdlib>
#include <cstdio>
#include <csignal>

static ExchangeSimulator* g_sim = nullptr;

void signal_handler(int) {
    fprintf(stderr, "\n[server] shutting down...\n");
    if (g_sim) g_sim->stop();
}

int main(int argc, char* argv[]) {
    uint16_t port       = 9876;
    size_t   num_sym    = 100;
    uint32_t tick_rate  = 10000;

    if (argc > 1) port      = static_cast<uint16_t>(atoi(argv[1]));
    if (argc > 2) num_sym   = static_cast<size_t>(atoi(argv[2]));
    if (argc > 3) tick_rate = static_cast<uint32_t>(atoi(argv[3]));

    ExchangeSimulator sim(port, num_sym);
    g_sim = &sim;

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    sim.set_tick_rate(tick_rate);
    sim.start();
    sim.run();   // blocks until stopped

    return 0;
}