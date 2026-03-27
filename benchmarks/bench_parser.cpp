#include "../src/client/parser.h"
#include "../src/server/tick_generator.h"
#include "../include/protocol.h"
#include <chrono>
#include <cstdio>

int main() {
    TickGenerator gen(100);
    constexpr int N = 1'000'000;

    // Pre-generate messages into a flat buffer
    std::vector<uint8_t> stream;
    stream.reserve(N * proto::MAX_MSG_SIZE);
    uint8_t buf[proto::MAX_MSG_SIZE];
    for (int i = 0; i < N; ++i) {
        size_t n = gen.generate(static_cast<uint16_t>(i % 100), buf, i);
        stream.insert(stream.end(), buf, buf + n);
    }

    int parsed = 0;
    BinaryParser parser([&](const ParsedMsg&) { parsed++; });

    auto t0 = std::chrono::steady_clock::now();

    // Feed in 8KB chunks to simulate real socket reads
    constexpr size_t CHUNK = 8192;
    for (size_t off = 0; off < stream.size(); off += CHUNK)
        parser.feed(stream.data() + off,
                    std::min(CHUNK, stream.size() - off));

    auto t1 = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double msgs_per_sec = parsed / (elapsed_ms / 1000.0);

    printf("=== Parser Benchmark ===\n");
    printf("Messages parsed: %d / %d\n", parsed, N);
    printf("Total time: %.2f ms\n", elapsed_ms);
    printf("Throughput: %.0f msg/s\n", msgs_per_sec);
    printf("Sequence gaps: %llu\n", (unsigned long long)parser.sequence_gaps());
    return 0;
}