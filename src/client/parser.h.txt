#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
#include "../../include/protocol.h"

// Parsed message delivered to handler callback
struct ParsedMsg {
    proto::Header hdr;
    union {
        proto::TradePayload  trade;
        proto::QuotePayload  quote;
    } payload;
};

// Callback type invoked for each fully parsed message
using MsgCallback = std::function<void(const ParsedMsg&)>;

// Streaming binary parser — handles TCP fragmentation.
// Uses an internal ring buffer; no dynamic allocation in hot path.
class BinaryParser {
public:
    static constexpr size_t RING_CAP = 65536;  // must be power of 2

    explicit BinaryParser(MsgCallback cb);

    // Feed raw bytes from recv() into parser.
    // Returns number of bytes consumed (always == len unless fatal error).
    size_t feed(const uint8_t* data, size_t len);

    // Statistics
    uint64_t messages_parsed()  const { return msgs_parsed_; }
    uint64_t sequence_gaps()    const { return seq_gaps_; }
    uint64_t checksum_errors()  const { return chk_errors_; }
    uint64_t bytes_consumed()   const { return bytes_in_; }
    void     reset_stats();

private:
    bool try_parse();  // attempt to extract one complete message from ring

    MsgCallback cb_;

    // Linear buffer — simpler than a true ring for message reassembly
    uint8_t  buf_[RING_CAP];
    size_t   buf_used_{0};

    uint32_t last_seq_{0};
    bool     first_msg_{true};

    uint64_t msgs_parsed_{0};
    uint64_t seq_gaps_{0};
    uint64_t chk_errors_{0};
    uint64_t bytes_in_{0};
};