#include <lora_phy/LoRaCodes.hpp>
#include <cstdint>
#include <string>
#include <vector>

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

int whitening_test_main() {
    // Payload+CRC (little endian) and whitening reference encoded in base64
    const std::string plain_b64 = "3q2+73AN"; // DE AD BE EF 70 0D
    const std::string whiten_b64 = "IVKQECzy"; // 21 52 90 10 2C F2

    auto plain = decode_base64(plain_b64);
    auto expected_whiten = decode_base64(whiten_b64);

    // Whitening
    std::vector<uint8_t> tmp = plain;
    Sx1272ComputeWhiteningLfsr(tmp.data(), tmp.size(), 0, 4);
    bool ok = (tmp == expected_whiten);

    // De-whitening
    Sx1272ComputeWhiteningLfsr(tmp.data(), tmp.size(), 0, 4);
    ok = ok && (tmp == plain);

    // CRC check on de-whitened data
    uint16_t crc_calc = sx1272DataChecksum(tmp.data(), tmp.size() - 2);
    uint16_t crc_buf = static_cast<uint16_t>(tmp[tmp.size() - 2]) |
                       (static_cast<uint16_t>(tmp[tmp.size() - 1]) << 8);
    ok = ok && (crc_calc == crc_buf);

    return ok ? 0 : 1;
}
