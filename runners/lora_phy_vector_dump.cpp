#include <lora_phy/phy.hpp>
#include <lora_phy/LoRaCodes.hpp>

#include <complex>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace lora_phy;

namespace {

void usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " --out=DIR [--sf=N] [--bytes=N] [--seed=N] [--osr=N]"
              << " [--bw=HZ] [--dump=STAGE,...] [--window=hann]\n";
}

} // namespace

int main(int argc, char** argv) {
    unsigned sf = 7;
    unsigned seed = 1;
    unsigned osr = 1;
    bandwidth bw = bandwidth::bw_125;
    size_t byte_count = 16;
    std::string out_dir;
    std::set<std::string> dumps;
    window_type win = window_type::window_none;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--sf=", 0) == 0) {
            sf = static_cast<unsigned>(std::stoul(arg.substr(5)));
        } else if (arg.rfind("--seed=", 0) == 0) {
            seed = static_cast<unsigned>(std::stoul(arg.substr(7)));
        } else if (arg.rfind("--bytes=", 0) == 0) {
            byte_count = static_cast<size_t>(std::stoul(arg.substr(8)));
        } else if (arg.rfind("--osr=", 0) == 0) {
            osr = static_cast<unsigned>(std::stoul(arg.substr(6)));
        } else if (arg.rfind("--bw=", 0) == 0) {
            unsigned val = static_cast<unsigned>(std::stoul(arg.substr(5)));
            if (val == 125000)
                bw = bandwidth::bw_125;
            else if (val == 250000)
                bw = bandwidth::bw_250;
            else if (val == 500000)
                bw = bandwidth::bw_500;
            else {
                std::cerr << "Unsupported bandwidth: " << val << "\n";
                return 1;
            }
        } else if (arg.rfind("--out=", 0) == 0) {
            out_dir = arg.substr(6);
        } else if (arg.rfind("--dump=", 0) == 0) {
            dumps.insert(arg.substr(7));
        } else if (arg.rfind("--window=", 0) == 0) {
            std::string w = arg.substr(9);
            if (w == "hann")
                win = window_type::window_hann;
            else
                win = window_type::window_none;
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            usage(argv[0]);
            return 1;
        }
    }

    if (out_dir.empty()) {
        usage(argv[0]);
        return 1;
    }

    if (dumps.empty()) {
        dumps = {"payload",      "pre_interleave", "post_interleave",
                 "iq",           "demod",          "deinterleave",
                 "decoded"};
    }

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 255);
    std::vector<uint8_t> payload(byte_count);
    for (size_t i = 0; i < byte_count; ++i)
        payload[i] = static_cast<uint8_t>(dist(rng));

    const size_t nibble_count = byte_count * 2;
    const size_t cw_count = ((nibble_count + sf - 1) / sf) * sf;
    const size_t rdd = 4;
    const size_t blocks = cw_count / sf;
    const size_t symbol_count = blocks * (4 + rdd);
    const size_t N = size_t(1) << sf;

    std::vector<uint8_t> pre_interleave(cw_count, 0);
    for (size_t i = 0; i < nibble_count; ++i) {
        uint8_t b = payload[i / 2];
        uint8_t nib = (i & 1) ? (b & 0x0f) : (b >> 4);
        pre_interleave[i] = encodeHamming84sx(nib);
    }

    std::vector<uint16_t> post_interleave(symbol_count);
    std::vector<uint16_t> demod(symbol_count);
    std::vector<uint8_t> deinterleave(cw_count, 0);
    std::vector<uint8_t> decoded(byte_count);
    std::vector<std::complex<float>> fft_in(N), fft_out(N * osr);
    std::vector<float> window(N);
    std::vector<std::complex<float>> samples((symbol_count + 2) * N * osr);

    lora_workspace ws{};
    ws.symbol_buf = post_interleave.data();
    ws.fft_in = fft_in.data();
    ws.fft_out = fft_out.data();
    ws.window = window.data();
    lora_params params{};
    params.sf = sf;
    params.bw = bw;
    params.cr = 0;
    params.osr = osr;
    params.window = win;
    if (init(&ws, &params) != 0) {
        std::cerr << "Failed to initialise workspace\n";
        return 1;
    }

    ssize_t produced = encode(&ws, payload.data(), payload.size(),
                              post_interleave.data(), post_interleave.size());
    if (produced < 0) {
        std::cerr << "encode() failed\n";
        return 1;
    }

    ssize_t sample_count = modulate(&ws, post_interleave.data(), produced,
                                    samples.data(), samples.size());
    if (sample_count < 0) {
        std::cerr << "modulate() failed\n";
        return 1;
    }

    ssize_t demod_syms = demodulate(&ws, samples.data(), sample_count,
                                    demod.data(), demod.size());
    if (demod_syms < 0) {
        std::cerr << "demodulate() failed\n";
        return 1;
    }

    diagonalDeterleaveSx(demod.data(), symbol_count, deinterleave.data(), sf,
                         rdd);

    for (size_t i = 0; i < byte_count; ++i) {
        bool err = false, bad = false;
        uint8_t hi = decodeHamming84sx(deinterleave[2 * i], err, bad) & 0x0f;
        err = bad = false;
        uint8_t lo = decodeHamming84sx(deinterleave[2 * i + 1], err, bad) & 0x0f;
        decoded[i] = static_cast<uint8_t>((hi << 4) | lo);
    }

    std::ofstream f;

    if (dumps.count("payload")) {
        f.open(out_dir + "/payload.bin", std::ios::binary);
        f.write(reinterpret_cast<const char*>(payload.data()), payload.size());
        f.close();
    }
    if (dumps.count("pre_interleave")) {
        f.open(out_dir + "/pre_interleave.csv");
        for (size_t i = 0; i < cw_count; ++i)
            f << static_cast<unsigned>(pre_interleave[i]) << "\n";
        f.close();
    }
    if (dumps.count("post_interleave")) {
        f.open(out_dir + "/post_interleave.csv");
        for (size_t i = 0; i < symbol_count; ++i) f << post_interleave[i] << "\n";
        f.close();
    }
    if (dumps.count("iq")) {
        f.open(out_dir + "/iq_samples.csv");
        for (ssize_t i = 0; i < sample_count; ++i)
            f << samples[i].real() << "," << samples[i].imag() << "\n";
        f.close();
    }
    if (dumps.count("demod")) {
        f.open(out_dir + "/demod_symbols.csv");
        for (size_t i = 0; i < symbol_count; ++i) f << demod[i] << "\n";
        f.close();
    }
    if (dumps.count("deinterleave")) {
        f.open(out_dir + "/deinterleave.csv");
        for (size_t i = 0; i < cw_count; ++i)
            f << static_cast<unsigned>(deinterleave[i]) << "\n";
        f.close();
    }
    if (dumps.count("decoded")) {
        f.open(out_dir + "/decoded.bin", std::ios::binary);
        f.write(reinterpret_cast<const char*>(decoded.data()), decoded.size());
        f.close();
    }

    return 0;
}

