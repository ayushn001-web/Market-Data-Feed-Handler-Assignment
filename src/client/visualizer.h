#pragma once
#include <cstdint>
#include <atomic>
#include <thread>
#include <functional>
#include "../common/cache.h"

struct DisplayStats {
    uint64_t total_msgs;
    double   msg_rate;
    uint64_t seq_gaps;
    uint64_t chk_errors;
    uint64_t p50_us, p99_us, p999_us;
    uint64_t uptime_sec;
};

class Visualizer {
public:
    Visualizer(const SymbolCache& cache,
               std::function<DisplayStats()> stats_fn);
    ~Visualizer();

    void start();
    void stop();

private:
    void display_loop();
    void render(const DisplayStats& stats);
    void clear_screen();
    void move_cursor(int row, int col);

    const SymbolCache&               cache_;
    std::function<DisplayStats()>    stats_fn_;
    std::atomic<bool>                running_{false};
    std::thread                      display_thread_;

    // For % change calculation — snapshot of price at start
    double open_prices_[SymbolCache::MAX_SYMBOLS]{};
    bool   prices_captured_{false};
};