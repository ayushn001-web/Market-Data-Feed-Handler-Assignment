#include <gtest/gtest.h>
#include "../src/server/tick_generator.h"
#include "../include/protocol.h"
#include <cstring>
#include <cmath>

TEST(GBMTest, PriceInReasonableRange) {
    TickGenerator gen(10);
    uint8_t buf[proto::MAX_MSG_SIZE];

    for (int i = 0; i < 1000; ++i) {
        uint16_t sym = static_cast<uint16_t>(i % 10);
        size_t n = gen.generate(sym, buf, i);
        ASSERT_GT(n, 0u);

        // Price should stay positive and not blow up
        double price = gen.symbol(sym).price;
        EXPECT_GT(price, 0.0);
        EXPECT_LT(price, 1e7);
    }
}

TEST(GBMTest, ChecksumValid) {
    TickGenerator gen(5);
    uint8_t buf[proto::MAX_MSG_SIZE];

    for (int i = 0; i < 100; ++i) {
        size_t n = gen.generate(0, buf, i);
        ASSERT_GT(n, 0u);

        uint32_t chk;
        memcpy(&chk, buf + n - sizeof(uint32_t), sizeof(uint32_t));
        uint32_t expected = proto::compute_checksum(buf, n - sizeof(uint32_t));
        EXPECT_EQ(chk, expected);
    }
}

TEST(GBMTest, MsgTypeDistribution) {
    TickGenerator gen(1);
    uint8_t buf[proto::MAX_MSG_SIZE];

    int trades = 0, quotes = 0;
    for (int i = 0; i < 1000; ++i) {
        gen.generate(0, buf, i);
        proto::Header hdr;
        memcpy(&hdr, buf, sizeof(hdr));
        if (hdr.msg_type == proto::MSG_TRADE) trades++;
        else if (hdr.msg_type == proto::MSG_QUOTE) quotes++;
    }

    // Should be roughly 30/70 split ± some variance
    EXPECT_GT(trades, 150);
    EXPECT_LT(trades, 450);
    EXPECT_GT(quotes, 550);
}