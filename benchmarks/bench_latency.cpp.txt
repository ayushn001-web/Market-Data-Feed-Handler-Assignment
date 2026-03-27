#include "../src/common/latency_tracker.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

int main() {
    LatencyTracker tracker;
    constexpr int N = 1'000'000;

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i)
        tracker.record(static_cast<uint64_t>(1000 + i % 50000));
    auto t1 = std::chrono::steady_clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double per_record_ns = (elapsed_ms * 1e6) / N;

    auto s = tracker.get_stats();
    printf("=== LatencyTracker Benchmark ===\n");
    printf("Samples: %d\n", N);
    printf("Total time: %.2f ms\n", elapsed_ms);
    printf("Per-record overhead: %.1f ns\n", per_record_ns);
    printf("p50=%llu ns  p99=%llu ns  p999=%llu ns\n",
           (unsigned long long)s.p50,
           (unsigned long long)s.p99,
           (unsigned long long)s.p999);

    tracker.export_csv("latency_histogram.csv");
    printf("Histogram exported to latency_histogram.csv\n");
    return 0;
}