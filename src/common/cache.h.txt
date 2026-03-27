#pragma once
#include <cstdint>
#include <atomic>
#include <cstring>
#include <array>

// MarketState holds the latest snapshot for one symbol.
struct MarketState {
    double   best_bid{0.0};
    double   best_ask{0.0};
    uint32_t bid_quantity{0};
    uint32_t ask_quantity{0};
    double   last_traded_price{0.0};
    uint32_t last_traded_quantity{0};
    uint64_t last_update_time{0};
    uint64_t update_count{0};
    double   open_price{0.0};   // for % change calculation
    char     symbol_name[16]{};
};

// Per-entry seqlock wrapper.
// seq even  => stable, safe to read
// seq odd   => write in progress, reader must retry
struct alignas(64) CacheEntry {
    std::atomic<uint32_t> seq{0};
    char _pad[4];
    MarketState state;
};

// Lock-free symbol cache: single writer (feed handler), multiple readers (visualizer).
// Uses seqlock pattern — no mutex in the hot path.
class SymbolCache {
public:
    static constexpr size_t MAX_SYMBOLS = 500;

    SymbolCache();

    void set_symbol_name(uint16_t id, const char* name);
    void update_quote(uint16_t id, double bid, uint32_t bqty,
                      double ask, uint32_t aqty, uint64_t ts_ns);
    void update_trade(uint16_t id, double price, uint32_t qty, uint64_t ts_ns);

    // Returns a consistent snapshot. Spins briefly if a write is in progress.
    MarketState get_snapshot(uint16_t id) const;

    // For visualizer: returns a copy of all entries (sorted by update_count desc)
    std::array<MarketState, MAX_SYMBOLS> get_all_snapshots() const;

private:
    std::array<CacheEntry, MAX_SYMBOLS> cache_;
};