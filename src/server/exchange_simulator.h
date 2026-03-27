#pragma once
#include <cstdint>
#include <atomic>
#include <thread>
#include <vector>
#include "tick_generator.h"
#include "client_manager.h"

class ExchangeSimulator {
public:
    ExchangeSimulator(uint16_t port, size_t num_symbols = 100);
    ~ExchangeSimulator();

    void start();
    void run();     // blocking — enter epoll accept/disconnect loop
    void stop();

    void set_tick_rate(uint32_t ticks_per_second);
    void enable_fault_injection(bool enable);

private:
    void handle_new_connection();
    void handle_client_disconnect(int client_fd);
    void tick_loop();       // runs in dedicated thread

    uint16_t          port_;
    int               server_fd_{-1};
    int               epoll_fd_{-1};

    TickGenerator     tick_gen_;
    ClientManager     client_mgr_;

    std::atomic<bool>     running_{false};
    std::atomic<uint32_t> tick_rate_{10000};
    std::atomic<bool>     fault_injection_{false};
    std::atomic<uint32_t> seq_counter_{0};

    std::thread tick_thread_;
};