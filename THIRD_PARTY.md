# Third-Party Components
*Version:* 1.0  
*Date:* 2025-02-14

See also: [API Specification](API_SPEC.md), [Porting Notes](PORTING_NOTES.md), [Test Plan](TEST_PLAN.md)

This project incorporates external code and artifacts. The sections below credit their sources, summarize any local modifications, and provide reproduction instructions for generated vectors where applicable.

## KISS FFT
- **Source:** [https://github.com/mborgerding/kissfft](https://github.com/mborgerding/kissfft)
- **License:** BSD-3-Clause
- **Usage:** Provides FFT routines for modulation and demodulation. The project embeds a minimal header-only adaptation at `include/lora_phy/kissfft.hh` (legacy copy in `legacy/kissfft.hh`).
- **Local modifications:** Unused features were removed and the interface was trimmed for static allocation and standalone use.

## LoRa-SDR (original project)
- **Source:** [https://github.com/myriadrf/LoRa-SDR](https://github.com/myriadrf/LoRa-SDR)
- **License:** Boost Software License 1.0
- **Usage:** The standalone library originates from the LoRa-SDR reference implementation. Portions of the PHY processing chain and the baseline test vectors were derived from it.
- **Local modifications:** Code was refactored to remove the Pothos framework, rely solely on KISS FFT, and avoid runtime allocations.
- **Vector regeneration:** Baseline vectors can be reproduced using `scripts/generate_baseline_vectors.py`, which invokes `scripts/generate_vectors.sh` and the original LoRa-SDR binary. The generated vectors are stored under `legacy_vectors/lorasdr_baseline`.

