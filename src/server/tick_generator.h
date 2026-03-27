#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <random>
#include <cmath>

struct SymbolInfo {
    uint16_t id;
    double   price;
    double   mu;        // drift
    double   sigma;     // volatility
    double   dt;        // time step (0.001 = 1ms)
    uint32_t base_vol;
    char     name[16];
};

class TickGenerator {
public:
    explicit TickGenerator(size_t num_symbols = 100);

    // Fill `buf` with an encoded proto message for symbol `id`.
    // Returns number of bytes written, or 0 on error.
    size_t generate(uint16_t symbol_id, uint8_t* buf, uint32_t seq_num);

    const SymbolInfo& symbol(uint16_t id) const { return syms_[id]; }
    size_t num_symbols() const { return syms_.size(); }

private:
    double box_muller();  // standard normal via Box-Muller transform

    std::vector<SymbolInfo>          syms_;
    std::mt19937_64                  rng_;
    std::uniform_real_distribution<> spread_dist_{0.0005, 0.002}; // 0.05%-0.20%
};