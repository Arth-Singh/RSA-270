# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

An investigation into whether RSA-270 (270 decimal digits, 895 bits, the next unfactored number of the original 1991 RSA Factoring Challenge list) can be factored by exploiting how the challenge primes were generated, instead of by a generic number field sieve. The full research handoff is CONTEXT.md, the experiment ledger is docs/EXPERIMENTS.md, and provenance plus missing artifacts are in docs/SOURCES.md. Read CONTEXT.md before touching any scanner. AGENTS.md states the same working rules for non-Claude agents; keep the two files consistent.

Status: RSA-270 is not factored. No weak seed, generator state, or factor has been found. A scanner hit, a modeled output block, or a statistical anomaly is not progress toward a factorization. Success means exact `p * q == N` with nontrivial factors whose primality was checked independently of the scanner that produced them.

## Feasibility numbers that constrain every plan

- Status on 2026-09-09: FactorDB lists RSA-270 as composite with no known factor, and Wikipedia's RSA numbers page says it has not been factored. RSA-260 (862 bits) was factored on 2026-09-03 by Eric Lu; the method, hardware, and running time were not disclosed. Sources are in results/status-check-2026-09-09/STATUS.md.
- RSA-250 (829 bits) took about 2,700 core-years on 2.1 GHz Xeon Gold 6130 cores with CADO-NFS in 2020 (2,450 sieving, 250 matrix). The GNFS L[1/3] heuristic scales that to 16,000 to 24,000 such core-years for 895 bits, or roughly 6,000 to 12,000 core-years on 2026 cores. Third-party estimates for RSA-260 were 3,400 to 8,000 core-years, or about 25 GPU-years with a GPU port of CADO-NFS, which puts RSA-270 near 60 to 75 GPU-years.
- The Nebius VM below has 64 physical cores. A generic GNFS run there takes a century or more, and a second identical VM halves that. Even the GPU route needs on the order of a thousand GPUs for weeks. Never launch CADO-NFS on the full target "to use the CPU"; it cannot finish and it costs money every hour it runs.
- ECM, P-1, and P+1 cannot reach balanced 447/448-bit factors. Preliminary runs were already done; more is waste.
- A cheap route needs a real reduction of the generator's unknown entropy (CONTEXT.md, "Strategy and evidence boundaries"). None has been established. The shared facts about known factors (binary prefix 11, residue 2 mod 3, balanced bit lengths) fit many implementations and identify none.
- The compute is legitimately useful for bounded seed-hypothesis scans, Monte Carlo controls for the predecessor-gap model, and CADO polynomial selection only if a distributed sieving effort is actually being organized. Measured on the VM with 128 threads (results/vm-benchmark-2026-09-09/):

| Scanner | Throughput | Full 2^32 word space |
| --- | ---: | ---: |
| `rsaref_md5_state_scan md5-word` | 307 M state-transition variants/s (4 per seed) | about 56 s |
| `seed_scan raw32` | 726 M seeds/s | about 6 s |

A 2^40 seed family therefore takes minutes and 2^48 takes days to weeks. Anything larger needs an entropy argument first, not more machines.

## Layout

- `src/` holds the first-party scanners (C++20) and analysis scripts (Python 3, standard library only). README.md describes each file. `bsafe_rng_scan.cpp` is a superseded Boost draft kept as evidence; `bsafe1_rng_scan.cpp` is the implementation to use.
- `data/solved_decimal_challenge.tsv` is the only ground truth: 17 solved pairs, RSA-100 through RSA-260 in steps of ten. Its SHA-256 is recorded in results/handoff-2026-09-09/SHA256SUMS. `data/target.json` holds the exact RSA-270 value.
- `references/rsaref/raw_*` is the preserved RSAREF source. The conditional model follows `raw_r_random.c`, `raw_r_keygen.c`, `raw_prime-old.c` (trial division by {3, 5, 7, 11}, Fermat base 2, gcd(candidate-1, e) = 1), and `raw_nn.h` (digit width). `raw_prime.c` is the later Outerbridge variant with a 53-entry small-prime table; the scanners do not implement it, so never diff `Accepted()` or `old_rsaref_accepts()` against that file. The files without the `raw_` prefix are HTML-wrapped downloads; never compile or diff those.
- `results/<run-name>/` receives every experiment: exact command, source hash, complete output, and limitations. Nothing goes to /tmp.
- `vendor/cado-nfs` is a git submodule pinned to upstream commit 73ca6b6. `vendor/patches/cado-nfs-macos-build.patch` adds `-fexperimental-library` for Apple Clang and guards `sys.set_int_max_str_digits`; apply it on macOS only.
- `build/` is gitignored. Compile fresh; a copied binary may predate its source.

## Build and checks

```sh
make                                    # every scanner into build/current/
make build/current/rsaref_md5_state_scan  # one scanner
make smoke                              # MD1 known answer, tiny scanner run, Python reconstruction
make clean
```

macOS uses Homebrew gmp and openssl@3 (override `GMP_PREFIX` and `SSL_PREFIX` if installed elsewhere). Linux needs `libgmp-dev` and `libssl-dev`. The scanners that hash with MD5 link OpenSSL; the ones using big integers link `-lgmpxx -lgmp`; `md1_hamming_scan` and `md1_cli` must be linked with `src/md1.cpp`.

