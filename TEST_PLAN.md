# Test Plan
*Version:* 1.0  
*Date:* 2025-02-14

See also: [API Specification](API_SPEC.md), [Porting Notes](PORTING_NOTES.md), [Third-Party Components](THIRD_PARTY.md)

## Overview
This document outlines how the library is validated. Build and execution steps are
documented in the project [README](README.md).

## Bit-Exact Regression Tests
Verifies modulation and demodulation against deterministic reference vectors.

1. Generate vectors (once per update):
   ```bash
   scripts/generate_vectors.sh vectors/my_run
   ```
2. Build and run the regression:
   ```bash
   cmake -B build -S .
   cmake --build build
   ./build/bit_exact_test
   ```

The test reports `passed` for profiles that match the golden vectors listed in
`tests/profiles.yaml`.

## BER/PER Sweeps
Evaluates bit and packet error rates over an AWGN channel for the profile matrix.

```bash
python tests/awgn_sweep.py --packets 100 --snr-start 0 --snr-stop 12 --snr-step 0.5 --out logs/awgn_sweep
```

The script writes `awgn_sweep.csv` and PNG plots under the chosen output
directory.

## Zero-Allocation Checks
Ensures that runtime modulation and demodulation perform no dynamic memory
allocation.

```bash
./build/no_alloc_test
```

Passing runs print `No allocations detected`.

## Performance Measurements
Captures throughput and cycle counts per symbol for each profile.

```bash
./build/performance_test
```

Results are stored in `logs/performance.csv` for further analysis.

