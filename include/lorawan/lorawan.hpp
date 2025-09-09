#pragma once

#include <cstdint>
#include <vector>

#include <lora_phy/phy.hpp>

namespace lorawan {

// Basic LoRaWAN message types
enum class MType : uint8_t {
    JoinRequest = 0,
    JoinAccept = 1,
    UnconfirmedDataUp = 2,
    UnconfirmedDataDown = 3,
    ConfirmedDataUp = 4,
    ConfirmedDataDown = 5,
    RFU = 6,
    Proprietary = 7,
};

struct MHDR {
    MType   mtype{MType::UnconfirmedDataUp};
    uint8_t major{0};
};

struct MACCommand {
    uint8_t cid{};
    std::vector<uint8_t> payload;
};

struct FHDR {
    uint32_t devaddr{};
    uint8_t  fctrl{}; // lower 4 bits encode FOpts length
    uint16_t fcnt{};
    std::vector<uint8_t> fopts; // raw bytes of MAC commands
};

struct Frame {
    MHDR mhdr;
    FHDR fhdr;
    std::vector<uint8_t> payload; // FRMPayload bytes
};

// Compute the LoRaWAN MIC using AES-128 CMAC.
// @param nwk_skey 16-byte network session key used for MIC calculation.
// @param uplink  true for uplink frames, false for downlink.
// @param devaddr Device address.
// @param fcnt    32-bit frame counter.
// @param data    Pointer to MHDR||FHDR||(FPort||FRMPayload) bytes.
// @param len     Length of @p data in bytes.
uint32_t compute_mic(const uint8_t nwk_skey[16],
                     bool uplink,
                     uint32_t devaddr,
                     uint32_t fcnt,
                     const uint8_t* data,
                     size_t len);

// Build @p frame into LoRa symbols using lora_phy::encode()
ssize_t build_frame(lora_phy::lora_workspace* ws,
                    const uint8_t nwk_skey[16],
                    const Frame& frame,
                    uint16_t* symbols,
                    size_t symbol_cap,
                    uint8_t* tmp_bytes,
                    size_t tmp_cap);

// Parse symbols back into a Frame verifying the MIC
ssize_t parse_frame(lora_phy::lora_workspace* ws,
                    const uint8_t nwk_skey[16],
                    const uint16_t* symbols,
                    size_t symbol_count,
                    Frame& out,
                    uint8_t* tmp_bytes,
                    size_t tmp_cap);

} // namespace lorawan

