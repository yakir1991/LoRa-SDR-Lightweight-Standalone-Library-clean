#include <lora_phy/phy.hpp>
#include <cstdint>
#include <complex>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstring>
#include "base64_utils.hpp"

int main() {
    std::ifstream f("vectors/golden/equal_power_iq.b64");
    if (!f) return 1;
    std::string b64((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto bytes = decode_base64(b64);

    const size_t sample_count = bytes.size() / (sizeof(float) * 2);
    std::vector<std::complex<float>> samples(sample_count);
    for (size_t i = 0; i < sample_count; ++i) {
        float re, im;
        std::memcpy(&re, &bytes[i * 8], sizeof(float));
        std::memcpy(&im, &bytes[i * 8 + 4], sizeof(float));
        samples[i] = std::complex<float>(re, im);
    }

    lora_phy::lora_demod_workspace ws{};
    std::vector<std::complex<float>> scratch(sample_count);
    lora_phy::lora_demod_init(&ws, 2, lora_phy::window_type::window_none,
                              scratch.data(), scratch.size());

    std::vector<uint16_t> symbols(1);
    lora_phy::lora_demodulate(&ws, samples.data(), sample_count, symbols.data(), 1);
    lora_phy::lora_demod_free(&ws);

    // Expect lowest index chosen on equal-power bins
    return symbols[0] == 0 ? 0 : 1;
}
