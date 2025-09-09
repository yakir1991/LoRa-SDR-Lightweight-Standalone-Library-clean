#include <lora_phy/phy.hpp>

#include <complex>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace lora_phy;

namespace {

void usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " --payload=HEX [--sf=N] [--cr=N] [--bw=HZ] [--out=FILE|--stdout]\n";
}

bool parse_hex_payload(const std::string& hex, std::vector<uint8_t>& out) {
    if (hex.size() % 2 != 0) return false;
    out.clear();
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        const std::string byte_str = hex.substr(i, 2);
        out.push_back(static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16)));
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::string payload_hex;
    std::string out_path;
    bool to_stdout = false;
    lora_params params{};
    params.sf = 7; // defaults

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--payload=", 0) == 0) {
            payload_hex = arg.substr(10);
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
        } else if (arg.rfind("--out=", 0) == 0) {
            out_path = arg.substr(6);
        } else if (arg == "--stdout") {
            to_stdout = true;
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            usage(argv[0]);
            return 1;
        }
    }

    if (payload_hex.empty()) {
        usage(argv[0]);
        return 1;
    }
    if (!to_stdout && out_path.empty()) {
        std::cerr << "Specify --out=FILE or --stdout\n";
        return 1;
    }

    std::vector<uint8_t> payload;
    if (!parse_hex_payload(payload_hex, payload)) {
        std::cerr << "Invalid payload hex string\n";
        return 1;
    }

    const size_t symbol_cap = payload.size() * 2; // Hamming(8,4)
    const size_t N = size_t(1) << params.sf;

    std::vector<uint16_t> symbols(symbol_cap);
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

    ssize_t symbol_count = encode(&ws, payload.data(), payload.size(),
                                  symbols.data(), symbols.size());
    if (symbol_count < 0) {
        std::cerr << "encode() failed\n";
        return 1;
    }

    std::vector<std::complex<float>> iq((symbol_count + 2) * N);
    ssize_t sample_count =
        modulate(&ws, symbols.data(), symbol_count, iq.data(), iq.size());
    if (sample_count < 0) {
        std::cerr << "modulate() failed\n";
        return 1;
    }

    std::ostream* out_stream = nullptr;
    std::ofstream file_stream;
    if (to_stdout) {
        out_stream = &std::cout;
    } else {
        file_stream.open(out_path, std::ios::binary);
        if (!file_stream) {
            std::cerr << "Unable to open output file\n";
            return 1;
        }
        out_stream = &file_stream;
    }

    for (ssize_t i = 0; i < sample_count; ++i) {
        const float re = iq[i].real();
        const float im = iq[i].imag();
        out_stream->write(reinterpret_cast<const char*>(&re), sizeof(float));
        out_stream->write(reinterpret_cast<const char*>(&im), sizeof(float));
    }

    if (file_stream.is_open()) file_stream.close();
    return 0;
}

