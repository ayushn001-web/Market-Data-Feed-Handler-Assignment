#include "cache.h"
#include <thread>

SymbolCache::SymbolCache() {
    for (auto& e : cache_) {
        e.seq.store(0, std::memory_order_relaxed);
        memset(&e.state, 0, sizeof(MarketState));
    }
}

void SymbolCache::set_symbol_name(uint16_t id, const char* name) {
    if (id >= MAX_SYMBOLS) return;
    strncpy(cache_[id].state.symbol_name, name, 15);
}

// Seqlock write: bump seq to odd, update, bump to even
void SymbolCache::update_quote(uint16_t id, double bid, uint32_t bqty,
                                double ask, uint32_t aqty, uint64_t ts_ns) {
    if (id >= MAX_SYMBOLS) return;
    auto& e = cache_[id];

    e.seq.fetch_add(1, std::memory_order_release);  // -> odd

    e.state.best_bid      = bid;
    e.state.bid_quantity  = bqty;
    e.state.best_ask      = ask;
    e.state.ask_quantity  = aqty;
    e.state.last_update_time = ts_ns;
    e.state.update_count++;
    if (e.state.open_price == 0.0)
        e.state.open_price = (bid + ask) * 0.5;

    e.seq.fetch_add(1, std::memory_order_release);  // -> even
}

void SymbolCache::update_trade(uint16_t id, double price, uint32_t qty, uint64_t ts_ns) {
    if (id >= MAX_SYMBOLS) return;
    auto& e = cache_[id];

    e.seq.fetch_add(1, std::memory_order_release);

    e.state.last_traded_price    = price;
    e.state.last_traded_quantity = qty;
    e.state.last_update_time     = ts_ns;
    e.state.update_count++;
    if (e.state.open_price == 0.0)
        e.state.open_price = price;

    e.seq.fetch_add(1, std::memory_order_release);
}

MarketState SymbolCache::get_snapshot(uint16_t id) const {
    if (id >= MAX_SYMBOLS) return {};
    const auto& e = cache_[id];
    MarketState snap;

    while (true) {
        uint32_t s1 = e.seq.load(std::memory_order_acquire);
        if (s1 & 1) {
            // write in progress — spin briefly
            std::this_thread::yield();
            continue;
        }
        memcpy(&snap, &e.state, sizeof(MarketState));
        std::atomic_thread_fence(std::memory_order_acquire);
        uint32_t s2 = e.seq.load(std::memory_order_relaxed);
        if (s1 == s2) break;
        // write happened during our copy — retry
    }
    return snap;
}

std::array<MarketState, SymbolCache::MAX_SYMBOLS> SymbolCache::get_all_snapshots() const {
    std::array<MarketState, MAX_SYMBOLS> result;
    for (size_t i = 0; i < MAX_SYMBOLS; ++i)
        result[i] = get_snapshot(static_cast<uint16_t>(i));
    return result;
}