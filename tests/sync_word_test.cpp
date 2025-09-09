#include <lora_phy/phy.hpp>
#include <cstdint>
#include <complex>
#include <cstring>
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
    const uint8_t sync = 0xAB; // test sync word
    const std::string iq_b64 = "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";

    auto bytes = decode_base64(iq_b64);
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
    bool same = std::memcmp(generated.data(), samples.data(), bytes.size()) == 0;

    // Demodulate and ensure sync word is recovered
    lora_phy::lora_demod_workspace ws{};
    std::vector<std::complex<float>> scratch(sample_count);
    lora_phy::lora_demod_init(&ws, 7, lora_phy::window_type::window_none,
                              scratch.data(), scratch.size());
    uint8_t out_sync = 0;
    std::vector<uint16_t> dummy(1);
    size_t produced = lora_phy::lora_demodulate(&ws, samples.data(),
                                                sample_count, dummy.data(), 1,
                                                &out_sync);
    lora_phy::lora_demod_free(&ws);

    bool ok = same && produced == 0 && out_sync == sync;
    return ok ? 0 : 1;
}

