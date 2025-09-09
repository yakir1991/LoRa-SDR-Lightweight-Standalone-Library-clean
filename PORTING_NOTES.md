# Porting Notes
*Version:* 1.0  
*Date:* 2025-02-14

See also: [API Specification](API_SPEC.md), [Third-Party Components](THIRD_PARTY.md), [Test Plan](TEST_PLAN.md)

This document summarizes core functions, dependencies, and allocation models for selected modules in the LoRa SDR lightweight library.

The PHY supports bandwidths of 125, 250 and 500 kHz selectable via the
`bandwidth` enumeration in `lora_params`.

| Module | Core Functions | Dependencies | Allocation Model | Notes |
| --- | --- | --- | --- | --- |
| `LoRaMod.cpp` | `LoRaMod::work` | Pothos framework (`Pothos::Block`, `Pothos::BufferChunk`, labels) | Uses `Pothos::BufferChunk` for payload; generates chirps in-place | Pothos block wrapper; no Poco/JSON |
| `LoRaDemod.cpp` | `LoRaDemod::work` | Pothos framework, `LoRaDetector` (FFT via kissfft) | Output via `Pothos::BufferChunk`; uses `std::vector` for chirp tables | Pothos block wrapper; no Poco/JSON |
| `LoRaEncoder.cpp` | `LoRaEncoder::work`, `encodeFec` | Pothos framework, `LoRaCodes.hpp` utilities | `std::vector` for data and symbols; output `Pothos::BufferChunk` | Pothos block wrapper; no Poco/JSON |
| `LoRaDecoder.cpp` | `LoRaDecoder::work`, `drop` | Pothos framework, `LoRaCodes.hpp` | `std::vector` for buffers; output `Pothos::BufferChunk` | Pothos block wrapper; no Poco/JSON |
| `ChirpGenerator.hpp` | `genChirp` | `<complex>`, `<cmath>` (includes `Pothos/Config.hpp` for macros) | Writes to caller-provided buffer; no dynamic allocation | Supports bandwidth scaling via an extra parameter |
| `LoRaDetector.hpp` | `feed`, `detect` | `kissfft.hh`, `<complex>` | Uses caller-provided FFT work buffers and plan | Independent; no external framework |
| `LoRaCodes.hpp` | `sx1272DataChecksum`, `diagonalInterleaveSx`, `grayToBinary16`, `SX1232RadioComputeWhitening` | Standard library (`<cstdint>`) only | Operates on caller buffers; no dynamic allocation | Contains CRC, interleaving, Gray mapping, whitening |
| `kissfft.hh` | `kissfft::transform` | `<complex>`, `<vector>` (optional `<alloca.h>`) | Twiddles and stage data allocated in constructor (init-only) | Standalone FFT backend |

# Module Inventory

| Module | Files |
| --- | --- |
| Encoder/Decoder | `LoRaEncoder.cpp`, `LoRaDecoder.cpp`, `LoRaCodes.hpp` |
| Mod/Demod | `LoRaMod.cpp`, `LoRaDemod.cpp` |
| Chirp/Detector | `ChirpGenerator.hpp`, `LoRaDetector.hpp` |
| Utilities | `BlockGen.cpp`, `TestCodesSx.cpp`, `TestDetector.cpp`, `TestGen.cpp`, `TestHamming.cpp`, `TestLoopback.cpp` |
| FFT | `kissfft.hh` |

## Pothos-only wrappers

These files remain in the legacy portion and will not be ported to the core library:

- `BlockGen.cpp`
- `TestGen.cpp`
- `TestHamming.cpp`
- `TestDetector.cpp`
- `TestLoopback.cpp`

## Files to extract

| Path | Allocation |
| --- | --- |
| `LoRaMod.cpp` | Runtime |
| `LoRaDemod.cpp` | Init + runtime |
| `LoRaEncoder.cpp` | Runtime |
| `LoRaDecoder.cpp` | Runtime |
| `ChirpGenerator.hpp` | None |
| `LoRaDetector.hpp` | Initialization only |
| `LoRaCodes.hpp` | None |
| `kissfft.hh` | Initialization only |
| `TestCodesSx.cpp` | Runtime |


## Zero-allocation scan

Run `scripts/scan_allocs.sh` to search `src` and `include` for dynamic allocation patterns such as `new`, `malloc`, `resize`, or `push_back`. The script writes matches to `alloc_report.txt`. The file is empty when the code base is "zero-alloc".

```bash
bash scripts/scan_allocs.sh
cat alloc_report.txt
```

## Numeric & semantic compatibility

Compare vectors produced by the original LoRa-SDR code and the new library with
`scripts/compare_vectors.py`. The script hashes all files in two directories and
reports any mismatch, highlighting deviations in numeric conventions.
