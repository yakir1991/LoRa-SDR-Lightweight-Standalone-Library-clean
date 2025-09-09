#include <lora_phy/LoRaDetector.hpp>
#include <lora_phy/phy.hpp>

#include <algorithm>
#include <cmath>
#include <new>

namespace lora_phy {

void lora_demod_init(lora_demod_workspace* ws, unsigned sf,
                     window_type win,
                     std::complex<float>* scratch,
                     size_t max_samples)
{
    ws->N = size_t(1) << sf;
    ws->window_kind = win;
    if (win == window_type::window_hann) {
        for (size_t i = 0; i < ws->N; ++i) {
            ws->window[i] =
                0.5f - 0.5f * std::cos(2.0f * float(M_PI) * static_cast<float>(i) /
                                        (static_cast<float>(ws->N) - 1.0f));
        }
    } else {
        for (size_t i = 0; i < ws->N; ++i) ws->window[i] = 1.0f;
    }
    kissfft<float>::init(ws->fft_plan, ws->N, false);
    ws->fft = new (ws->fft_buf) kissfft<float>(ws->fft_plan);
    ws->detector =
        new (ws->detector_buf) LoRaDetector<float>(ws->N, ws->fft_in, ws->fft_out, *ws->fft);
    ws->scratch = scratch;
    ws->scratch_len = max_samples;
}

void lora_demod_free(lora_demod_workspace* ws)
{
    if (ws->detector) {
        ws->detector->~LoRaDetector<float>();
        ws->detector = nullptr;
    }
    if (ws->fft) {
        ws->fft->~kissfft<float>();
        ws->fft = nullptr;
    }
    ws->N = 0;
    ws->scratch = nullptr;
    ws->scratch_len = 0;
}

size_t lora_demodulate(lora_demod_workspace* ws,
                       const std::complex<float>* samples, size_t sample_count,
                       uint16_t* out_symbols, unsigned osr,
                       uint8_t* out_sync)
{
    const size_t N = ws->N;                    // base samples per symbol
    const size_t step = N * osr;                // oversampled samples per symbol
    const size_t total_symbols = sample_count / step;
    const bool have_sync = total_symbols >= 2;

    // Ensure incoming samples fit within the canonical [-1.0, 1.0] range.
    const std::complex<float>* norm_samples = samples;
    float max_amp = 0.0f;
    for (size_t i = 0; i < sample_count; ++i) {
        float r = std::abs(samples[i].real());
        float im = std::abs(samples[i].imag());
        float m = std::max(r, im);
        if (m > max_amp) max_amp = m;
    }
    if (max_amp > 1.0f) {
        if (!ws->scratch || ws->scratch_len < sample_count) {
            return 0;
        }
        float scale = 1.0f / max_amp;
        for (size_t i = 0; i < sample_count; ++i) {
            ws->scratch[i] = samples[i] * scale;
        }
        norm_samples = ws->scratch;
    }

    const size_t est_syms = std::min(total_symbols, size_t(2));
    float sum_index = 0.0f;
    float phase_diff = 0.0f;
    float prev_phase = 0.0f;
    bool have_prev = false;
    unsigned sum_t = 0;
    for (size_t s = 0; s < est_syms; ++s) {
        const std::complex<float>* sym_base = norm_samples + s * step;
        float best_p = -1e30f;
        size_t best_idx = 0;
        float best_fi = 0.0f;
        unsigned best_t = 0;
        std::complex<float> best_bin;
        for (unsigned t = 0; t < osr; ++t) {
            for (size_t i = 0; i < N; ++i) {
                std::complex<float> samp = sym_base[t + i * osr];
                if (ws->window_kind != window_type::window_none)
                    samp *= ws->window[i];
                ws->detector->feed(i, samp);
            }
            float p, pav, findex;
            size_t idx = ws->detector->detect(p, pav, findex);
            if (p > best_p || (p == best_p && idx < best_idx)) {
                // Select the lowest index on equal power to guarantee
                // deterministic behaviour when multiple bins share the
                // same magnitude.
                best_p = p;
                best_idx = idx;
                best_fi = findex;
                best_t = t;
                best_bin = ws->fft_out[idx];
            }
        }
        sum_t += best_t;
        sum_index += static_cast<float>(best_idx) + best_fi;
        float phase = std::arg(best_bin);
        if (have_prev) {
            float d = phase - prev_phase;
            while (d > float(M_PI)) d -= 2.0f * float(M_PI);
            while (d < -float(M_PI)) d += 2.0f * float(M_PI);
            phase_diff += d;
        }
        prev_phase = phase;
        have_prev = true;
    }

    float avg_index = sum_index / static_cast<float>(est_syms);
    float cfo_coarse = avg_index / static_cast<float>(N);
    float cfo_fine = 0.0f;
    if (est_syms > 1)
        cfo_fine = (phase_diff / static_cast<float>(est_syms - 1)) /
                   (2.0f * float(M_PI) * static_cast<float>(N));
    ws->metrics.cfo = cfo_coarse + cfo_fine;
    float frac = avg_index - std::floor(avg_index + 0.5f);
    float avg_t = static_cast<float>(sum_t) / static_cast<float>(est_syms);
    ws->metrics.time_offset = avg_t -
                              frac * static_cast<float>(N) * static_cast<float>(osr);

    int t_off = static_cast<int>(std::round(ws->metrics.time_offset));
    float rate = -2.0f * float(M_PI) * ws->metrics.cfo / static_cast<float>(N);
    uint16_t sw0 = 0, sw1 = 0;
    size_t out_idx = 0;
    for (size_t s = 0; s < total_symbols; ++s) {
        size_t base = s * step;
        if (t_off > 0) {
            if (base + size_t(t_off) + step <= sample_count)
                base += size_t(t_off);
        } else if (t_off < 0) {
            size_t off = size_t(-t_off);
            if (off <= base) base -= off;
        }
        const std::complex<float>* sym_samps = norm_samples + base;
        float start = rate * (static_cast<float>(s * N) +
                              static_cast<float>(t_off) / static_cast<float>(osr));
        for (size_t i = 0; i < N; ++i) {
            float ph = start + rate * static_cast<float>(i);
            float cs = std::cos(ph);
            float sn = std::sin(ph);
            std::complex<float> samp = sym_samps[i * osr] *
                                       std::complex<float>(cs, sn);
            if (ws->window_kind != window_type::window_none)
                samp *= ws->window[i];
            ws->detector->feed(i, samp);
        }
        float p, pav, findex;
        size_t idx = ws->detector->detect(p, pav, findex);
        if (have_sync) {
            if (s == 0)
                sw0 = static_cast<uint16_t>(idx);
            else if (s == 1)
                sw1 = static_cast<uint16_t>(idx);
            else
                out_symbols[out_idx++] = static_cast<uint16_t>(idx);
        } else {
            out_symbols[out_idx++] = static_cast<uint16_t>(idx);
        }
    }

    if (out_sync) {
        if (have_sync) {
            unsigned sf_bits = 0;
            size_t tmp = N;
            while (tmp > 1) {
                tmp >>= 1;
                ++sf_bits;
            }
            unsigned shift = sf_bits > 4 ? (sf_bits - 4) : 0;
            uint8_t hi = static_cast<uint8_t>(sw0 >> shift) & 0x0f;
            uint8_t lo = static_cast<uint8_t>(sw1 >> shift) & 0x0f;
            *out_sync = static_cast<uint8_t>((hi << 4) | lo);
        } else {
            *out_sync = 0;
        }
    }

    return have_sync ? out_idx : total_symbols;
}

} // namespace lora_phy

