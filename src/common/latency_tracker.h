#pragma once
#include <cstdint>
#include <atomic>
#include <array>
#include <cmath>
#include <string>

// Lock-free ring-buffer latency tracker with approximate histogram percentiles.
// Recording overhead is designed to stay under ~30ns on modern hardware.
class LatencyTracker {
public:
    static constexpr size_t RING_SIZE    = 1 << 20;  // 1M samples
    static constexpr size_t NUM_BUCKETS  = 256;       // log2-ish buckets

    struct LatencyStats {
        uint64_t min_ns, max_ns, mean_ns;
        uint64_t p50, p95, p99, p999;
        uint64_t sample_count;
    };

    LatencyTracker();

    // Thread-safe: record a latency sample in nanoseconds
    void record(uint64_t latency_ns);

    // Compute stats from histogram (approximate)
    LatencyStats get_stats() const;

    // Dump histogram buckets to CSV file
    void export_csv(const std::string& path) const;

    void reset();

private:
    // Histogram bucket index for a given latency (log2 scale, 4 sub-buckets per power)
    static int bucket_index(uint64_t ns);

    // Ring buffer for raw samples
    std::array<uint64_t, RING_SIZE> ring_;
    std::atomic<size_t> write_pos_{0};

    // Approximate histogram for fast percentile computation
    // Buckets: 0-3 ns, 4-7 ns, 8-15 ns ... (4 sub-buckets per power of 2)
    mutable std::array<std::atomic<uint64_t>, NUM_BUCKETS> hist_;

    std::atomic<uint64_t> total_samples_{0};
    std::atomic<uint64_t> sum_ns_{0};
    std::atomic<uint64_t> min_ns_{UINT64_MAX};
    std::atomic<uint64_t> max_ns_{0};
};