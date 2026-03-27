#include <gtest/gtest.h>
#include "../src/common/cache.h"
#include <thread>
#include <atomic>

TEST(CacheTest, UpdateAndReadQuote) {
    SymbolCache cache;
    cache.update_quote(0, 1499.50, 100, 1500.50, 200, 9999ULL);
    auto s = cache.get_snapshot(0);
    EXPECT_DOUBLE_EQ(s.best_bid, 1499.50);
    EXPECT_DOUBLE_EQ(s.best_ask, 1500.50);
    EXPECT_EQ(s.bid_quantity, 100u);
}

TEST(CacheTest, UpdateAndReadTrade) {
    SymbolCache cache;
    cache.update_trade(5, 2340.0, 500, 12345ULL);
    auto s = cache.get_snapshot(5);
    EXPECT_DOUBLE_EQ(s.last_traded_price, 2340.0);
    EXPECT_EQ(s.last_traded_quantity, 500u);
}

TEST(CacheTest, OutOfBoundsIgnored) {
    SymbolCache cache;
    // Should not crash
    cache.update_quote(600, 100.0, 10, 101.0, 10, 0ULL);
    auto s = cache.get_snapshot(600);
    EXPECT_DOUBLE_EQ(s.best_bid, 0.0);
}

TEST(CacheTest, ConcurrentWriteRead) {
    SymbolCache cache;
    std::atomic<bool> go{false};
    std::atomic<int>  reads{0};

    std::thread writer([&]() {
        while (!go.load()) {}
        for (int i = 0; i < 50000; ++i)
            cache.update_quote(0, 100.0 + i, 10, 101.0 + i, 10, static_cast<uint64_t>(i));
    });

    std::thread reader([&]() {
        while (!go.load()) {}
        for (int i = 0; i < 50000; ++i) {
            auto s = cache.get_snapshot(0);
            // Just ensure bid <= ask (basic consistency)
            if (s.best_bid > 0)
                EXPECT_LE(s.best_bid, s.best_ask + 1.0);
            reads++;
        }
    });

    go.store(true);
    writer.join();
    reader.join();

    EXPECT_EQ(reads.load(), 50000);
}