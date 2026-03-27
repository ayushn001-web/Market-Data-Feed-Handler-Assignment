#include "feed_handler.h"
#include <cstdio>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>
#include <cstring>

FeedHandler::FeedHandler(const std::string& host, uint16_t port)
    : host_(host), port_(port)
{
    parser_ = new BinaryParser([this](const ParsedMsg& m) { on_message(m); });
    viz_    = new Visualizer(cache_, [this]() { return get_display_stats(); });
}

FeedHandler::~FeedHandler() {
    stop();
    delete parser_;
    delete viz_;
}

void FeedHandler::start() {
    start_time_     = std::chrono::steady_clock::now();
    last_rate_time_ = start_time_;
    running_.store(true);
}

void FeedHandler::run() {
    if (!socket_.connect(host_, port_)) {
        fprintf(stderr, "[client] initial connect failed, will retry...\n");
    } else {
        // Subscribe to first 500 symbols
        std::vector<uint16_t> syms;
        for (uint16_t i = 0; i < 500; ++i) syms.push_back(i);
        socket_.send_subscription(syms);
    }

    viz_->start();

    constexpr size_t BUF_SIZE = 65536;
    uint8_t recv_buf[BUF_SIZE];

    while (running_.load()) {
        if (!socket_.is_connected()) {
            reconnect_with_backoff();
            continue;
        }

        bool data_ready = socket_.wait_for_data(100);
        if (!data_ready) continue;

        // Edge-triggered: drain all available data
        while (true) {
            auto t_recv = std::chrono::steady_clock::now();
            ssize_t n = socket_.receive(recv_buf, BUF_SIZE);

            if (n < 0) {
                fprintf(stderr, "[client] socket error / disconnected\n");
                break;
            }
            if (n == 0) break;  // EAGAIN — drained

            // Record recv latency (kernel buffer → userspace)
            auto recv_lat = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - t_recv).count();
            latency_.record(static_cast<uint64_t>(recv_lat));

            parser_->feed(recv_buf, static_cast<size_t>(n));
        }

        // Update rate approximately every second
        auto now   = std::chrono::steady_clock::now();
        double sec = std::chrono::duration<double>(now - last_rate_time_).count();
        if (sec >= 1.0) {
            uint64_t cur = total_msgs_.load();
            current_rate_ = (cur - last_msg_count_) / sec;
            last_msg_count_ = cur;
            last_rate_time_ = now;
        }
    }

    viz_->stop();
}

void FeedHandler::stop() {
    running_.store(false);
    socket_.disconnect();
}

void FeedHandler::on_message(const ParsedMsg& msg) {
    total_msgs_.fetch_add(1, std::memory_order_relaxed);

    uint16_t sym_id = msg.hdr.symbol_id;
    if (sym_id >= SymbolCache::MAX_SYMBOLS) return;

    if (msg.hdr.msg_type == proto::MSG_QUOTE) {
        cache_.update_quote(sym_id,
            msg.payload.quote.bid_price, msg.payload.quote.bid_qty,
            msg.payload.quote.ask_price, msg.payload.quote.ask_qty,
            msg.hdr.timestamp_ns);
    } else if (msg.hdr.msg_type == proto::MSG_TRADE) {
        cache_.update_trade(sym_id,
            msg.payload.trade.price,
            msg.payload.trade.quantity,
            msg.hdr.timestamp_ns);
    }
    // Heartbeat — nothing to update in cache
}

void FeedHandler::reconnect_with_backoff() {
    // Exponential backoff: 500ms, 1s, 2s, 4s ... capped at 30s
    int delay_ms = std::min(500 * (1 << std::min(reconnect_attempts_, 6)),
                            MAX_BACKOFF_MS);
    fprintf(stderr, "[client] reconnecting in %dms (attempt %d)...\n",
            delay_ms, reconnect_attempts_ + 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

    if (socket_.connect(host_, port_)) {
        reconnect_attempts_ = 0;
        // Re-subscribe
        std::vector<uint16_t> syms;
        for (uint16_t i = 0; i < 500; ++i) syms.push_back(i);
        socket_.send_subscription(syms);
        fprintf(stderr, "[client] reconnected successfully\n");
    } else {
        reconnect_attempts_++;
    }
}

DisplayStats FeedHandler::get_display_stats() {
    DisplayStats s{};
    s.total_msgs = total_msgs_.load(std::memory_order_relaxed);
    s.msg_rate   = current_rate_;
    s.seq_gaps   = parser_->sequence_gaps();
    s.chk_errors = parser_->checksum_errors();

    auto lstats = latency_.get_stats();
    // convert ns → μs for display
    s.p50_us  = lstats.p50  / 1000;
    s.p99_us  = lstats.p99  / 1000;
    s.p999_us = lstats.p999 / 1000;

    s.uptime_sec = static_cast<uint64_t>(
        std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start_time_).count());
    return s;
}