There is no test suite and no C++ linter. Run `ruff check src/` for the Python scripts. Correctness evidence is the MD1 known-answer table and the RC2 known-answer in CONTEXT.md, plus exact integer arithmetic on the dataset (`python3 src/rsaref_stream_reconstruct.py`, `python3 src/factor_forensics.py`).

CADO-NFS on Linux:

```sh
sudo apt-get install -y python3-flask            # cmake fails without it; cado-nfs.py also needs it at run time
cd vendor/cado-nfs && make -j"$(nproc)"          # cmake into build/$(hostname)
./cado-nfs.py 90377629292003121684002147101760858109247336549001090677693 -t "$(nproc)"   # c59 sanity check, 61 s on the VM
```

## Scanner conventions

| Binary | Arguments |
| --- | --- |
| `rsaref_md5_state_scan` | `MODE BEGIN END THREADS [WORD_BYTES]`; modes `raw-word raw-repeat-word md5-word md5-word-times64 md5-repeat-buffer ansi-rand ms-rand borland-rand`; `END` at most 2^32 |
| `seed_scan` | `MODE BEGIN END [THREADS] [OFFSET] [le\|be\|post-le\|post-be] [TARGET_HEX ...]`; always pass an explicit `OFFSET` and targets, because the default offset 77 and the built-in targets were never substantiated |
| `bsafe1_rng_scan` | `time-constant JULIAN_FIRST JULIAN_LAST HUNDREDTH_STEP CANDIDATE_STARTS [TARGET_TSV]` for the fixed-timestamp model; with no subcommand, `[STREAM_BYTES] [TARGET_TSV]` runs the parity/injected-byte scan followed by the seed-independent dll-radix16 exclusion check (`STREAM_BYTES` at least 128, default 2048). These are the only two forms. Both exit 0 when nothing matched and 1 on a match, the opposite of `rsaref_md5_state_scan` |
| `noise_time_scan` | `START END TIME_MODE SAMPLE_MODE THREADS [needed\|full] [BIT]`; RSA-100 only |
| `snfs_scan` | `N [MAX_LEADING] [MAX_MULTIPLIER] [KEEP]` |
| `md1_cli` | `STRING` |

`rsaref_md5_state_scan self-test` plants known seeds (raw-word with the postincrement carry case, and md5-word) through the real scan loop against synthetic anchors and a synthetic target. Run it after any change to the scanner; `make smoke` includes it.

`scripts/rsaref_state_sweep.sh OUT_DIR [THREADS]` runs the self-test and then the full word-seed space of every mode (1 to 4 byte words for the word families, 2^32 seeds for the LCG families) and writes `OUT_DIR/RUN.txt` with commit, source hash, per-family exit codes, and timings. About 30 minutes on the VM. Commit the output directory with the ledger entry in docs/EXPERIMENTS.md.

Exit codes of `rsaref_md5_state_scan`: 0 only when the reconstructed primes multiply to the target (`result=product-match`), 3 when a state passed every anchor but the product did not match (`result=state-only`, which would still mean the stream model is right and the RSA-270 offsets are wrong), 1 when nothing matched, 2 on bad arguments. Even exit 0 must be followed by independent multiplication and primality checks before anything is called a factorization.

Two RSAREF details every scanner must model explicitly (CONTEXT.md, "The postincrement trap"): the counter increment in r_random.c is `if (state[15-i]++) break;`, which carries when the old byte is zero rather than the new one, and the prime search uses the old Fermat-base-2 predicate, so the "previous prime" for interval inversion is the previous value accepted by that predicate, pseudoprimes included. `noise_time_scan` and `md1_hamming_scan` use only the ordinary increment. `seed_scan` accepts `post-le` and `post-be` (a bytewise subtract-one conjugation around ordinary addition that reproduces the literal transition), but every recorded `seed_scan` run used `le` or `be` with the pre-patch binary, no post-transition run is recorded, and `seed_scan` has no self-test for that path. None of the recorded negative results cover the literal transition.

Every scan needs a synthetic positive control (a known seed planted through the same model) before its negative result is recorded in docs/EXPERIMENTS.md.

## Compute: Nebius VM apricot-cougar-instance-3

```sh
ssh -i ~/.ssh/share_nebius_ed25519 arth@89.169.111.225
```

128 vCPU (AMD EPYC Genoa, 64 cores with 2 threads each), 503 GiB RAM, 1.28 TB disk, x86_64, Ubuntu 24.04, no GPU, non-preemptible. It is billed while running, so stop it from the Nebius console whenever nothing is executing. gcc, cmake, libgmp-dev, libhwloc-dev, libssl-dev, gmp-ecm, python3-flask, and tmux were installed on 2026-09-09, and CADO-NFS is built under `~/RSA-270/vendor/cado-nfs/build/`. The repository is cloned at `~/RSA-270`; pull before building. `~/juniper` belongs to another project; leave it alone. Run anything longer than a minute inside tmux with output under `~/logs/`, then copy the output into `results/` and commit it.

## Git and agents

`main` tracks github.com/Arth-Singh/RSA-270. Parallel agents work in worktrees: `git worktree add ../RSA-270-wt/<task> -b <task>`. The CADO submodule only needs `git submodule update --init` when CADO is actually used. Commit a results directory together with the source revision that produced it. Never commit binaries, CADO build trees, or `references/recovered-temp/`.
