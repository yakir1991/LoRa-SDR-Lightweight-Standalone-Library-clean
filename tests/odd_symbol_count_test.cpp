#include <lora_phy/phy.hpp>
#include <cerrno>
#include <iostream>

int main() {
    uint16_t symbols[3] = {0, 0, 0};
    uint8_t out[2];
    ssize_t s = lora_phy::lora_decode(symbols, 3, out);
    if (s != -EINVAL) {
        std::cerr << "expected -EINVAL, got " << s << std::endl;
        return 1;
    }
    return 0;
}
