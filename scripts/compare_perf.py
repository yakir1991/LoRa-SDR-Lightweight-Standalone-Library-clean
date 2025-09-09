import csv
import sys
from typing import Dict


def load(path: str) -> Dict[str, Dict[str, float]]:
    data: Dict[str, Dict[str, float]] = {}
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            data[row["profile"]] = {
                "pps": float(row["pps"]),
                "cycles_per_symbol": float(row["cycles_per_symbol"]),
            }
    return data


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: compare_perf.py <baseline.csv> <new.csv>")
        return 1

    base = load(sys.argv[1])
    new = load(sys.argv[2])

    reg = []
    for profile, metrics in new.items():
        if profile not in base:
            continue
        b = base[profile]
        if metrics["pps"] < b["pps"] or metrics["cycles_per_symbol"] > b["cycles_per_symbol"]:
            reg.append((profile, b, metrics))

    if reg:
        print("REGRESSION DETECTED")
        for profile, b, n in reg:
            print(
                f"{profile}: pps {b['pps']:.2f}->{n['pps']:.2f}, cycles/sym {b['cycles_per_symbol']:.2f}->{n['cycles_per_symbol']:.2f}"
            )
        return 2
    else:
        print("No regressions detected.")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
