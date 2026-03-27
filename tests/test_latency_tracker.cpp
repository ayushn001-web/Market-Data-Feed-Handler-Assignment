#include <gtest/gtest.h>
#include "../src/common/latency_tracker.h"

TEST(LatencyTest, BasicRecord) {
    LatencyTracker lt;
    for (uint64_t i = 1; i <= 1000; ++i)
        lt.record(i * 100);  // 100ns to 100000ns

    auto s = lt.get_stats();
    EXPECT_EQ(s.sample_count, 1000ULL);
    EXPECT_GE(s.p50, 0ULL);
    EXPECT_LE(s.p50, s.p99);
    EXPECT_LE(s.p99, s.p999);
}

TEST(LatencyTest, Reset) {
    LatencyTracker lt;
    lt.record(1000);
    lt.reset();
    auto s = lt.get_stats();
    EXPECT_EQ(s.sample_count, 0ULL);
}

TEST(LatencyTest, MinMax) {
    LatencyTracker lt;
    lt.record(500);
    lt.record(200);
    lt.record(9000);
    auto s = lt.get_stats();
    EXPECT_EQ(s.min_ns, 200ULL);
    EXPECT_EQ(s.max_ns, 9000ULL);
}