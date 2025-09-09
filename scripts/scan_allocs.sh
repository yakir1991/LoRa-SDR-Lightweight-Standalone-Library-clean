#!/usr/bin/env bash
set -euo pipefail

# scan for dynamic allocation patterns in src and include
OUT_FILE="${1:-alloc_report.txt}"

PATTERN='\b(new|malloc|calloc|realloc|resize|push_back|emplace_back)\b'

# Use ripgrep to search the source tree
rg -n "$PATTERN" src include > "$OUT_FILE" || true

echo "Allocation report saved to $OUT_FILE"
