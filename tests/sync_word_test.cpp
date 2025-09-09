#include <lora_phy/phy.hpp>
#include <cstdint>
#include <complex>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
#include "base64_utils.hpp"

int main() {
    const uint8_t sync = 0xAB; // test sync word
    std::ifstream f("vectors/golden/sync_word_iq.b64");
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

    // Verify modulated samples match fixture
    std::vector<std::complex<float>> generated(sample_count);
    lora_phy::lora_modulate(nullptr, 0, generated.data(), 7, 1,
                            lora_phy::bandwidth::bw_125, 1.0f, sync);
    bool same = std::memcmp(generated.data(), samples.data(),
                            sample_count * sizeof(std::complex<float>)) == 0;

    // Demodulate and ensure sync word is recovered
    lora_phy::lora_demod_workspace ws{};
    std::vector<std::complex<float>> scratch(sample_count);
    lora_phy::lora_demod_init(&ws, 7, lora_phy::window_type::window_none,
                              scratch.data(), scratch.size());
    uint8_t out_sync = 0;
    std::vector<uint16_t> dummy(1);
    ssize_t produced = lora_phy::lora_demodulate(&ws, samples.data(),
                                                 sample_count, dummy.data(), 1,
                                                 &out_sync);
    lora_phy::lora_demod_free(&ws);

    bool ok = same && produced == 0 && out_sync == sync;
    return ok ? 0 : 1;
}

