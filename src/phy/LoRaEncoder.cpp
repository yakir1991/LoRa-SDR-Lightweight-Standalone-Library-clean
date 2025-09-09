#include <lora_phy/LoRaCodes.hpp>
#include <lora_phy/phy.hpp>

namespace lora_phy {

size_t lora_encode(const uint8_t* bytes, size_t byte_count,
                   uint16_t* out_symbols, unsigned /*sf*/)
{
    size_t sym_idx = 0;
    for (size_t i = 0; i < byte_count; ++i)
    {
        uint8_t hi = bytes[i] >> 4;
        uint8_t lo = bytes[i] & 0x0f;
        out_symbols[sym_idx++] = encodeHamming84sx(hi);
        out_symbols[sym_idx++] = encodeHamming84sx(lo);
    }
    return sym_idx;
}

} // namespace lora_phy

