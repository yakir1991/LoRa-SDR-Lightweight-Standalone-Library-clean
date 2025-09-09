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
    // Four complex samples: (1,0), (0,0), (1,0), (0,0)
    const std::string iq_b64 = "AACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAA=";
    auto bytes = decode_base64(iq_b64);

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
