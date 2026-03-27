#include "parser.h"
#include <cstring>
#include <cstdio>

BinaryParser::BinaryParser(MsgCallback cb) : cb_(std::move(cb)) {}

size_t BinaryParser::feed(const uint8_t* data, size_t len) {
    if (len == 0) return 0;
    bytes_in_ += len;

    // Append to internal buffer — handle overflow
    if (buf_used_ + len > RING_CAP) {
        // Should not happen in normal operation; drop oldest data
        fprintf(stderr, "[parser] buffer overflow, dropping %zu bytes\n",
                buf_used_ + len - RING_CAP);
        size_t keep = RING_CAP - len;
        memmove(buf_, buf_ + (buf_used_ - keep), keep);
        buf_used_ = keep;
    }

    memcpy(buf_ + buf_used_, data, len);
    buf_used_ += len;

    // Try to parse as many complete messages as possible
    while (try_parse()) {}

    return len;
}

bool BinaryParser::try_parse() {
    // Need at least a full header
    if (buf_used_ < sizeof(proto::Header)) return false;

    const proto::Header* hdr = reinterpret_cast<const proto::Header*>(buf_);

    size_t total = proto::msg_total_size(hdr->msg_type);
    if (total == 0) {
        // Unknown message type — skip one byte and try again (resync)
        memmove(buf_, buf_ + 1, buf_used_ - 1);
        buf_used_--;
        return true;
    }

    if (buf_used_ < total) return false;  // incomplete message, wait for more data

    // Verify checksum
    uint32_t expected = proto::compute_checksum(buf_, total - sizeof(uint32_t));
    uint32_t actual;
    memcpy(&actual, buf_ + total - sizeof(uint32_t), sizeof(uint32_t));

    if (expected != actual) {
        chk_errors_++;
        // Skip this message
        memmove(buf_, buf_ + total, buf_used_ - total);
        buf_used_ -= total;
        return true;
    }

    // Check sequence gap
    if (!first_msg_) {
        uint32_t expected_seq = last_seq_ + 1;
        if (hdr->seq_num > expected_seq) {
            seq_gaps_++;
            // We just log it; no retransmit mechanism over TCP
            // (TCP guarantees delivery, gaps mean server intentionally skipped)
        }
    }
    last_seq_   = hdr->seq_num;
    first_msg_  = false;

    // Build parsed message and invoke callback
    ParsedMsg msg{};
    msg.hdr = *hdr;

    if (hdr->msg_type == proto::MSG_TRADE) {
        memcpy(&msg.payload.trade,
               buf_ + sizeof(proto::Header),
               sizeof(proto::TradePayload));
    } else if (hdr->msg_type == proto::MSG_QUOTE) {
        memcpy(&msg.payload.quote,
               buf_ + sizeof(proto::Header),
               sizeof(proto::QuotePayload));
    }
    // Heartbeat has no payload

    cb_(msg);
    msgs_parsed_++;

    // Consume bytes
    memmove(buf_, buf_ + total, buf_used_ - total);
    buf_used_ -= total;
    return true;
}

void BinaryParser::reset_stats() {
    msgs_parsed_ = seq_gaps_ = chk_errors_ = bytes_in_ = 0;
    first_msg_ = true; last_seq_ = 0;
}