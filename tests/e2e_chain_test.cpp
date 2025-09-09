#include <lora_phy/phy.hpp>
#include <lora_phy/ChirpGenerator.hpp>
#include <cstdint>
#include <complex>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

struct Profile {
    std::string name;
    unsigned sf{};
    unsigned bw{};
    std::string cr;
    std::string dir; // optional
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

    bool ok = true;
    for (const auto& p : profiles) {
        // generate deterministic payload
        const size_t payload_size = 32;
        std::vector<uint8_t> payload(payload_size);
        for (size_t i = 0; i < payload_size; ++i) payload[i] = static_cast<uint8_t>(i & 0xFF);

        // encode to symbols
        std::vector<uint16_t> symbols(payload_size * 2);
        const size_t symbol_count = lora_phy::lora_encode(payload.data(), payload.size(), symbols.data(), p.sf);

        // modulate symbols into IQ samples
        const size_t samples_per_symbol = 1u << p.sf;
        const size_t sample_count = (symbol_count + 2) * samples_per_symbol;
        std::vector<std::complex<float>> samples(sample_count);
        lora_phy::lora_modulate(symbols.data(), symbol_count, samples.data(), p.sf, 1,
                                static_cast<lora_phy::bandwidth>(p.bw), 1.0f, 0x12);

        // dechirp the samples before demodulation
        std::vector<std::complex<float>> dechirped(sample_count);
        std::vector<std::complex<float>> down(samples_per_symbol);
        std::vector<std::complex<float>> scratch(sample_count);
        float phase = 0.0f;
        float scale = lora_phy::bw_scale(static_cast<lora_phy::bandwidth>(p.bw));
        genChirp(down.data(), static_cast<int>(samples_per_symbol), 1,
                 static_cast<int>(samples_per_symbol), 0.0f, true, 1.0f, phase,
                 scale);
        for (size_t s = 0; s < symbol_count + 2; ++s) {
            for (size_t i = 0; i < samples_per_symbol; ++i) {
                dechirped[s * samples_per_symbol + i] =
                    samples[s * samples_per_symbol + i] * down[i];
            }
        }

        // demodulate back
        std::vector<uint16_t> demod(symbol_count);
        lora_phy::lora_demod_workspace ws{};
        lora_phy::lora_demod_init(&ws, p.sf, lora_phy::window_type::window_none,
                                   scratch.data(), scratch.size());
        lora_phy::lora_demodulate(&ws, dechirped.data(), sample_count, demod.data(), 1,
                                   nullptr);
        lora_phy::lora_demod_free(&ws);

        // decode symbols back to bytes
        std::vector<uint8_t> decoded(payload_size);
        lora_phy::lora_decode(demod.data(), symbol_count, decoded.data());

        if (decoded != payload) {
            std::cerr << "Mismatch in profile " << p.name << "\n";
            ok = false;
        } else {
            std::cout << "Profile " << p.name << " passed." << std::endl;
        }
    }

    return ok ? 0 : 1;
}

