#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

# Subdirectory name under vectors/ for this run
SUBDIR=${1:-sf7}
# Oversampling ratio
OSR=${2:-1}

# Generate vectors for supported bandwidths via the standalone library
for BW in 125000 250000 500000; do
    python3 "$SCRIPT_DIR/generate_lora_phy_vectors.py" \
        --sf=7 --seed=1 --bytes=16 --osr="$OSR" --bw="$BW" \
        --out="${SUBDIR}_bw$((BW/1000))_nowin"

    python3 "$SCRIPT_DIR/generate_lora_phy_vectors.py" \
        --sf=7 --seed=1 --bytes=16 --osr="$OSR" --bw="$BW" \
        --window=hann --out="${SUBDIR}_bw$((BW/1000))_hann"
done

# Generate matching vectors via the original LoRa-SDR (125 kHz only)
python3 "$SCRIPT_DIR/generate_baseline_vectors.py" \
    --sf=7 --cr=4/5 --snr=30 --seed=1 --out="$SUBDIR"

echo "Vectors generated under vectors/lora_phy/${SUBDIR}_bw125_nowin, vectors/lora_phy/${SUBDIR}_bw250_nowin, vectors/lora_phy/${SUBDIR}_bw500_nowin and vectors/lorasdr/$SUBDIR"
