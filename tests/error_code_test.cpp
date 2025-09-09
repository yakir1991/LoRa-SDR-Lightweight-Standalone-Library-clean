#include <lora_phy/phy.hpp>
#include <lorawan/lorawan.hpp>
#include <cerrno>
#include <complex>
#include <vector>
#include <iostream>

int main() {
    using namespace lora_phy;
    using namespace lorawan;
    bool ok = true;

    lora_params cfg{};
    cfg.sf = 7;
    cfg.bw = bandwidth::bw_125;
    cfg.cr = 1;
    cfg.osr = 1;
    cfg.sync_word = 0x12;

    // init errors
    if (init(nullptr, &cfg) != -EINVAL) {
        std::cerr << "init null ws" << std::endl;
        ok = false;
    }
    lora_workspace tmp_ws{};
    if (init(&tmp_ws, nullptr) != -EINVAL) {
        std::cerr << "init null cfg" << std::endl;
        ok = false;
    }
    cfg.window = window_type::window_hann;
    if (init(&tmp_ws, &cfg) != -ENOMEM) {
        std::cerr << "init missing window" << std::endl;
        ok = false;
    }
    cfg.window = window_type::window_none;

    // prepare valid workspace
    std::vector<std::complex<float>> fft_in(1u << cfg.sf);
    std::vector<std::complex<float>> fft_out(1u << cfg.sf);
    std::vector<float> window(1u << cfg.sf);
    lora_workspace ws{};
    ws.fft_in = fft_in.data();
    ws.fft_out = fft_out.data();
    ws.window = window.data();
    if (init(&ws, &cfg) != 0) {
        std::cerr << "init valid" << std::endl;
        return 1;
    }

    // encode errors
    uint8_t payload[4] = {1,2,3,4};
    uint16_t symbols[8];
    if (encode(nullptr, payload, 4, symbols, 8) != -EINVAL) {
        std::cerr << "encode null ws" << std::endl;
        ok = false;
    }
    if (encode(&ws, payload, 4, symbols, 1) != -ERANGE) {
        std::cerr << "encode small cap" << std::endl;
        ok = false;
    }

    // modulate errors
    std::complex<float> iq[8];
    if (modulate(nullptr, symbols, 1, iq, 8) != -EINVAL) {
        std::cerr << "modulate null ws" << std::endl;
        ok = false;
    }
    if (modulate(&ws, symbols, 1, iq, 1) != -ERANGE) {
        std::cerr << "modulate small cap" << std::endl;
        ok = false;
    }

    // demodulate errors
    std::complex<float> samples[10];
    if (demodulate(nullptr, samples, 10, symbols, 8) != -EINVAL) {
        std::cerr << "demodulate null ws" << std::endl;
        ok = false;
    }
    if (demodulate(&ws, samples, 10, symbols, 8) != -EINVAL) {
        std::cerr << "demodulate misaligned" << std::endl;
        ok = false;
    }
    size_t step = (1u << cfg.sf) * cfg.osr;
    std::vector<std::complex<float>> few(step);
    if (demodulate(&ws, few.data(), step, symbols, 8) != -ERANGE) {
        std::cerr << "demodulate short" << std::endl;
        ok = false;
    }
    std::vector<std::complex<float>> many(step * 4);
    if (demodulate(&ws, many.data(), step * 4, symbols, 1) != -ERANGE) {
        std::cerr << "demodulate overflow" << std::endl;
        ok = false;
    }

    // decode errors
    uint8_t out[4];
    if (decode(nullptr, symbols, 1, out, 4) != -EINVAL) {
        std::cerr << "decode null ws" << std::endl;
        ok = false;
    }
    if (decode(&ws, symbols, 1, out, 0) != -ERANGE) {
        std::cerr << "decode small cap" << std::endl;
        ok = false;
    }

    // lorawan build_frame errors
    Frame frame{};
    frame.mhdr.mtype = MType::UnconfirmedDataUp;
    frame.mhdr.major = 0;
    frame.fhdr.devaddr = 0x01020304;
    frame.fhdr.fcnt = 1;
    frame.payload = {0xAA};
    std::vector<uint16_t> lora_syms(64);
    std::vector<uint8_t> tmp(64);
    uint8_t nwk_skey[16] = {};
    if (build_frame(nullptr, nwk_skey, frame, lora_syms.data(), lora_syms.size(), tmp.data(), tmp.size()) != -EINVAL) {
        std::cerr << "build_frame null" << std::endl;
        ok = false;
    }
    if (build_frame(&ws, nwk_skey, frame, lora_syms.data(), lora_syms.size(), tmp.data(), 1) != -ERANGE) {
        std::cerr << "build_frame small tmp" << std::endl;
        ok = false;
    }

    // lorawan parse_frame errors
    Frame out_frame;
    if (parse_frame(nullptr, nwk_skey, lora_syms.data(), 0, out_frame, tmp.data(), tmp.size()) != -EINVAL) {
        std::cerr << "parse_frame null" << std::endl;
        ok = false;
    }
    if (parse_frame(&ws, nwk_skey, lora_syms.data(), 0, out_frame, tmp.data(), tmp.size()) != -ERANGE) {
        std::cerr << "parse_frame short" << std::endl;
        ok = false;
    }

    // fopts overrun
    std::vector<uint8_t> bad_bytes = {0,0,0,0,0,0x05,0,0};
    uint32_t mic = compute_mic(nwk_skey, true, 0, 0, bad_bytes.data(), bad_bytes.size());
    bad_bytes.push_back(static_cast<uint8_t>(mic & 0xFF));
    bad_bytes.push_back(static_cast<uint8_t>((mic >> 8) & 0xFF));
    bad_bytes.push_back(static_cast<uint8_t>((mic >> 16) & 0xFF));
    bad_bytes.push_back(static_cast<uint8_t>((mic >> 24) & 0xFF));
    std::vector<uint16_t> bad_syms(64);
    ssize_t s = encode(&ws, bad_bytes.data(), bad_bytes.size(), bad_syms.data(), bad_syms.size());
    if (s > 0) {
        if (parse_frame(&ws, nwk_skey, bad_syms.data(), static_cast<size_t>(s), out_frame, tmp.data(), tmp.size()) != -ERANGE) {
            std::cerr << "parse_frame fopts" << std::endl;
            ok = false;
        }
    } else {
        std::cerr << "encode failed for fopts" << std::endl;
        ok = false;
    }

    // MIC mismatch
    ssize_t good = build_frame(&ws, nwk_skey, frame, lora_syms.data(), lora_syms.size(), tmp.data(), tmp.size());
    if (good > 0) {
        lora_syms[0] ^= 1;
        if (parse_frame(&ws, nwk_skey, lora_syms.data(), static_cast<size_t>(good), out_frame, tmp.data(), tmp.size()) != -EINVAL) {
            std::cerr << "parse_frame mic" << std::endl;
            ok = false;
        }
    } else {
        std::cerr << "build_frame failed" << std::endl;
        ok = false;
    }

    return ok ? 0 : 1;
}

