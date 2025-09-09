#include <lora_phy/phy.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include "base64_utils.hpp"

int main() {
    // Payload and expected symbol vector encoded as base64 so no binary blobs
    const std::string payload_b64 = "3q2+7w=="; // 0xDE 0xAD 0xBE 0xEF
    const std::string symbols_b64 = "jQAuAJoAjQBLAC4ALgD/AA==";

    auto payload = decode_base64(payload_b64);
    auto symbol_bytes = decode_base64(symbols_b64);

    // Convert little-endian byte stream into 16-bit symbol values
    std::vector<uint16_t> expected(symbol_bytes.size() / 2);
    for (size_t i = 0; i < expected.size(); ++i) {
        expected[i] = static_cast<uint16_t>(symbol_bytes[2 * i]) |
                      (static_cast<uint16_t>(symbol_bytes[2 * i + 1]) << 8);
    }

    std::vector<uint16_t> symbols(expected.size());
    lora_phy::lora_encode(payload.data(), payload.size(), symbols.data(), 7);
    bool ok = symbols == expected;

    std::vector<uint8_t> decoded(payload.size());
    lora_phy::lora_decode(expected.data(), expected.size(), decoded.data());
    ok = ok && (decoded == payload);

    return ok ? 0 : 1;
}
