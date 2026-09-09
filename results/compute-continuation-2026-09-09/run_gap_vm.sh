#!/usr/bin/env bash
set -euo pipefail
cd "$HOME/RSA-270"
out=results/gap-study-20260909
mkdir -p "$out" build/current
exec > "$out/vm-run.log" 2>&1
trap 'code=$?; echo "driver_exit=$code ended=$(date -u +%FT%TZ)"' EXIT
echo "started=$(date -u +%FT%TZ)"
git rev-parse HEAD
sha256sum "$out/gap_monte_carlo.cpp" data/solved_decimal_challenge.tsv
g++ --version
set -x
g++ -O3 -std=c++20 -Wall -Wextra -Wpedantic "$out/gap_monte_carlo.cpp" -lgmpxx -lgmp -pthread -o build/current/gap_monte_carlo
sha256sum build/current/gap_monte_carlo
build/current/gap_monte_carlo --self-test
timeout 600 build/current/gap_monte_carlo --replicates 131072 --threads 128 --seed 20260910 --output-prefix "$out/vm"
