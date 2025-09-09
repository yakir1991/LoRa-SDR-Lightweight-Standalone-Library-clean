#!/usr/bin/env python3
"""Generate reference vectors using the new lora_phy library.

The script runs a small CLI that exercises the lora_phy library and
writes deterministic payloads, symbols and IQ samples.  A manifest file
with SHA256 checksums is written so downstream tests can verify the data.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import pathlib
import subprocess
import sys
import math
from dataclasses import asdict, dataclass
from typing import List


def run(cmd: List[str]) -> None:
    """Run ``cmd`` and raise if it exits with a non-zero status."""

    print(f"[run] {' '.join(cmd)}", file=sys.stderr)
    subprocess.run(cmd, check=True)


def compute_checksum(path: pathlib.Path) -> str:
    """Return the SHA256 checksum for ``path``."""

    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


@dataclass
class FileRecord:
    name: str
    sha256: str


@dataclass
class Manifest:
    sf: int
    seed: int
    bytes: int
    osr: int
    bw: int
    files: List[FileRecord]


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate vectors via lora_phy")
    parser.add_argument("--sf", type=int, required=True, help="Spreading factor")
    parser.add_argument("--seed", type=int, default=0, help="Random seed")
    parser.add_argument("--bytes", type=int, default=16, help="Number of payload bytes")
    parser.add_argument("--osr", type=int, default=1, help="Oversampling ratio")
    parser.add_argument("--bw", type=int, default=125000, help="LoRa bandwidth in Hz")
    parser.add_argument(
        "--out",
        required=True,
        help="Output subdirectory name under vectors/lora_phy",
    )
    parser.add_argument(
        "--binary",
        default=os.environ.get("LORAPHY_VECTOR_BIN", "build/lora_phy_vector_dump"),
        help="Path to the lora_phy_vector_dump binary",
    )
    parser.add_argument("--cfo-bins", type=float, default=0.0, help="CFO in FFT bins")
    parser.add_argument("--time-offset", type=float, default=0.0, help="Sample time offset")
    parser.add_argument(
        "--window",
        default="none",
        choices=["none", "hann"],
        help="Analysis window to apply during demodulation",
    )
    args = parser.parse_args()

    vector_bin = pathlib.Path(args.binary).resolve()
    if not vector_bin.is_file():
        parser.error(f"Vector dump binary not found: {vector_bin}")

    base_dir = pathlib.Path("vectors/lora_phy")
    out_dir = base_dir / args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        str(vector_bin),
        f"--sf={args.sf}",
        f"--seed={args.seed}",
        f"--bytes={args.bytes}",
        f"--out={out_dir}",
        f"--osr={args.osr}",
        f"--bw={args.bw}",
    ]
    if args.window != "none":
        cmd.append(f"--window={args.window}")
    run(cmd)

    iq_path = out_dir / "iq_samples.csv"
    if (args.cfo_bins != 0.0 or args.time_offset != 0.0) and iq_path.is_file():
        lines = iq_path.read_text().strip().splitlines()
        samples: List[complex] = []
        for line in lines:
            re, im = line.split(",")
            samples.append(complex(float(re), float(im)))
        N = (1 << args.sf) * args.osr
        if args.cfo_bins != 0.0:
            for n, s in enumerate(samples):
                ph = 2.0 * math.pi * args.cfo_bins * (n % N) / N
                rot = complex(math.cos(ph), math.sin(ph))
                samples[n] = s * rot
        if args.time_offset != 0.0:
            shift = int(round(args.time_offset))
            if shift > 0:
                samples = samples[shift:] + [0j] * shift
            elif shift < 0:
                shift = -shift
                samples = [0j] * shift + samples[:-shift]
        out_iq = out_dir / "iq_samples_offset.csv"
        with out_iq.open("w") as handle:
            for s in samples:
                handle.write(f"{s.real},{s.imag}\n")

    files: List[FileRecord] = []
    for path in sorted(out_dir.glob("*")):
        if path.name == "manifest.json":
            continue
        b64_path = path.with_suffix(path.suffix + ".b64")
        with path.open("rb") as src, b64_path.open("wb") as dst:
            base64.encode(src, dst)
        path.unlink()
        files.append(FileRecord(b64_path.name, compute_checksum(b64_path)))

    manifest = Manifest(args.sf, args.seed, args.bytes, args.osr, args.bw, files)
    with (out_dir / "manifest.json").open("w") as handle:
        json.dump(asdict(manifest), handle, indent=2)


if __name__ == "__main__":
    main()
