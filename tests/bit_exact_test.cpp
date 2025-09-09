#include <lora_phy/phy.hpp>
#include <lora_phy/ChirpGenerator.hpp>
#include <complex>
#include <cstdint>
#include <fstream>
#include <iostream>
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

static unsigned cr_to_idx(const std::string& cr) {
    if (cr == "4/5") return 1;
    if (cr == "4/6") return 2;
    if (cr == "4/7") return 3;
    if (cr == "4/8") return 4;
    return 0;
}

static bool load_from_bin(const std::string& path, const Profile& p,
                          std::vector<std::complex<float>>& samples,
                          std::vector<uint8_t>& payload) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    auto read_u32 = [&](uint32_t& v) {
        return static_cast<bool>(f.read(reinterpret_cast<char*>(&v), sizeof(v)));
    };
    uint32_t count = 0;
    if (!read_u32(count)) return false;
    const unsigned bw_khz = p.bw / 1000;
    const unsigned cr_idx = cr_to_idx(p.cr);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t sf_raw, bw_raw, cr_raw, flags_raw, len_raw;
        if (!(read_u32(sf_raw) && read_u32(bw_raw) && read_u32(cr_raw) &&
              read_u32(flags_raw) && read_u32(len_raw)))
            return false;
        const unsigned sf = sf_raw >> 8;
        const unsigned bw = bw_raw >> 8;
        const unsigned cr = cr_raw >> 8;
        const unsigned len = len_raw >> 8;
        char reserved;
        if (!f.read(&reserved, 1)) return false;
        std::vector<uint8_t> pay(len);
        if (!f.read(reinterpret_cast<char*>(pay.data()), len)) return false;
        uint32_t sample_count = 0;
        if (!read_u32(sample_count)) return false;
        std::vector<double> tmp(sample_count * 2);
        if (!f.read(reinterpret_cast<char*>(tmp.data()),
                    tmp.size() * sizeof(double)))
            return false;
        if (sf == p.sf && bw == bw_khz && cr == cr_idx) {
            samples.resize(sample_count);
            for (uint32_t s = 0; s < sample_count; ++s) {
                samples[s] =
                    std::complex<float>(static_cast<float>(tmp[2 * s]),
                                         static_cast<float>(tmp[2 * s + 1]));
            }
            payload = std::move(pay);
            return true;
        }
    }
    return false;
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
        std::vector<uint8_t> expected;
        if (!load_from_bin(p.dir + "/modulation_tests.bin", p, samples,
                           expected)) {
            std::cerr << "Failed to load vectors for profile " << p.name
                      << "\n";
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

