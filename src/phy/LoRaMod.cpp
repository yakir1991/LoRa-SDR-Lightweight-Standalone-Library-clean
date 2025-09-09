#include <algorithm>
#include <cmath>
#include <lora_phy/ChirpGenerator.hpp>
#include <lora_phy/phy.hpp>

namespace lora_phy {

size_t lora_modulate(const uint16_t* symbols, size_t symbol_count,
                     std::complex<float>* out_samples, unsigned sf, unsigned osr,
                     bandwidth bw, float amplitude, uint8_t sync)
{
    const size_t N = size_t(1) << sf; // base samples per symbol
    const size_t step = N * osr;
    float phase = 0.0f;
    const float bw_scale = lora_phy::bw_scale(bw);

    // Clamp user requested amplitude to the canonical IQ range of [-1.0, 1.0].
    amplitude = std::max(-1.0f, std::min(1.0f, amplitude));

    unsigned shift = sf > 4 ? (sf - 4) : 0;
    const uint16_t sw0 = static_cast<uint16_t>((sync >> 4) << shift);
    const uint16_t sw1 = static_cast<uint16_t>((sync & 0x0f) << shift);

    const float f0 = (2.0f * float(M_PI) * sw0 * bw_scale) /
                     (float(N) * static_cast<float>(osr));
    genChirp(out_samples, static_cast<int>(N), static_cast<int>(osr),
             static_cast<int>(step), f0, false, amplitude, phase, bw_scale);

    const float f1 = (2.0f * float(M_PI) * sw1 * bw_scale) /
                     (float(N) * static_cast<float>(osr));
    genChirp(out_samples + step, static_cast<int>(N), static_cast<int>(osr),
             static_cast<int>(step), f1, false, amplitude, phase, bw_scale);

    for (size_t s = 0; s < symbol_count; ++s)
    {
        const float freq = (2.0f * float(M_PI) * symbols[s] * bw_scale) /
                           (float(N) * static_cast<float>(osr));
        genChirp(out_samples + (s + 2) * step, static_cast<int>(N),
                 static_cast<int>(osr), static_cast<int>(step), freq, false,
                 amplitude, phase, bw_scale);
    }
    return (symbol_count + 2) * step;
}

} // namespace lora_phy

