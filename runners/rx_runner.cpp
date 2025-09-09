#include <lora_phy/phy.hpp>

#include <complex>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace lora_phy;

namespace {

void usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " [--in=FILE] [--sf=N] [--cr=N] [--bw=HZ] [--report-offsets]\n";
    std::cerr << "Input samples are float32 IQ pairs" << std::endl;
}

} // namespace

int main(int argc, char** argv) {
    std::string in_path;
    lora_params params{};
    params.sf = 7; // defaults
    bool report_offsets = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--in=", 0) == 0) {
            in_path = arg.substr(5);
        } else if (arg.rfind("--sf=", 0) == 0) {
            params.sf = static_cast<unsigned>(std::stoul(arg.substr(5)));
        } else if (arg.rfind("--cr=", 0) == 0) {
            params.cr = static_cast<unsigned>(std::stoul(arg.substr(5)));
        } else if (arg.rfind("--bw=", 0) == 0) {
            unsigned val = static_cast<unsigned>(std::stoul(arg.substr(5)));
            if (val == 125000)
                params.bw = bandwidth::bw_125;
            else if (val == 250000)
                params.bw = bandwidth::bw_250;
            else if (val == 500000)
                params.bw = bandwidth::bw_500;
            else {
                std::cerr << "Unsupported bandwidth\n";
                return 1;
            }
        } else if (arg == "--report-offsets") {
            report_offsets = true;
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            usage(argv[0]);
            return 1;
        }
    }

    std::istream* in_stream = nullptr;
    std::ifstream file_stream;
    if (!in_path.empty()) {
        file_stream.open(in_path, std::ios::binary);
        if (!file_stream) {
            std::cerr << "Unable to open input file\n";
            return 1;
        }
        in_stream = &file_stream;
    } else {
        in_stream = &std::cin;
    }

    std::vector<std::complex<float>> samples;
    float re = 0.0f, im = 0.0f;
    while (in_stream->read(reinterpret_cast<char*>(&re), sizeof(float))) {
        if (!in_stream->read(reinterpret_cast<char*>(&im), sizeof(float))) break;
        samples.emplace_back(re, im);
    }

    if (samples.empty()) {
        std::cerr << "No samples provided\n";
        return 1;
    }

    const size_t N = size_t(1) << params.sf;
    if (samples.size() % N != 0) {
        std::cerr << "Sample count not multiple of symbol size\n";
        return 1;
    }
    const size_t symbol_count = samples.size() / N;

    std::vector<uint16_t> symbols(symbol_count);
    std::vector<std::complex<float>> fft_in(N);
    std::vector<std::complex<float>> fft_out(N);

    lora_workspace ws{};
    ws.symbol_buf = symbols.data();
    ws.fft_in = fft_in.data();
    ws.fft_out = fft_out.data();

    if (init(&ws, &params) != 0) {
        std::cerr << "Failed to initialise workspace\n";
        return 1;
    }

    ssize_t demod_syms = demodulate(&ws, samples.data(), samples.size(),
                                    symbols.data(), symbols.size());
    if (demod_syms < 0) {
        std::cerr << "demodulate() failed\n";
        return 1;
    }

    std::vector<uint8_t> decoded(demod_syms / 2);
    ssize_t decoded_bytes =
        decode(&ws, symbols.data(), demod_syms, decoded.data(), decoded.size());
    if (decoded_bytes < 0) {
        std::cerr << "decode() failed\n";
        return 1;
    }

    const lora_metrics* m = get_last_metrics(&ws);

    std::cout << "Payload: ";
    for (ssize_t i = 0; i < decoded_bytes; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<unsigned>(decoded[i]);
    }
    std::cout << std::dec << "\n";

    if (report_offsets && m) {
        std::cout << "CRC OK: " << (m->crc_ok ? "yes" : "no") << "\n";
        std::cout << "CFO: " << m->cfo << "\n";
        std::cout << "Time offset: " << m->time_offset << "\n";
    }

    return 0;
}

