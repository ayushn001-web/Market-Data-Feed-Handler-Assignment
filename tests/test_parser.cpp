#include <gtest/gtest.h>
#include "../src/client/parser.h"
#include "../include/protocol.h"
#include <vector>
#include <cstring>

// Helper: build a valid QuoteMsg
static proto::QuoteMsg make_quote(uint32_t seq, uint16_t sym) {
    proto::QuoteMsg msg{};
    msg.hdr.msg_type     = proto::MSG_QUOTE;
    msg.hdr.seq_num      = seq;
    msg.hdr.timestamp_ns = 1234567890ULL;
    msg.hdr.symbol_id    = sym;
    msg.data.bid_price   = 1500.25;
    msg.data.bid_qty     = 100;
    msg.data.ask_price   = 1500.75;
    msg.data.ask_qty     = 200;
    msg.checksum = proto::compute_checksum(&msg, sizeof(msg) - sizeof(uint32_t));
    return msg;
}

TEST(ParserTest, ParseSingleQuote) {
    int count = 0;
    BinaryParser p([&](const ParsedMsg& m) {
        EXPECT_EQ(m.hdr.msg_type, proto::MSG_QUOTE);
        EXPECT_DOUBLE_EQ(m.payload.quote.bid_price, 1500.25);
        count++;
    });

    auto msg = make_quote(1, 42);
    p.feed(reinterpret_cast<uint8_t*>(&msg), sizeof(msg));
    EXPECT_EQ(count, 1);
    EXPECT_EQ(p.messages_parsed(), 1ULL);
}

TEST(ParserTest, HandleFragmentation) {
    int count = 0;
    BinaryParser p([&](const ParsedMsg&) { count++; });

    auto msg = make_quote(1, 0);
    const uint8_t* raw = reinterpret_cast<uint8_t*>(&msg);

    // Feed in small 4-byte chunks
    for (size_t i = 0; i < sizeof(msg); i += 4)
        p.feed(raw + i, std::min((size_t)4, sizeof(msg) - i));

    EXPECT_EQ(count, 1);
}

TEST(ParserTest, DetectChecksumError) {
    int count = 0;
    BinaryParser p([&](const ParsedMsg&) { count++; });

    auto msg = make_quote(1, 0);
    msg.checksum ^= 0xDEADBEEF;  // corrupt checksum
    p.feed(reinterpret_cast<uint8_t*>(&msg), sizeof(msg));

    EXPECT_EQ(count, 0);
    EXPECT_EQ(p.checksum_errors(), 1ULL);
}

TEST(ParserTest, ParseMultipleMessages) {
    int count = 0;
    BinaryParser p([&](const ParsedMsg&) { count++; });

    for (int i = 1; i <= 100; ++i) {
        auto msg = make_quote(i, static_cast<uint16_t>(i % 100));
        p.feed(reinterpret_cast<uint8_t*>(&msg), sizeof(msg));
    }
    EXPECT_EQ(count, 100);
}

TEST(ParserTest, DetectSequenceGap) {
    BinaryParser p([](const ParsedMsg&) {});

    auto m1 = make_quote(1, 0);
    auto m2 = make_quote(5, 0);  // gap of 3
    p.feed(reinterpret_cast<uint8_t*>(&m1), sizeof(m1));
    p.feed(reinterpret_cast<uint8_t*>(&m2), sizeof(m2));

    EXPECT_GT(p.sequence_gaps(), 0ULL);
}