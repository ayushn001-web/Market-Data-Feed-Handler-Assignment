#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>

// Simple pre-allocated buffer pool to avoid heap allocation in hot path.
// TODO: make lock-free with atomic stack for production use
class MemoryPool {
public:
    static constexpr size_t BUF_SIZE  = 8192;
    static constexpr size_t POOL_CAP  = 512;

    MemoryPool();
    ~MemoryPool() = default;

    uint8_t* acquire();          // returns nullptr if pool exhausted
    void release(uint8_t* buf);

private:
    std::vector<uint8_t>   storage_;
    std::vector<uint8_t*>  free_list_;
    std::mutex             mtx_;
};