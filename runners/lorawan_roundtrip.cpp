#include <lorawan/lorawan.hpp>
#include <lora_phy/phy.hpp>

#include <iostream>
#include <sstream>
#include <vector>

static bool hex_to_bytes(const std::string& hex, std::vector<uint8_t>& out) {
    if (hex.size() % 2) return false;
    out.clear();
    for (size_t i = 0; i < hex.size(); i += 2) {
        uint8_t b = static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16));
        out.push_back(b);
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <hex_payload>\n";
        return 1;
    }
    std::vector<uint8_t> payload;
    if (!hex_to_bytes(argv[1], payload)) {
        std::cerr << "Invalid hex input\n";
        return 1;
    }

    lorawan::MHDR mhdr;
    mhdr.mtype = lorawan::MType::UnconfirmedDataUp;
    mhdr.major = 0;

    lorawan::FHDR fhdr;
    fhdr.devaddr = 0x01020304u;
    fhdr.fctrl = 0x00u;
    fhdr.fcnt = 1u;

    lorawan::Frame frame;
    frame.mhdr = mhdr;
    frame.fhdr = fhdr;
    frame.payload = payload;

    lora_phy::lora_workspace ws{};
    lora_phy::lora_params params{};
    params.sf = 7;
    params.cr = 1;
    params.bw = lora_phy::bandwidth::bw_125;
    lora_phy::init(&ws, &params);

    std::vector<uint16_t> symbols(payload.size() * 2 + 32);
    std::vector<uint8_t> tmp(symbols.size() / 2 + 8);
    uint8_t nwk_skey[16] = {};
    ssize_t sc = lorawan::build_frame(&ws, nwk_skey, frame, symbols.data(), symbols.size(),
                                      tmp.data(), tmp.size());
    if (sc < 0) {
        std::cerr << "build_frame failed\n";
        return 2;
    }

    lorawan::Frame parsed;
    ssize_t pc = lorawan::parse_frame(&ws, nwk_skey, symbols.data(), static_cast<size_t>(sc),
                                      parsed, tmp.data(), tmp.size());
    if (pc < 0) {
        std::cerr << "parse_frame failed\n";
        return 3;
    }

    if (parsed.payload != payload) {
        std::cerr << "Round-trip mismatch\n";
        return 4;
    }

    std::cout << "roundtrip ok" << std::endl;
    return 0;
}
