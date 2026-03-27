#pragma once
#include <cstdint>
#include <cstring>

// Shared binary protocol definitions between server and client.
namespace proto {

enum MsgType : uint16_t {
    MSG_TRADE     = 0x0001,
    MSG_QUOTE     = 0x0002,
    MSG_HEARTBEAT = 0x0003
};

#pragma pack(push, 1)

// Fixed 16-byte header for every message
struct Header {
    uint16_t msg_type;
    uint32_t seq_num;
    uint64_t timestamp_ns;
    uint16_t symbol_id;
};

struct TradePayload {
    double   price;       // 8 bytes
    uint32_t quantity;    // 4 bytes
};

struct QuotePayload {
    double   bid_price;   // 8 bytes
    uint32_t bid_qty;     // 4 bytes
    double   ask_price;   // 8 bytes
    uint32_t ask_qty;     // 4 bytes
};

struct TradeMsg {
    Header       hdr;
    TradePayload data;
    uint32_t     checksum;  // XOR of all preceding bytes
};

struct QuoteMsg {
    Header       hdr;
    QuotePayload data;
    uint32_t     checksum;
};

struct HeartbeatMsg {
    Header   hdr;
    uint32_t checksum;
};

#pragma pack(pop)

static_assert(sizeof(Header)       == 16, "Header size mismatch");
static_assert(sizeof(TradeMsg)     == 32, "TradeMsg size mismatch");
static_assert(sizeof(QuoteMsg)     == 44, "QuoteMsg size mismatch");
static_assert(sizeof(HeartbeatMsg) == 20, "HeartbeatMsg size mismatch");

constexpr size_t MAX_MSG_SIZE = sizeof(QuoteMsg);  // 44 bytes
constexpr size_t MAX_SYMBOLS  = 500;

// Compute XOR-based checksum over `len` bytes
inline uint32_t compute_checksum(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t c = 0;
    for (size_t i = 0; i < len; ++i)
        c ^= static_cast<uint32_t>(p[i]) << ((i & 3) << 3);
    return c;
}

// Returns total on-wire message size for a given type (0 = unknown)
inline size_t msg_total_size(uint16_t type) {
    switch (type) {
        case MSG_TRADE:     return sizeof(TradeMsg);
        case MSG_QUOTE:     return sizeof(QuoteMsg);
        case MSG_HEARTBEAT: return sizeof(HeartbeatMsg);
        default:            return 0;
    }
}

// Subscription request sent by client (0xFF header byte, 2-byte count, then symbol id array)
struct SubRequest {
    uint8_t  cmd;    // 0xFF
    uint16_t count;
    // uint16_t symbol_ids[count] follows in wire format
};

} // namespace proto