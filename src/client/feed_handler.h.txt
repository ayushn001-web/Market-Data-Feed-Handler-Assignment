#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include "socket.h"
#include "parser.h"
#include "visualizer.h"
#include "../common/cache.h"
#include "../common/latency_tracker.h"

class FeedHandler {
public:
    FeedHandler(const std::string& host, uint16_t port);
    ~FeedHandler();

    void start();
    void run();    // blocking main loop
    void stop();

private:
    void recv_loop();
    void reconnect_with_backoff();
    void on_message(const ParsedMsg& msg);
    DisplayStats get_display_stats();

    std::string  host_;
    uint16_t     port_;

    MarketDataSocket  socket_;
    SymbolCache       cache_;
    LatencyTracker    latency_;
    BinaryParser*     parser_{nullptr};
    Visualizer*       viz_{nullptr};

    std::atomic<bool>     running_{false};
    std::atomic<uint64_t> total_msgs_{0};
    std::atomic<uint64_t> seq_gaps_{0};
    std::atomic<uint64_t> chk_errors_{0};

    std::chrono::steady_clock::time_point start_time_;

    // For rate calculation
    uint64_t last_msg_count_{0};
    std::chrono::steady_clock::time_point last_rate_time_;
    double   current_rate_{0.0};

    // Reconnect state
    int reconnect_attempts_{0};
    static constexpr int MAX_BACKOFF_MS = 30000;
};