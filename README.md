# RSA-270 research workspace

This is the complete project copied from /Users/arth_rogerthat/rsa-270 on 2026-09-09, with a durable research handoff added. The copy was approximately 341 MB before handoff artifacts. Source, data, references, executables, and the CADO-NFS checkout were copied and checksum-compared.

**Status: RSA-270 is not factored by this project.** This is an experimental historical-generator investigation, not a working RSA-270 cracker.

Start with [CONTEXT.md](CONTEXT.md). Then read [experiment coverage and caveats](docs/EXPERIMENTS.md) and [sources and artifact recovery](docs/SOURCES.md). The next session should work in this directory.

## Quick restart

Suggested opening prompt:

> Read AGENTS.md, CONTEXT.md, docs/EXPERIMENTS.md, and docs/SOURCES.md. Continue the RSA-270 investigation from the documented state. First validate the current scanner and choose one bounded experiment, preserving exact commands and results. Do not repeat completed scans without a reason.

The original one-day deadline has expired. Set a new scope or budget if desired.

## Files

| Path | Purpose |
| --- | --- |
| src/md1.cpp, src/md1.hpp, src/md1_cli.cpp | Recovered RSA-MD1 implementation and command-line digest tool |
| src/bsafe1_rng_scan.cpp | Recovered 1991 Lotus BSAFE RC2 RNG, parity/time hypotheses, and sparse-candidate checks |
| src/bsafe_rng_scan.cpp | Older incomplete Boost-based draft; not the preferred implementation |
| src/md1_hamming_scan.cpp | Hypothetical MD1 additive-RNG binary-sample seed search |
| src/seed_scan.cpp | Older fast MD5 seed-family scanner; requires explicit trustworthy target/offset |
| src/noise_time_scan.cpp | Older MD5 time-plus-noise model restricted to RSA-100 |
| src/rsaref_stream_reconstruct.py | Exact conditional inversion of old RSAREF candidate ranges and stream masks |
| src/rsaref_md5_state_scan.cpp | Newer conditional MD5 state scanner with pre/post-increment variants and RSA-270 reconstruction |
| src/factor_forensics.py, src/rng_stats.cpp | Statistical checks of known-factor interiors |
| src/snfs_scan.cpp | Search for favorable small-coefficient algebraic representations |
| data/solved_decimal_challenge.tsv | 17 solved decimal-list pairs, RSA-100 through RSA-260 in steps of ten |
| data/target.json | Exact RSA-270 target and handoff status |
| references/rsaref/ | Preserved historical RSAREF source, including HTML-wrapped and raw versions |
| references/recovered-temp/netscape-1.0/ | Surviving Netscape 1.0 research download; no demonstrated RSA DSP connection |
| results/handoff-2026-09-09/ | Regenerated masks, verification notes, and file hashes |
| vendor/cado-nfs/ | CADO-NFS git submodule at upstream commit 73ca6b6; macOS build patch in vendor/patches/ |

## Local build examples

The original machine was an Apple M4 Mac with 10 logical CPUs and 16 GB RAM. `make` builds every scanner into build/current/ on macOS (Homebrew gmp and openssl@3) and Linux (libgmp-dev, libssl-dev); `make smoke` runs the known-answer checks. The manual commands below show the exact flags the Makefile uses.

Build fresh outputs in a separate directory:

```sh
cd /Users/arth_rogerthat/Downloads/RSA-270
mkdir -p build/current

clang++ -O3 -std=c++20 \
  src/md1.cpp src/md1_cli.cpp \
  -o build/current/md1_cli

clang++ -O3 -std=c++20 \
  -I/opt/homebrew/opt/gmp/include \
  -I/opt/homebrew/opt/openssl@3/include \
  -L/opt/homebrew/opt/gmp/lib \
  -L/opt/homebrew/opt/openssl@3/lib \
  src/rsaref_md5_state_scan.cpp \
  -lgmpxx -lgmp -lcrypto -pthread \
  -o build/current/rsaref_md5_state_scan

./build/current/md1_cli abc
./build/current/rsaref_md5_state_scan md5-word 0 1000 2 4
```

The MD1 result must be `74594a0a73b03503efe62e574dbaf816`. The tiny scanner run is only a smoke test, not a useful search. The newer scanner exits 1 when no modeled state matches (the normal outcome), 3 when a state passes every anchor but the reconstructed primes do not multiply to RSA-270, and 0 only on an exact product match. `rsaref_md5_state_scan self-test` plants known seeds through the real search path, including the postincrement carry case, and must print `self-test PASSED`.

Other GMP programs use the same GMP include/library flags. The older MD5 programs also need OpenSSL. md1_hamming_scan.cpp must be linked with md1.cpp. The forensic Python scripts use the standard library.

## Portability

Copied CADO CMake caches, virtual-environment entry points, and generated scripts can retain absolute paths to the original workspace. Reconfigure/rebuild them in this directory if needed. Do not reset its local patches or assume an existing binary was rebuilt after the last source edit.

The project is the Git repository github.com/Arth-Singh/RSA-270 (since 2026-09-09). vendor/cado-nfs is a submodule; run `git submodule update --init` after cloning and apply vendor/patches/cado-nfs-macos-build.patch on macOS. Compiled binaries under build/ and the recovered Netscape distribution under references/recovered-temp/ are not tracked.

