#include "latency_tracker.h"
#include <fstream>
#include <algorithm>
#include <cstring>

LatencyTracker::LatencyTracker() {
    ring_.fill(0);
    for (auto& h : hist_) h.store(0, std::memory_order_relaxed);
}

int LatencyTracker::bucket_index(uint64_t ns) {
    if (ns == 0) return 0;
    // Find MSB position, then use 2 more bits for sub-buckets
    int msb = 63 - __builtin_clzll(ns);  // floor(log2(ns))
    int sub = (msb >= 2) ? ((ns >> (msb - 2)) & 3) : static_cast<int>(ns);
    int idx = std::max(0, msb - 1) * 4 + sub;
    return std::min(idx, static_cast<int>(NUM_BUCKETS - 1));
}

void LatencyTracker::record(uint64_t latency_ns) {
    // Write to ring buffer (wraps around)
    size_t pos = write_pos_.fetch_add(1, std::memory_order_relaxed) & (RING_SIZE - 1);
    ring_[pos] = latency_ns;

    // Update histogram
    hist_[bucket_index(latency_ns)].fetch_add(1, std::memory_order_relaxed);

    // Update aggregates
    total_samples_.fetch_add(1, std::memory_order_relaxed);
    sum_ns_.fetch_add(latency_ns, std::memory_order_relaxed);

    uint64_t cur_min = min_ns_.load(std::memory_order_relaxed);
    while (latency_ns < cur_min &&
           !min_ns_.compare_exchange_weak(cur_min, latency_ns,
                                          std::memory_order_relaxed)) {}

    uint64_t cur_max = max_ns_.load(std::memory_order_relaxed);
    while (latency_ns > cur_max &&
           !max_ns_.compare_exchange_weak(cur_max, latency_ns,
                                          std::memory_order_relaxed)) {}
}

LatencyTracker::LatencyStats LatencyTracker::get_stats() const {
    LatencyStats s{};
    s.sample_count = total_samples_.load(std::memory_order_relaxed);
    if (s.sample_count == 0) return s;

    s.min_ns  = min_ns_.load(std::memory_order_relaxed);
    s.max_ns  = max_ns_.load(std::memory_order_relaxed);
    s.mean_ns = sum_ns_.load(std::memory_order_relaxed) / s.sample_count;

    // Build cumulative distribution from histogram
    uint64_t cum = 0;
    bool got_p50 = false, got_p95 = false, got_p99 = false, got_p999 = false;

    auto target = [&](double pct) {
        return static_cast<uint64_t>(s.sample_count * pct);
    };

    for (int i = 0; i < static_cast<int>(NUM_BUCKETS); ++i) {
        cum += hist_[i].load(std::memory_order_relaxed);

        // Approximate bucket midpoint (rough but fine for display)
        uint64_t bucket_val = (i < 4) ? static_cast<uint64_t>(i) :
            (static_cast<uint64_t>(1) << ((i / 4) + 1)) + (i % 4) * (static_cast<uint64_t>(1) << (i / 4));

        if (!got_p50  && cum >= target(0.500)) { s.p50  = bucket_val; got_p50  = true; }
        if (!got_p95  && cum >= target(0.950)) { s.p95  = bucket_val; got_p95  = true; }
        if (!got_p99  && cum >= target(0.990)) { s.p99  = bucket_val; got_p99  = true; }
        if (!got_p999 && cum >= target(0.999)) { s.p999 = bucket_val; got_p999 = true; }
    }
    return s;
}

void LatencyTracker::export_csv(const std::string& path) const {
    std::ofstream f(path);
    f << "bucket_index,approx_ns,count\n";
    for (int i = 0; i < static_cast<int>(NUM_BUCKETS); ++i) {
        uint64_t cnt = hist_[i].load(std::memory_order_relaxed);
        if (cnt == 0) continue;
        uint64_t ns = (i < 4) ? static_cast<uint64_t>(i) :
            (static_cast<uint64_t>(1) << ((i / 4) + 1)) + (i % 4) * (static_cast<uint64_t>(1) << (i / 4));
        f << i << "," << ns << "," << cnt << "\n";
    }
}

void LatencyTracker::reset() {
    for (auto& h : hist_) h.store(0, std::memory_order_relaxed);
    total_samples_.store(0, std::memory_order_relaxed);
    sum_ns_.store(0, std::memory_order_relaxed);
    min_ns_.store(UINT64_MAX, std::memory_order_relaxed);
    max_ns_.store(0, std::memory_order_relaxed);
    write_pos_.store(0, std::memory_order_relaxed);
}