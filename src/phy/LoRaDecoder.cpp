#include <lora_phy/LoRaCodes.hpp>
#include <lora_phy/phy.hpp>

namespace lora_phy {

size_t lora_decode(const uint16_t* symbols, size_t symbol_count,
                   uint8_t* out_bytes)
{
    size_t byte_idx = 0;
    for (size_t i = 0; i + 1 < symbol_count; i += 2)
    {
        bool err = false, bad = false;
        uint8_t hi = decodeHamming84sx(static_cast<uint8_t>(symbols[i]), err, bad) & 0x0f;
        err = false; bad = false;
        uint8_t lo = decodeHamming84sx(static_cast<uint8_t>(symbols[i + 1]), err, bad) & 0x0f;
        out_bytes[byte_idx++] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return byte_idx;
}

} // namespace lora_phy

