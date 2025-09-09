#include <lorawan/lorawan.hpp>

#include <algorithm>
#include <cerrno>
#include <cstring>

extern "C" {
#include <lorawan/aes.h>
}

namespace lorawan {

namespace {

static void left_shift_one(const uint8_t* in, uint8_t* out) {
    uint8_t carry = 0;
    for (int i = 15; i >= 0; --i) {
        uint8_t b = in[i];
        out[i] = static_cast<uint8_t>((b << 1) | carry);
        carry = (b & 0x80) ? 1 : 0;
    }
}

static void generate_subkeys(const AES_ctx* ctx, uint8_t* K1, uint8_t* K2) {
    uint8_t L[16]{};
    AES_ECB_encrypt(ctx, L);
    left_shift_one(L, K1);
    if (L[0] & 0x80) K1[15] ^= 0x87;
    left_shift_one(K1, K2);
    if (K1[0] & 0x80) K2[15] ^= 0x87;
}

} // namespace

uint32_t compute_mic(const uint8_t nwk_skey[16],
                     bool uplink,
                     uint32_t devaddr,
                     uint32_t fcnt,
                     const uint8_t* data,
                     size_t len) {
    AES_ctx ctx;
    AES_init_ctx(&ctx, nwk_skey);
    uint8_t K1[16], K2[16];
    generate_subkeys(&ctx, K1, K2);

    uint8_t b0[16]{};
    b0[0] = 0x49;
    b0[5] = uplink ? 0 : 1;
    b0[6] = static_cast<uint8_t>(devaddr & 0xFF);
    b0[7] = static_cast<uint8_t>((devaddr >> 8) & 0xFF);
    b0[8] = static_cast<uint8_t>((devaddr >> 16) & 0xFF);
    b0[9] = static_cast<uint8_t>((devaddr >> 24) & 0xFF);
    b0[10] = static_cast<uint8_t>(fcnt & 0xFF);
    b0[11] = static_cast<uint8_t>((fcnt >> 8) & 0xFF);
    b0[12] = static_cast<uint8_t>((fcnt >> 16) & 0xFF);
    b0[13] = static_cast<uint8_t>((fcnt >> 24) & 0xFF);
    b0[14] = static_cast<uint8_t>((len >> 8) & 0xFF);
    b0[15] = static_cast<uint8_t>(len & 0xFF);

    size_t total = len + 16;
    size_t n = (total + 15) / 16;
    bool flag = (total % 16) == 0;

    uint8_t X[16]{};
    uint8_t block[16];

    // process all but last block
    for (size_t i = 0; i < n - 1; ++i) {
        for (int j = 0; j < 16; ++j) {
            size_t pos = i * 16 + j;
            uint8_t byte = pos < 16 ? b0[pos] : data[pos - 16];
            block[j] = byte ^ X[j];
        }
        AES_ECB_encrypt(&ctx, block);
        std::memcpy(X, block, 16);
    }

    // last block
    size_t last_offset = (n - 1) * 16;
    size_t bytes_in_last = total - last_offset;
    uint8_t last[16]{};
    for (size_t j = 0; j < bytes_in_last && j < 16; ++j) {
        size_t pos = last_offset + j;
        last[j] = pos < 16 ? b0[pos] : data[pos - 16];
    }
    if (flag) {
        for (int j = 0; j < 16; ++j) last[j] ^= K1[j];
    } else {
        last[bytes_in_last] = 0x80;
        for (int j = 0; j < 16; ++j) last[j] ^= K2[j];
    }
    for (int j = 0; j < 16; ++j) last[j] ^= X[j];
    AES_ECB_encrypt(&ctx, last);
    return static_cast<uint32_t>(last[0]) |
           (static_cast<uint32_t>(last[1]) << 8) |
           (static_cast<uint32_t>(last[2]) << 16) |
           (static_cast<uint32_t>(last[3]) << 24);
}

ssize_t build_frame(lora_phy::lora_workspace* ws,
                    const uint8_t nwk_skey[16],
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
    bool uplink = (static_cast<uint8_t>(frame.mhdr.mtype) & 1) == 0;
    uint32_t mic = compute_mic(nwk_skey, uplink, frame.fhdr.devaddr,
                               frame.fhdr.fcnt, tmp_bytes, idx);
    tmp_bytes[idx++] = static_cast<uint8_t>(mic & 0xFF);
    tmp_bytes[idx++] = static_cast<uint8_t>((mic >> 8) & 0xFF);
    tmp_bytes[idx++] = static_cast<uint8_t>((mic >> 16) & 0xFF);
    tmp_bytes[idx++] = static_cast<uint8_t>((mic >> 24) & 0xFF);
    return lora_phy::encode(ws, tmp_bytes, idx, symbols, symbol_cap);
}

ssize_t parse_frame(lora_phy::lora_workspace* ws,
                    const uint8_t nwk_skey[16],
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
    uint8_t mhdr = tmp_bytes[0];
    uint32_t devaddr = tmp_bytes[1] | (tmp_bytes[2] << 8) |
                       (tmp_bytes[3] << 16) | (tmp_bytes[4] << 24);
    uint16_t fcnt = tmp_bytes[6] | (tmp_bytes[7] << 8);
    bool uplink = ((mhdr >> 5) & 1) == 0;
    uint32_t mic = tmp_bytes[len - 4] | (tmp_bytes[len - 3] << 8) |
                   (tmp_bytes[len - 2] << 16) | (tmp_bytes[len - 1] << 24);
    uint32_t calc = compute_mic(nwk_skey, uplink, devaddr, fcnt,
                                tmp_bytes, len - 4);
    if (mic != calc) return -EINVAL;
    size_t idx = 0;
    out.mhdr.mtype = static_cast<MType>(mhdr >> 5);
    out.mhdr.major = mhdr & 0x3;
    idx = 1;
    out.fhdr.devaddr = devaddr;
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

