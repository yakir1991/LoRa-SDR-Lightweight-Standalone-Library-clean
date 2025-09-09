#include <lora_phy/phy.hpp>
#include <lora_phy/ChirpGenerator.hpp>
#include <chrono>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#ifdef __x86_64__
#include <x86intrin.h>
#endif

struct Profile {
    std::string name;
    unsigned sf{};
    unsigned bw{};
    std::string cr;
    std::string dir;
};

static std::string trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool load_profiles(const std::string& path, std::vector<Profile>& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    Profile current;
    bool in_profile = false;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '-') {
            if (in_profile) out.push_back(current);
            current = Profile();
            in_profile = true;
            continue;
        }
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = trim(line.substr(0, colon));
        std::string val = trim(line.substr(colon + 1));
        if (key == "name") current.name = val;
        else if (key == "sf") current.sf = static_cast<unsigned>(std::stoul(val));
        else if (key == "bw") current.bw = static_cast<unsigned>(std::stoul(val));
        else if (key == "cr") current.cr = val;
        else if (key == "dir") current.dir = val;
    }
    if (in_profile) out.push_back(current);
    return true;
}

int main() {
    std::vector<Profile> profiles;
    if (!load_profiles("tests/profiles.yaml", profiles)) {
        std::cerr << "Failed to load profiles.yaml\n";
        return 1;
    }

    const size_t PACKETS = 1000;
    const size_t PAYLOAD_SIZE = 32;

    const char* env_run = std::getenv("RUN_ID");
    std::string run_id = env_run ? env_run : "run";
    std::string path = "logs/performance_" + run_id + ".csv";

    std::system("mkdir -p logs");
    std::ofstream csv(path);
    csv << "run_id,profile,sf,N,pps,cycles_per_symbol\n";

    for (const auto& p : profiles) {
        // deterministic payload
        std::vector<uint8_t> payload(PAYLOAD_SIZE);
        for (size_t i = 0; i < PAYLOAD_SIZE; ++i) payload[i] = static_cast<uint8_t>(i & 0xFF);

        // encode once to get symbol count
        std::vector<uint16_t> symbols(PAYLOAD_SIZE * 2);
        const size_t symbol_count = lora_phy::lora_encode(payload.data(), payload.size(), symbols.data(), p.sf);
        const size_t samples_per_symbol = 1u << p.sf;
        const size_t sample_count = (symbol_count + 2) * samples_per_symbol;

        std::vector<std::complex<float>> samples(sample_count);
        std::vector<std::complex<float>> dechirped(sample_count);
        std::vector<std::complex<float>> scratch(sample_count);
        std::vector<uint16_t> demod(symbol_count);

        // precompute downchirp for dechirp
        std::vector<std::complex<float>> down(samples_per_symbol);
        float phase = 0.0f;
        float scale = lora_phy::bw_scale(static_cast<lora_phy::bandwidth>(p.bw));
        genChirp(down.data(), static_cast<int>(samples_per_symbol), 1,
                 static_cast<int>(samples_per_symbol), 0.0f, true, 1.0f, phase,
                 scale);

        lora_phy::lora_demod_workspace ws{};
        lora_phy::lora_demod_init(&ws, p.sf, lora_phy::window_type::window_none,
                                   scratch.data(), scratch.size());

        auto t_start = std::chrono::high_resolution_clock::now();
#ifdef __x86_64__
        unsigned long long c_start = __rdtsc();
#else
        auto c_start = std::chrono::high_resolution_clock::now();
#endif

        for (size_t pkt = 0; pkt < PACKETS; ++pkt) {
            lora_phy::lora_modulate(symbols.data(), symbol_count, samples.data(),
                                    p.sf, 1,
                                    static_cast<lora_phy::bandwidth>(p.bw), 1.0f,
                                    0x12);
            for (size_t s = 0; s < symbol_count + 2; ++s) {
                for (size_t i = 0; i < samples_per_symbol; ++i) {
                    dechirped[s * samples_per_symbol + i] =
                        samples[s * samples_per_symbol + i] * down[i];
                }
            }
            lora_phy::lora_demodulate(&ws, dechirped.data(), sample_count,
                                      demod.data(), 1, nullptr);
        }

#ifdef __x86_64__
        unsigned long long c_end = __rdtsc();
#else
        auto c_end = std::chrono::high_resolution_clock::now();
#endif
        auto t_end = std::chrono::high_resolution_clock::now();

        lora_phy::lora_demod_free(&ws);

        double seconds =
            std::chrono::duration<double>(t_end - t_start).count();
        double pps = static_cast<double>(PACKETS) / seconds;
        unsigned N = 1u << p.sf;
#ifdef __x86_64__
        double cycles = static_cast<double>(c_end - c_start);
        double cycles_per_symbol =
            cycles / (static_cast<double>(symbol_count) * PACKETS);
        csv << run_id << ',' << p.name << ',' << p.sf << ',' << N << ','
            << pps << ',' << cycles_per_symbol << '\n';
        std::cout << '[' << run_id << "] " << p.name << ": " << pps
                  << " pps, " << cycles_per_symbol << " cycles/symbol"
                  << std::endl;
#else
        (void)c_start;
        (void)c_end;
        csv << run_id << ',' << p.name << ',' << p.sf << ',' << N << ','
            << pps << ',' << "N/A" << '\n';
        std::cout << '[' << run_id << "] " << p.name << ": " << pps
                  << " pps, N/A cycles/symbol" << std::endl;
#endif
    }

    return 0;
}

