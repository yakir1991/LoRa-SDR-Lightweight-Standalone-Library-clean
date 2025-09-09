# LoRa-SDR Lightweight Standalone Library

A lightweight, standalone implementation of LoRa PHY layer based on the original LoRa-SDR (MyriadRF) with KISS-FFT as the sole FFT backend.

## Features

- **Zero Runtime Allocations**: All buffers allocated during initialization
- **KISS-FFT Only**: Single FFT dependency for embedded compatibility
- **Bit-Exact Compatibility**: Matches original LoRa-SDR behavior
- **Comprehensive Testing**: Golden vectors for validation

## Supported Parameters

- **Spreading Factors**: 7, 8, 9, 10, 11, 12
- **Bandwidths**: 125, 250, 500 kHz
- **Coding Rates**: 4/5, 4/6, 4/7, 4/8
- **Header Mode**: Explicit header
- **Error Correction**: Hamming codes (8/4, 7/4), Parity codes (6/4, 5/4)

## Quick Start

### Building

```bash
mkdir build && cd build
cmake ..
make
```

### Running Tests

```bash
# Run lora_phy_tests with golden vectors
./lora_phy_tests
```

### Using the Library

```cpp
#include "lora_phy/phy.hpp"

// Initialize LoRa parameters
LoRaParams params = {
    .sf = 7,
    .bw = 125000,
    .cr = 1,
    .explicit_header = true,
    .crc_enabled = true
};

// Initialize PHY
LoRaPHY phy(params);

// Modulate payload
std::vector<uint8_t> payload = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
auto iq_samples = phy.modulate(payload);

// Demodulate IQ samples
auto decoded_payload = phy.demodulate(iq_samples);
```

## Project Structure

```
├── src/phy/           # Core PHY implementation
├── include/lora_phy/  # Public headers
├── runners/           # CLI utilities
├── tests/             # Test suite
├── vectors/golden/    # Golden test vectors
└── scripts/           # Build and test scripts
```

## Golden Vectors

The `vectors/golden/` directory contains essential test vectors:

- `awgn_tests.bin`: Additive white Gaussian noise channel simulations
- `crc_tests.bin`: CRC calculation and verification cases
- `detection_tests.bin`: Symbol detection and synchronization scenarios
- `hamming_tests.bin`: Hamming code encoding/decoding checks
- `interleaver_tests.bin`: Bit interleaving and deinterleaving patterns
- `modulation_tests.bin`: Modulation and demodulation round-trip samples
- `performance_tests.bin`: Stress cases for performance benchmarking
- `sync_word_tests.bin`: Sync word generation and detection data

These vectors are generated from the original LoRa-SDR submodule and provide bit-exact validation.

**Note:** Before running any vector generation scripts, synchronize the `external/LoRa-SDR` submodule:

```bash
git submodule update --init --recursive
```

## Testing

The library includes comprehensive tests:

- **Bit-Exact Tests**: Compare against golden vectors
- **End-to-End Tests**: Full modulation/demodulation chain
- **Performance Tests**: Timing and memory usage
- **No-Allocation Tests**: Verify zero runtime allocations

## Dependencies

- **KISS-FFT**: Header-only FFT library (included)
- **C++17**: Requires a C++17-compatible compiler
- **CMake**: Build system

## License

This project is based on the original LoRa-SDR implementation by MyriadRF.
See THIRD_PARTY.md for licensing details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass
5. Submit a pull request



## Credits and Acknowledgments

This project is based on the original LoRa-SDR implementation by MyriadRF.

### Original Implementation
- **Repository**: [myriadrf/LoRa-SDR](https://github.com/myriadrf/LoRa-SDR)
- **License**: GPL-3.0
- **Author**: MyriadRF
- **Purpose**: Reference implementation for validation

### Technology Credits
- **Semtech**: LoRa technology and specifications
- **KISS-FFT**: FFT library (included in original)
- **LoRa Alliance**: LoRaWAN specifications

### Submodule Reference
The original LoRa-SDR submodule is included for:
- Generating golden test vectors
- Validating bit-exact compatibility
- Reference implementation comparison

To initialize the submodule:
```bash
git submodule update --init --recursive
```

## License Notice
This lightweight implementation is derived from the original LoRa-SDR
but is designed to be standalone with minimal dependencies.

## References

- [LoRa-SDR Original Implementation](https://github.com/myriadrf/LoRa-SDR)
- [KISS-FFT](https://github.com/mborgerding/kissfft)
- [LoRaWAN Specification](https://lora-alliance.org/resource_hub/lorawan-specification-v1-0-3/)
