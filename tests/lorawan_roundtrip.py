#!/usr/bin/env python3
import os
import binascii
import subprocess
from pathlib import Path
import secrets

def run_roundtrip(exe: Path, payload: bytes) -> bool:
    hex_payload = binascii.hexlify(payload).decode()
    result = subprocess.run([str(exe), hex_payload], capture_output=True, text=True)
    return result.returncode == 0

def main():
    root = Path(__file__).resolve().parent.parent
    exe = root / 'build' / 'lorawan_roundtrip'
    if not exe.exists():
        raise SystemExit(f"runner not found: {exe}")
    for _ in range(5):
        payload = secrets.token_bytes(8)
        if not run_roundtrip(exe, payload):
            raise SystemExit('roundtrip failed')
    print('LoRaWAN round-trip tests passed')

if __name__ == '__main__':
    main()
