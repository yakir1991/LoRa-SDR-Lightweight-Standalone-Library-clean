# Numeric and Semantic Compatibility Decisions

This document captures conventions that the lightweight LoRa PHY library
must follow to remain bit‑exact with the original LoRa‑SDR implementation.

| Aspect | Decision |
| --- | --- |
| IQ samples | Complex `float32` values with I in the real part and Q in the imaginary part. Samples are normalized to the range \[-1.0, 1.0]. Modulation clamps the requested amplitude to this interval and demodulation rescales inputs that exceed it. The file `legacy_vectors/lorasdr_baseline/iq_norm.cfloat.b64` decodes to two complex samples `(1.0, 0.0)` and `(-1.0, 0.0)` demonstrating the amplitude limits. |
| Symbol length | Each LoRa symbol contains exactly `N = 2^SF` samples. Demodulation assumes `sample_count` is an integer multiple of `N`. |
| Dechirp | Base chirps start at phase 0.0. Modulation uses up‑chirps; demodulation multiplies by a down‑chirp generated with the same `genChirp` routine. |
| Argmax policy | Symbol decision and offset estimation select the first FFT bin with the highest magnitude‑squared. Ties resolve to the lowest index. |
| Bit ordering | Gray mapping, diagonal interleaving and whitening operate on least‑significant bit first. Whitening uses the polynomial $x^7 + x^5 + 1$ with an initial LFSR value of `0xFF`. |
| CRC | Modified CCITT‑16 with polynomial `0x1021` and SX1272 stream masking. |
| Sync word | Two‑byte sync word applied via XOR against the header. Default private‑network value is `0x34`. |

To verify compatibility between the new library and the reference
implementation, generate vectors for both and compare them with
`scripts/compare_vectors.py`. The tool performs a SHA256 check on every file
and reports mismatches.
