#include "visualizer.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>
#include <sys/ioctl.h>
#include <unistd.h>

// ANSI color codes
#define ANSI_RESET   "\033[0m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_RED     "\033[31m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_CLEAR   "\033[2J"
#define ANSI_HOME    "\033[H"

Visualizer::Visualizer(const SymbolCache& cache,
                       std::function<DisplayStats()> stats_fn)
    : cache_(cache), stats_fn_(std::move(stats_fn)) {}

Visualizer::~Visualizer() { stop(); }

void Visualizer::start() {
    running_.store(true);
    display_thread_ = std::thread(&Visualizer::display_loop, this);
}

void Visualizer::stop() {
    running_.store(false);
    if (display_thread_.joinable()) display_thread_.join();
}

void Visualizer::clear_screen() {
    printf(ANSI_CLEAR ANSI_HOME);
}

void Visualizer::display_loop() {
    while (running_.load()) {
        auto t0 = std::chrono::steady_clock::now();

        DisplayStats stats = stats_fn_();
        render(stats);

        auto elapsed = std::chrono::steady_clock::now() - t0;
        auto sleep_time = std::chrono::milliseconds(500) - elapsed;
        if (sleep_time.count() > 0)
            std::this_thread::sleep_for(sleep_time);
    }
}

void Visualizer::render(const DisplayStats& stats) {
    auto snapshots = cache_.get_all_snapshots();

    // Sort by update_count descending — top 20 most active
    std::vector<size_t> indices(SymbolCache::MAX_SYMBOLS);
    for (size_t i = 0; i < indices.size(); ++i) indices[i] = i;
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return snapshots[a].update_count > snapshots[b].update_count;
    });

    // Capture open prices once (first non-zero price seen)
    if (!prices_captured_) {
        bool any = false;
        for (size_t i = 0; i < SymbolCache::MAX_SYMBOLS; ++i) {
            if (snapshots[i].open_price > 0.0) {
                open_prices_[i] = snapshots[i].open_price;
                any = true;
            }
        }
        if (any) prices_captured_ = true;
    }

    // Get terminal width for handle resize
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    int term_cols = (ws.ws_col > 0) ? ws.ws_col : 120;

    clear_screen();

    // Header
    uint64_t h   = stats.uptime_sec / 3600;
    uint64_t m   = (stats.uptime_sec % 3600) / 60;
    uint64_t s   = stats.uptime_sec % 60;

    printf(ANSI_BOLD "=== NSE Market Data Feed Handler ===\n" ANSI_RESET);
    printf("Connected to: localhost:9876\n");
    printf("Uptime: %02llu:%02llu:%02llu | Messages: %llu | Rate: %.0f msg/s\n\n",
           (unsigned long long)h, (unsigned long long)m, (unsigned long long)s,
           (unsigned long long)stats.total_msgs, stats.msg_rate);

    printf(ANSI_BOLD "%-12s %10s %10s %10s %12s %8s %10s\n" ANSI_RESET,
           "Symbol", "Bid", "Ask", "LTP", "Volume", "Chg%", "Updates");

    for (int i = 0; i < term_cols && i < 80; ++i) printf("─");
    printf("\n");

    int shown = 0;
    for (size_t idx : indices) {
        if (shown >= 20) break;
        const auto& st = snapshots[idx];
        if (st.update_count == 0) continue;
        if (st.symbol_name[0] == '\0') continue;

        double open = open_prices_[idx];
        double ltp  = st.last_traded_price > 0 ? st.last_traded_price
                                                : (st.best_bid + st.best_ask) * 0.5;
        double chg_pct = (open > 0) ? ((ltp - open) / open) * 100.0 : 0.0;

        const char* color = (chg_pct >= 0) ? ANSI_GREEN : ANSI_RED;
        char chg_str[16];
        snprintf(chg_str, sizeof(chg_str), "%+.2f%%", chg_pct);

        printf("%-12s %10.2f %10.2f %10.2f %12u %s%8s" ANSI_RESET " %10llu\n",
               st.symbol_name,
               st.best_bid,
               st.best_ask,
               ltp,
               st.last_traded_quantity,
               color, chg_str,
               (unsigned long long)st.update_count);
        ++shown;
    }

    printf("\n" ANSI_BOLD "Statistics:\n" ANSI_RESET);
    printf("  Parser Throughput:   %.0f msg/s\n", stats.msg_rate);
    printf("  End-to-End Latency:  p50=%lluμs  p99=%lluμs  p999=%lluμs\n",
           (unsigned long long)stats.p50_us,
           (unsigned long long)stats.p99_us,
           (unsigned long long)stats.p999_us);
    printf("  Sequence Gaps:       %llu\n", (unsigned long long)stats.seq_gaps);
    printf("  Checksum Errors:     %llu\n", (unsigned long long)stats.chk_errors);
    printf("  Cache Updates:       %llu\n", (unsigned long long)stats.total_msgs);
    printf("\nPress 'q' to quit, 'r' to reset stats\n");

    fflush(stdout);
}