# API Specification
*Version:* 1.0  
*Date:* 2025-02-14

See also: [Porting Notes](PORTING_NOTES.md), [Third-Party Components](THIRD_PARTY.md), [Test Plan](TEST_PLAN.md)

## Workspace

The runtime operates on a caller supplied `lora_workspace` structure.  The
workspace owns all scratch buffers and FFT plans required by the modem.  Buffers
are allocated by the caller before `init()` and handed to the workspace; the
library never performs dynamic memory allocation after initialization.  Typical
fields include symbol and sample buffers, FFT input/output arrays and the
KISS‑FFT plans reused by `demodulate()`.

```
struct lora_workspace {
    /* preallocated by caller */
    uint16_t     *symbol_buf;    /* N entries */
    float complex *fft_in;       /* N samples */
    float complex *fft_out;      /* N*osr samples */

    /* initialized by init() */
    kissfft_plan  plan_fwd;
    kissfft_plan  plan_inv;

    struct lora_metrics metrics; /* updated by processing functions */
    unsigned       osr;          /* oversampling ratio */
    enum bandwidth bw;           /* operating bandwidth */
};
```

The caller retains ownership of the workspace and the memory referenced by its
pointers.  The library never frees or reallocates these buffers.

## Functions

All routines return `0` on success or a negative error code (`-EINVAL`,
`-ERANGE`, …) on failure unless noted otherwise.  Output functions return the
number of elements written when successful.

The `bandwidth` enumeration defines the supported LoRa bandwidths and
currently allows `bw_125` (125 kHz), `bw_250` (250 kHz) and `bw_500`
(500 kHz).

### `int init(struct lora_workspace *ws, const struct lora_params *cfg);`
Initializes the workspace for a given set of parameters.

* `ws` – workspace to populate. Must reference valid buffers.
* `cfg` – modulation and coding parameters (spread factor, bandwidth, coding rate, oversampling, etc.).
* Returns `0` on success or `-EINVAL` if parameters are invalid.

### `void reset(struct lora_workspace *ws);`
Clears runtime counters and metric fields inside `ws` without touching the
preallocated buffers or FFT plans.

### `ssize_t encode(struct lora_workspace *ws,
                     const uint8_t *payload, size_t payload_len,
                     uint16_t *symbols, size_t symbol_cap);`
Encodes a payload into LoRa symbols.

* `payload` – input bytes; caller retains ownership.
* `symbols` – caller provided output buffer with capacity `symbol_cap`.
* Returns number of symbols produced or `-ERANGE` if `symbol_cap` is too small.

### `ssize_t decode(struct lora_workspace *ws,
                     const uint16_t *symbols, size_t symbol_count,
                     uint8_t *payload, size_t payload_cap);`
Decodes a block of symbols into payload bytes.

* `symbols` – input symbol buffer owned by caller.
* `payload` – output buffer supplied by caller.
* Returns number of bytes written or a negative error code on CRC/format error.

### `ssize_t modulate(struct lora_workspace *ws,
                      const uint16_t *symbols, size_t symbol_count,
                      float complex *iq, size_t iq_cap);`
Generates complex time‑domain samples from symbols.

* `symbols` – input symbols.
* `iq` – caller supplied buffer for `symbol_count * (1<<sf) * osr` samples.
* Returns samples written or `-ERANGE` if the buffer is insufficient.

### `ssize_t demodulate(struct lora_workspace *ws,
                        const float complex *iq, size_t sample_count,
                        uint16_t *symbols, size_t symbol_cap);`
Demodulates IQ samples into decided symbols using the workspace FFT plans.

* `iq` – input samples; length must be a multiple of `(1<<sf) * osr`.
* `symbols` – output buffer for decoded symbols.
* Returns number of symbols produced or negative error on invalid sizes.

### `const struct lora_metrics *get_last_metrics(const struct lora_workspace *ws);`
Returns a pointer to the metrics collected during the most recent processing
call (`decode` or `demodulate`).  The caller must not free the returned pointer
and it remains valid until the next call that updates the metrics.

## LoRaWAN helpers

An optional helper module in `include/lorawan/lorawan.hpp` provides small
structures representing LoRaWAN headers along with utilities to build and
parse frames.

### Data structures

* `lorawan::MHDR` – message header carrying the frame type and protocol major
  version.
* `lorawan::FHDR` – frame header containing device address, frame control,
  frame counter and optional MAC commands (`fopts`).
* `lorawan::Frame` – aggregates `MHDR`, `FHDR` and the FRMPayload bytes.

### `ssize_t lorawan::build_frame(lora_phy::lora_workspace *ws,
                                  const uint8_t nwk_skey[16],
                                  const lorawan::Frame &frame,
                                  uint16_t *symbols,
                                  size_t symbol_cap,
                                  uint8_t *tmp_bytes,
                                  size_t tmp_cap);`
Serialises `frame`, computes the LoRaWAN MIC using AES‑128 CMAC with
`nwk_skey`, appends it and encodes the resulting buffer into LoRa symbols using
`lora_phy::encode`.  `tmp_bytes` must point to a caller provided workspace for
the intermediate byte representation.  Returns the number of symbols written or
a negative value on error.

### `ssize_t lorawan::parse_frame(lora_phy::lora_workspace *ws,
                                  const uint8_t nwk_skey[16],
                                  const uint16_t *symbols, size_t count,
                                  lorawan::Frame &out,
                                  uint8_t *tmp_bytes,
                                  size_t tmp_cap);`
Decodes symbols with `lora_phy::decode`, verifies the AES‑128 CMAC‑based MIC
using `nwk_skey` and populates `out` with the parsed fields using `tmp_bytes`
as scratch space.  The return value is the number of payload bytes or a negative
error code.

## Buffer Ownership and Error Handling

All input and output buffers are owned by the caller.  The library reads from or
writes to them only for the duration of the call.  No asynchronous callbacks are
involved; errors are reported solely through return codes.

## Numeric conventions

See `SEMANTIC_COMPATIBILITY.md` for sample scaling, bit ordering and other
semantic requirements needed for vector compatibility with the reference
implementation.

