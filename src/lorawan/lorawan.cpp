#include <lorawan/lorawan.hpp>

#include <algorithm>
#include <cerrno>

namespace lorawan {

namespace {
// Simple CRC32 implementation for MIC generation
static uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}
} // namespace

uint32_t compute_mic(const uint8_t* data, size_t len) {
    return crc32(data, len);
}

ssize_t build_frame(lora_phy::lora_workspace* ws,
                    const Frame& frame,
                    uint16_t* symbols,
                    size_t symbol_cap,
                    uint8_t* tmp_bytes,
                    size_t tmp_cap) {
    if (!ws || !symbols || !tmp_bytes) return -EINVAL;
    size_t needed = 1 + 4 + 1 + 2 + frame.fhdr.fopts.size() + frame.payload.size() + 4;
    if (needed > tmp_cap) return -ERANGE;
    size_t idx = 0;
    uint8_t mhdr = (static_cast<uint8_t>(frame.mhdr.mtype) << 5) |
                   (frame.mhdr.major & 0x3);
    tmp_bytes[idx++] = mhdr;
    uint32_t a = frame.fhdr.devaddr;
    tmp_bytes[idx++] = static_cast<uint8_t>(a & 0xFF);
    tmp_bytes[idx++] = static_cast<uint8_t>((a >> 8) & 0xFF);
    tmp_bytes[idx++] = static_cast<uint8_t>((a >> 16) & 0xFF);
    tmp_bytes[idx++] = static_cast<uint8_t>((a >> 24) & 0xFF);
    uint8_t fctrl = (frame.fhdr.fctrl & 0xF0) |
                    (static_cast<uint8_t>(frame.fhdr.fopts.size()) & 0x0F);
    tmp_bytes[idx++] = fctrl;
    tmp_bytes[idx++] = static_cast<uint8_t>(frame.fhdr.fcnt & 0xFF);
    tmp_bytes[idx++] = static_cast<uint8_t>((frame.fhdr.fcnt >> 8) & 0xFF);
    std::copy(frame.fhdr.fopts.begin(), frame.fhdr.fopts.end(), tmp_bytes + idx);
    idx += frame.fhdr.fopts.size();
    std::copy(frame.payload.begin(), frame.payload.end(), tmp_bytes + idx);
    idx += frame.payload.size();
    uint32_t mic = compute_mic(tmp_bytes, idx);
    tmp_bytes[idx++] = static_cast<uint8_t>(mic & 0xFF);
    tmp_bytes[idx++] = static_cast<uint8_t>((mic >> 8) & 0xFF);
    tmp_bytes[idx++] = static_cast<uint8_t>((mic >> 16) & 0xFF);
    tmp_bytes[idx++] = static_cast<uint8_t>((mic >> 24) & 0xFF);
    return lora_phy::encode(ws, tmp_bytes, idx, symbols, symbol_cap);
}

ssize_t parse_frame(lora_phy::lora_workspace* ws,
                    const uint16_t* symbols,
                    size_t symbol_count,
                    Frame& out,
                    uint8_t* tmp_bytes,
                    size_t tmp_cap) {
    if (!ws || !symbols || !tmp_bytes) return -EINVAL;
    size_t byte_cap = tmp_cap;
    ssize_t produced = lora_phy::decode(ws, symbols, symbol_count,
                                        tmp_bytes, byte_cap);
    if (produced < 0) return produced;
    if (static_cast<size_t>(produced) < 1 + 4 + 1 + 2 + 4) return -ERANGE;
    size_t len = static_cast<size_t>(produced);
    uint32_t mic = tmp_bytes[len - 4] | (tmp_bytes[len - 3] << 8) |
                   (tmp_bytes[len - 2] << 16) | (tmp_bytes[len - 1] << 24);
    uint32_t calc = compute_mic(tmp_bytes, len - 4);
    if (mic != calc) return -EINVAL;
    size_t idx = 0;
    uint8_t mhdr = tmp_bytes[idx++];
    out.mhdr.mtype = static_cast<MType>(mhdr >> 5);
    out.mhdr.major = mhdr & 0x3;
    out.fhdr.devaddr = tmp_bytes[idx] | (tmp_bytes[idx + 1] << 8) |
                       (tmp_bytes[idx + 2] << 16) | (tmp_bytes[idx + 3] << 24);
    idx += 4;
    out.fhdr.fctrl = tmp_bytes[idx++];
    unsigned fopts_len = out.fhdr.fctrl & 0x0F;
    out.fhdr.fcnt = tmp_bytes[idx] | (tmp_bytes[idx + 1] << 8);
    idx += 2;
    if (idx + fopts_len > len - 4) return -ERANGE;
    out.fhdr.fopts.assign(tmp_bytes + idx, tmp_bytes + idx + fopts_len);
    idx += fopts_len;
    out.payload.assign(tmp_bytes + idx, tmp_bytes + (len - 4));
    return static_cast<ssize_t>(out.payload.size());
}

} // namespace lorawan

