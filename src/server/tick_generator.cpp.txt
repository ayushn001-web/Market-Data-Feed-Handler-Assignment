#include "tick_generator.h"
#include "../../include/protocol.h"
#include <cstring>
#include <cstdio>
#include <chrono>
#include <cmath>

static const char* KNOWN_NAMES[] = {
    "RELIANCE","TCS","INFY","HDFCBANK","ICICIBANK",
    "HINDUNILVR","BAJFINANCE","SBIN","BHARTIARTL","WIPRO",
    "KOTAKBANK","LT","ASIANPAINT","MARUTI","SUNPHARMA",
    "ULTRACEMCO","TITAN","NESTLEIND","HCLTECH","POWERGRID"
};
static constexpr size_t N_KNOWN = sizeof(KNOWN_NAMES) / sizeof(KNOWN_NAMES[0]);

TickGenerator::TickGenerator(size_t num_symbols)
    : rng_(std::random_device{}())
{
    std::uniform_real_distribution<> price_dist(100.0, 5000.0);
    std::uniform_real_distribution<> sigma_dist(0.01, 0.06);
    std::uniform_real_distribution<> mu_dist(-0.02, 0.05);

    syms_.resize(num_symbols);
    for (size_t i = 0; i < num_symbols; ++i) {
        auto& s  = syms_[i];
        s.id     = static_cast<uint16_t>(i);
        s.price  = price_dist(rng_);
        s.sigma  = sigma_dist(rng_);
        s.mu     = mu_dist(rng_);
        s.dt     = 0.001;
        s.base_vol = 500 + static_cast<uint32_t>(rng_() % 9500);

        if (i < N_KNOWN)
            strncpy(s.name, KNOWN_NAMES[i], 15);
        else
            snprintf(s.name, 16, "SYM%03zu", i);
    }
}

double TickGenerator::box_muller() {
    // Generate standard normal using Box-Muller
    // U1, U2 must be in (0,1) — avoid log(0)
    double u1 = (static_cast<double>(rng_()) + 1.0) /
                (static_cast<double>(rng_.max()) + 2.0);
    double u2 = (static_cast<double>(rng_()) + 1.0) /
                (static_cast<double>(rng_.max()) + 2.0);
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
}

size_t TickGenerator::generate(uint16_t symbol_id, uint8_t* buf, uint32_t seq_num) {
    if (symbol_id >= syms_.size()) return 0;
    auto& s = syms_[symbol_id];

    // GBM: dS = mu*S*dt + sigma*S*dW  where dW = Z * sqrt(dt)
    double dW = box_muller() * std::sqrt(s.dt);
    double dS = s.mu * s.price * s.dt + s.sigma * s.price * dW;
    s.price  += dS;
    if (s.price < 0.5) s.price = 0.5;  // shouldn't happen but just in case

    uint64_t ts = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    bool is_trade = (rng_() % 10) < 3;  // 30% trades, 70% quotes

    if (is_trade) {
        proto::TradeMsg msg{};
        msg.hdr.msg_type     = proto::MSG_TRADE;
        msg.hdr.seq_num      = seq_num;
        msg.hdr.timestamp_ns = ts;
        msg.hdr.symbol_id    = symbol_id;
        msg.data.price       = s.price;
        // volume weakly correlated with magnitude of price move
        msg.data.quantity    = s.base_vol +
            static_cast<uint32_t>(std::fabs(dS / s.price) * 2e5);
        msg.checksum = proto::compute_checksum(&msg, sizeof(msg) - sizeof(uint32_t));
        memcpy(buf, &msg, sizeof(msg));
        return sizeof(msg);
    } else {
        double spread = s.price * spread_dist_(rng_);
        proto::QuoteMsg msg{};
        msg.hdr.msg_type     = proto::MSG_QUOTE;
        msg.hdr.seq_num      = seq_num;
        msg.hdr.timestamp_ns = ts;
        msg.hdr.symbol_id    = symbol_id;
        msg.data.bid_price   = s.price - spread * 0.5;
        msg.data.ask_price   = s.price + spread * 0.5;
        msg.data.bid_qty     = 100 + static_cast<uint32_t>(rng_() % 900);
        msg.data.ask_qty     = 100 + static_cast<uint32_t>(rng_() % 900);
        msg.checksum = proto::compute_checksum(&msg, sizeof(msg) - sizeof(uint32_t));
        memcpy(buf, &msg, sizeof(msg));
        return sizeof(msg);
    }
}