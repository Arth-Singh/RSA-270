#!/usr/bin/env bash
# Exhaustive word-seed sweep of every rsaref_md5_state_scan mode on one machine.
# Covers 1-, 2-, 3-, and 4-byte word seeds for the word families and the full
# 32-bit seed space for the three LCG families, both counter transitions, both
# byte orders, and all three digit-width models. Writes one log per family and
# a RUN.txt ledger with commit, source hash, timings, and exit codes.
#
# Usage: scripts/rsaref_state_sweep.sh OUT_DIR [THREADS]
# Exit codes of the scanner: 0 product match, 1 no match, 3 anchor-only hit.
set -euo pipefail
out=${1:?output directory}
threads=${2:-$(nproc)}
root=$(cd "$(dirname "$0")/.." && pwd)
bin="$root/build/current/rsaref_md5_state_scan"
mkdir -p "$out"
{
  echo "commit=$(git -C "$root" rev-parse HEAD)"
  echo "source_sha256=$(sha256sum "$root/src/rsaref_md5_state_scan.cpp" | cut -d' ' -f1)"
  echo "host=$(hostname) threads=$threads started=$(date -u +%FT%TZ)"
} > "$out/RUN.txt"
"$bin" self-test > "$out/self-test.log" 2>&1 || { echo "self-test FAILED" >> "$out/RUN.txt"; exit 1; }
echo "self-test passed" >> "$out/RUN.txt"
for mode in raw-word raw-repeat-word md5-word md5-word-times64 md5-repeat-buffer ansi-rand ms-rand borland-rand; do
  for wb in 1 2 3 4; do
    case $mode in ansi-rand|ms-rand|borland-rand) [ "$wb" = 4 ] || continue ;; esac
    end=$((1 << (8 * wb)))
    log="$out/$mode-w$wb.log"
    echo "$mode w$wb start=$(date -u +%FT%TZ)" >> "$out/RUN.txt"
    set +e
    "$bin" "$mode" 0 "$end" "$threads" "$wb" > "$log" 2>&1
    code=$?
    set -e
    echo "$mode w$wb exit=$code $(tail -1 "$log")" >> "$out/RUN.txt"
    if [ "$code" -eq 0 ] || [ "$code" -eq 3 ]; then
      echo "HIT in $mode w$wb (exit $code); see $log" >> "$out/RUN.txt"
    fi
  done
done
echo "finished=$(date -u +%FT%TZ)" >> "$out/RUN.txt"
