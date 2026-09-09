#!/usr/bin/env bash
set -euo pipefail
cd "$HOME/RSA-270"
out=results/noise-hamming-vm-2026-09-09
mkdir "$out"
mkdir -p build/current
exec > "$out/RUN.log" 2>&1
trap 'code=$?; echo "driver_exit=$code ended=$(date -u +%FT%TZ)"' EXIT
echo "started=$(date -u +%FT%TZ)"
git rev-parse HEAD
sha256sum src/noise_hamming_scan.cpp src/md1.cpp src/md1.hpp data/solved_decimal_challenge.tsv
cp src/noise_hamming_scan.cpp src/md1.cpp src/md1.hpp "$out/"
cp results/compute-continuation-2026-09-09/run_noise_vm.sh "$out/"
g++ --version
set -x
g++ -O3 -std=c++20 -Wall -Wextra -Wno-deprecated-declarations src/md1.cpp src/noise_hamming_scan.cpp -lgmpxx -lgmp -lcrypto -pthread -o build/current/noise_hamming_scan
sha256sum build/current/noise_hamming_scan
build/current/noise_hamming_scan self-test > "$out/self-test.log" 2>&1
timeout 60 build/current/noise_hamming_scan scan --digest md1 --end 65536 --threads 128 --stream-bytes 16384 > "$out/benchmark.log" 2>&1
timeout 600 build/current/noise_hamming_scan scan --digest both --threads 128 --stream-bytes 16384 > "$out/full-scan.log" 2>&1
grep '^summary states=2081200 .*fingerprint_hits=0 interior_pattern_hits=0 candidate_interval_hits=0 fresh_candidate_hits=0 ' "$out/full-scan.log"
echo "complete_negative_range=yes finished=$(date -u +%FT%TZ)"
