#!/usr/bin/env bash
# Exhaustive word-seed sweep of every rsaref_md5_state_scan mode on one machine.
# Covers 1-, 2-, 3-, and 4-byte word seeds for the word families and the full
# distinct streams of the three LCG families, both counter transitions, both
# byte orders, and all three digit-width models. Writes one log per family and
# a RUN.txt ledger with commit, source hash, timings, and exit codes.
#
# Usage: scripts/rsaref_state_sweep.sh OUT_DIR [THREADS] [all|lcg]
# Exit codes of the scanner: 0 product match, 1 no match, 3 anchor-only hit.
set -euo pipefail
out=${1:?output directory}
threads=${2:-$(nproc)}
scope=${3:-all}
case $scope in all|lcg) ;; *) echo "invalid scope: $scope" >&2; exit 2 ;; esac
[[ $threads =~ ^[1-9][0-9]*$ ]] || { echo "invalid thread count" >&2; exit 2; }
root=$(cd "$(dirname "$0")/.." && pwd)
bin="$root/build/current/rsaref_md5_state_scan"
mkdir -p "$(dirname "$out")"
mkdir "$out" || { echo "refusing to overwrite output directory: $out" >&2; exit 2; }
out=$(cd "$out" && pwd)
hash_file() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1"; else shasum -a 256 "$1"; fi
}
trap 'code=$?; echo "driver_exit=$code ended=$(date -u +%FT%TZ)" >> "$out/RUN.txt"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
{
  echo "commit=$(git -C "$root" rev-parse HEAD)"
  echo "source_sha256=$(hash_file "$root/src/rsaref_md5_state_scan.cpp" | cut -d' ' -f1)"
  echo "host=$(hostname) threads=$threads scope=$scope started=$(date -u +%FT%TZ)"
  echo "build_command=make -C $root -B build/current/rsaref_md5_state_scan"
  echo "LCG coverage: seeds differing by multiples of 2^24 produce identical bytes; [0,2^24) covers [0,2^32)."
} > "$out/RUN.txt"
cp "$root/src/rsaref_md5_state_scan.cpp" "$out/scanner-source.cpp"
cp "$root/scripts/rsaref_state_sweep.sh" "$out/sweep-driver.sh"
cp "$root/Makefile" "$out/Makefile.snapshot"
"${CXX:-c++}" --version > "$out/compiler.txt" 2>&1
make -C "$root" -B build/current/rsaref_md5_state_scan > "$out/build.log" 2>&1
echo "binary_sha256=$(hash_file "$bin" | cut -d' ' -f1)" >> "$out/RUN.txt"
"$bin" self-test > "$out/self-test.log" 2>&1 || { echo "self-test FAILED" >> "$out/RUN.txt"; exit 1; }
echo "self-test passed" >> "$out/RUN.txt"
modes="raw-word raw-repeat-word md5-word md5-word-times64 md5-repeat-buffer ansi-rand ms-rand borland-rand"
if [ "$scope" = lcg ]; then modes="ansi-rand ms-rand borland-rand"; fi
for mode in $modes; do
  for wb in 1 2 3 4; do
    case $mode in ansi-rand|ms-rand|borland-rand) [ "$wb" = 4 ] || continue ;; esac
    end=$((1 << (8 * wb)))
    case $mode in ansi-rand|ms-rand|borland-rand) end=$((1 << 24)) ;; esac
    log="$out/$mode-w$wb.log"
    echo "$mode w$wb start=$(date -u +%FT%TZ)" >> "$out/RUN.txt"
    echo "command=$bin $mode 0 $end $threads $wb" >> "$out/RUN.txt"
    set +e
    "$bin" "$mode" 0 "$end" "$threads" "$wb" > "$log" 2>&1
    code=$?
    set -e
    echo "$mode w$wb exit=$code $(tail -1 "$log")" >> "$out/RUN.txt"
    case $code in
      0|3)
        echo "HIT in $mode w$wb (exit $code); remaining ranges not run; see $log" >> "$out/RUN.txt"
        exit "$code"
        ;;
      1)
        # An exit code alone does not establish complete negative coverage.
        variants=4
        case $mode in
          raw-word) variants=$((4 * (17 - wb))) ;;
          ansi-rand|ms-rand|borland-rand) variants=2 ;;
        esac
        expected=$((end * variants))
        if ! grep -q "^summary mode=$mode candidates=$end tested_state_transition_variants=$expected result=none " "$log"; then
          echo "FAILED: missing complete negative summary for $mode w$wb" >> "$out/RUN.txt"
          exit 2
        fi
        ;;
      *) echo "FAILED: scanner exit=$code for $mode w$wb" >> "$out/RUN.txt"; exit "$code" ;;
    esac
  done
done
echo "finished=$(date -u +%FT%TZ)" >> "$out/RUN.txt"
