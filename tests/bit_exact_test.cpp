#include <lora_phy/phy.hpp>
#include <lora_phy/ChirpGenerator.hpp>
#include <complex>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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

static bool load_iq_samples(const std::string& path, std::vector<std::complex<float>>& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty()) continue;
        std::stringstream ss(line);
        float re, im;
        char comma;
        if (!(ss >> re)) return false;
        if (!(ss >> comma)) return false;
        if (!(ss >> im)) return false;
        out.emplace_back(re, im);
    }
    return true;
}

static bool load_payload(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

int main() {
    std::vector<Profile> profiles;
    if (!load_profiles("tests/profiles.yaml", profiles)) {
        std::cerr << "Failed to load profiles.yaml\n";
        return 1;
    }
    bool ok = true;
    for (const auto& p : profiles) {
        if (p.dir.empty()) {
            std::cout << "Skipping profile " << p.name
                      << " (no vector directory)" << std::endl;
            continue;
        }
        std::vector<std::complex<float>> samples;
        if (!load_iq_samples(p.dir + "/iq_samples.csv", samples)) {
            std::cerr << "Failed to load IQ samples for profile " << p.name << "\n";
            ok = false;
            continue;
        }
        std::vector<uint8_t> expected;
        // golden payload produced by the reference pipeline
        if (!load_payload(p.dir + "/decoded.bin", expected)) {
            std::cerr << "Failed to load decoded payload for profile " << p.name << "\n";
            ok = false;
            continue;
        }
        const size_t sample_count = samples.size();
        const size_t samples_per_symbol = 1u << p.sf;
        if (sample_count % samples_per_symbol != 0) {
            std::cerr << "Invalid sample count for profile " << p.name << "\n";
            ok = false;
            continue;
        }
        const size_t symbol_count = sample_count / samples_per_symbol;
        if (symbol_count < 2) {
            std::cerr << "Not enough symbols for profile " << p.name << "\n";
            ok = false;
            continue;
        }
        const size_t data_symbols = symbol_count - 2;
        std::vector<std::complex<float>> dechirped(sample_count);
        std::vector<std::complex<float>> down(samples_per_symbol);
        float phase = 0.0f;
        float scale = lora_phy::bw_scale(static_cast<lora_phy::bandwidth>(p.bw));
        genChirp(down.data(), static_cast<int>(samples_per_symbol), 1,
                 static_cast<int>(samples_per_symbol), 0.0f, true, 1.0f, phase,
                 scale);
        for (size_t s = 0; s < symbol_count; ++s) {
            for (size_t i = 0; i < samples_per_symbol; ++i) {
                dechirped[s * samples_per_symbol + i] =
                    samples[s * samples_per_symbol + i] * down[i];
            }
        }

        std::vector<uint16_t> demod(data_symbols);
        lora_phy::lora_demod_workspace ws{};
        std::vector<std::complex<float>> scratch(sample_count);
        lora_phy::lora_demod_init(&ws, p.sf, lora_phy::window_type::window_none,
                                   scratch.data(), scratch.size());
        lora_phy::lora_demodulate(&ws, dechirped.data(), sample_count,
                                   demod.data(), 1, nullptr);
        lora_phy::lora_demod_free(&ws);
        std::vector<uint8_t> decoded(expected.size());
        lora_phy::lora_decode(demod.data(), data_symbols, decoded.data());
        if (decoded != expected) {
            std::cerr << "Mismatch in profile " << p.name << ":\n";
            for (size_t i = 0; i < expected.size(); ++i) {
                if (decoded[i] != expected[i]) {
                    std::cerr << "  byte " << i << ": expected "
                              << static_cast<unsigned>(expected[i]) << " got "
                              << static_cast<unsigned>(decoded[i]) << "\n";
                }
            }
            ok = false;
        } else {
            std::cout << "Profile " << p.name << " passed." << std::endl;
        }
    }
    return ok ? 0 : 1;
}

