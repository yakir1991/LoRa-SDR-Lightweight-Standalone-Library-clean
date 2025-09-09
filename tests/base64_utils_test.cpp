#include "base64_utils.hpp"
#include <cstdint>
#include <vector>
#include <string>

int main() {
    const std::string b64 = "3q2+7w=="; // 0xDE 0xAD 0xBE 0xEF
    std::vector<uint8_t> expected = {0xDE, 0xAD, 0xBE, 0xEF};
    auto decoded = decode_base64(b64);
    return decoded == expected ? 0 : 1;
}
