#!/bin/bash
set -e
echo "=== Running Latency Tracker Benchmark ==="
./build/bench_latency

echo ""
echo "=== Running Parser Benchmark ==="
./build/bench_parser

echo ""
echo "Done. Check latency_histogram.csv for histogram data."