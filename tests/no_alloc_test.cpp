#include "alloc_tracker.h"
#include <lora_phy/phy.hpp>
#include <lora_phy/ChirpGenerator.hpp>
#include <complex>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// Simple base64 decoder used by existing tests.
static std::vector<uint8_t> decode_base64(const std::string& in) {
    std::vector<uint8_t> out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        int d;
        if (c >= 'A' && c <= 'Z') d = c - 'A';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
        else if (c >= '0' && c <= '9') d = c - '0' + 52;
        else if (c == '+') d = 62;
        else if (c == '/') d = 63;
        else if (c == '=') break;
        else continue;
        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

int main() {
    // Symbol vector encoded in base64 (little endian 16-bit values)
    const std::string symbols_b64 = "AAABAAwAIgA4AA=="; // [0,1,12,34,56]
    auto symbol_bytes = decode_base64(symbols_b64);
    const size_t symbol_count = symbol_bytes.size() / 2;

    std::vector<uint16_t> symbols(symbol_count);
    for (size_t i = 0; i < symbol_count; ++i) {
        symbols[i] = static_cast<uint16_t>(symbol_bytes[2 * i]) |
                      (static_cast<uint16_t>(symbol_bytes[2 * i + 1]) << 8);
    }

    const unsigned sf = 7;
    const size_t samples_per_symbol = size_t(1) << sf;
    const size_t sample_count = (symbol_count + 2) * samples_per_symbol;

    std::vector<std::complex<float>> samples(sample_count);
    std::vector<std::complex<float>> scratch(sample_count);

    {
        alloc_tracker::Guard guard;
        lora_phy::lora_modulate(symbols.data(), symbol_count, samples.data(), sf, 1,
                                lora_phy::bandwidth::bw_125, 1.0f, 0x12);
        if (guard.count() != 0) {
            std::cerr << "Allocation occurred in modulate" << std::endl;
            return 1;
        }
    }

    // Precompute downchirp and dechirp the signal before demodulation
    std::vector<std::complex<float>> dechirped(sample_count);
    std::vector<std::complex<float>> down(samples_per_symbol);
    float phase = 0.0f;
    float scale = lora_phy::bw_scale(lora_phy::bandwidth::bw_125);
    genChirp(down.data(), static_cast<int>(samples_per_symbol), 1,
             static_cast<int>(samples_per_symbol), 0.0f, true, 1.0f, phase, scale);
    for (size_t s = 0; s < symbol_count + 2; ++s) {
        for (size_t i = 0; i < samples_per_symbol; ++i) {
            dechirped[s * samples_per_symbol + i] =
                samples[s * samples_per_symbol + i] * down[i];
        }
    }

    lora_phy::lora_demod_workspace ws{};

    {
        alloc_tracker::Guard guard;
        lora_phy::lora_demod_init(&ws, sf, lora_phy::window_type::window_none,
                                  scratch.data(), scratch.size());
        if (guard.count() != 0) {
            std::cerr << "Allocation occurred in demod init" << std::endl;
            return 1;
        }
    }

    std::vector<uint16_t> demod(symbol_count);

    {
        alloc_tracker::Guard guard;
        lora_phy::lora_demodulate(&ws, dechirped.data(), sample_count, demod.data(), 1,
                                   nullptr);
        if (guard.count() != 0) {
            std::cerr << "Allocation occurred in demodulate" << std::endl;
            lora_phy::lora_demod_free(&ws);
            return 1;
        }
    }

    lora_phy::lora_demod_free(&ws);

    if (demod != symbols) {
        std::cerr << "Round-trip mismatch" << std::endl;
        return 1;
    }

    std::cout << "No allocations detected" << std::endl;
    return 0;
}

