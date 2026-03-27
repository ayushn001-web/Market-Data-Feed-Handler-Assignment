#include "memory_pool.h"
#include <stdexcept>

MemoryPool::MemoryPool() : storage_(BUF_SIZE * POOL_CAP) {
    free_list_.reserve(POOL_CAP);
    for (size_t i = 0; i < POOL_CAP; ++i)
        free_list_.push_back(storage_.data() + i * BUF_SIZE);
}

uint8_t* MemoryPool::acquire() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (free_list_.empty()) return nullptr;
    uint8_t* buf = free_list_.back();
    free_list_.pop_back();
    return buf;
}

void MemoryPool::release(uint8_t* buf) {
    if (!buf) return;
    std::lock_guard<std::mutex> lk(mtx_);
    free_list_.push_back(buf);
}