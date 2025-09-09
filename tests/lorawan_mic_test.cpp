#include <lorawan/lorawan.hpp>
#include <algorithm>
#include <cstdint>
#include <iostream>

int lorawan_mic_test_main() {
    uint8_t nwk_skey[16];
    std::fill(std::begin(nwk_skey), std::end(nwk_skey), 2);
    uint8_t msg[] = {0x40,0x04,0x03,0x02,0x01,0x80,0x01,0x00,0x01,0xA6,0x94,0x64,0x26,0x15};
    uint32_t mic = lorawan::compute_mic(nwk_skey, true, 0x01020304, 1, msg, sizeof(msg));
    if (mic != 0x82B5C3D6u) {
        std::cerr << "mic mismatch\n";
        return 1;
    }
    return 0;
}
