#!/usr/bin/env python3
"""Compare two vector directories for bit-for-bit compatibility.

Both directories must contain the files to compare.  If a ``manifest.json``
file is present it will be ignored; actual file contents are hashed.
"""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import sys
from typing import Dict


def compute_checksum(path: pathlib.Path) -> str:
    """Return the SHA256 checksum for ``path``."""

    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_checksums(directory: pathlib.Path) -> Dict[str, str]:
    """Return a mapping of file name to SHA256 checksum for ``directory``."""

    checksums: Dict[str, str] = {}
    for path in sorted(directory.glob("*")):
        if path.name == "manifest.json" or not path.is_file():
            continue
        checksums[path.name] = compute_checksum(path)
    return checksums


def main() -> None:
    parser = argparse.ArgumentParser(description="Compare vector directories")
    parser.add_argument("ref", help="Reference vector directory")
    parser.add_argument("test", help="Test vector directory")
    args = parser.parse_args()

    ref_dir = pathlib.Path(args.ref)
    test_dir = pathlib.Path(args.test)
    ref = load_checksums(ref_dir)
    test = load_checksums(test_dir)

    ok = True
    for name, sha in ref.items():
        other = test.get(name)
        if other is None:
            print(f"missing: {name}", file=sys.stderr)
            ok = False
        elif other != sha:
            print(f"mismatch: {name}", file=sys.stderr)
            ok = False
    for name in set(test) - set(ref):
        print(f"extra file: {name}", file=sys.stderr)
        ok = False

    if ok:
        print("vectors match")
        sys.exit(0)
    sys.exit(1)


if __name__ == "__main__":
    main()
