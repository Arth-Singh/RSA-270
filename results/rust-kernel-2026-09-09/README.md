# Safe Rust MD1 versus preserved production C++

The safe Rust prototype matched every checked output. This local run measured
a small Rust median whole-process throughput advantage: **0.88%** for fresh
synthetic 16-byte inputs and **0.74%** for the postfix-counter workload. The
timing ranges overlap, and individual Rust runs were sometimes slower. These
measurements show near parity for these implementations and toolchains; they do
not establish a substantial advantage or a universal language-speed result.

| Workload | Digests per process | C++ median seconds (min–max) | Rust median seconds (min–max) | Rust/C++ median throughput ratio |
| --- | ---: | ---: | ---: | ---: |
| Fresh synthetic full-state input | 65,536 | 0.372116 (0.371339–0.372837) | 0.368856 (0.367780–0.374388) | 1.00884 |
| Exact postfix counter after each digest | 65,536 | 0.371360 (0.370598–0.384909) | 0.368620 (0.367798–0.412168) | 1.00743 |

Six alternating-order timing pairs were collected per workload: 24 timed child
processes in total. All retained intervals exceeded 0.367 seconds. Median
absolute deviations were 0.000704 s (C++) and 0.001012 s (Rust) for fresh inputs,
and 0.000625 s (C++) and 0.000629 s (Rust) for postfix inputs. Median **paired**
C++/Rust time ratios were 1.00877 and 1.00592, respectively. No outlier was
removed; the 0.412168-second Rust postfix run is retained in all evidence.

## What was compared

- `md1_rust.rs` is a safe, general-input translation of the existing Md1 context
  and digest API, with `#![forbid(unsafe_code)]`. It retains padding, checksum,
  finalization checks, all 18 sweeps, and all 48 byte updates per sweep. There is
  no hand-written fixed-length specialization.
- `md1_cpp_bench.cpp` calls the unchanged production `src/md1.cpp` through
  `src/md1.hpp`. Both executable builds use whole-program LTO and native CPU
  optimization. Each optimizer can make its own inlining decisions.
- Fresh-input mode constructs all 16 input bytes on every iteration using the
  same deterministic SplitMix64 operations and explicit little-endian encoding
  in both programs. This is synthetic workload generation, not an assertion
  about entropy or historical seeding.
- Postfix mode starts from the same generated state, clears its final two
  bytes, hashes the state, then applies the literal preserved RSAREF postfix
  transition: increment byte 15 first and carry only when the old byte was
  zero. This combines the recovered MD1 with a counter for benchmarking; the
  preserved historical RSAREF source itself uses MD5.
- Every digest byte is consumed by an observable 64-bit rolling fold, and the
  final state and generator state are printed. All corresponding warmup,
  calibration, and timed-process outputs matched exactly. Nothing searches
  for a seed, compares a challenge anchor, or factors a number.

## Correctness and timing method

Validation passed all four preserved known-answer tests, 128 separately
generated binary 16-byte test inputs, seven general-input boundary lengths,
and 21 carry vectors with before/after digest comparisons: **181 digest cases**
and **21 independently checked counter transitions**. Python supplied the
counter reference, including all-zero propagation and the old-`ff` stopping
case. Full inputs and both implementations' outputs are retained in
`run/digest-vectors.tsv` and `run/carry-vectors.tsv`.

One Python parent measured each subprocess with two calls to its own
`perf_counter_ns`. No clock origins from different processes were compared.
The children were sequential, with only one active benchmark process. Each
workload was warmed up, then calibrated using identical counts for both
implementations until both lasted at least 0.35 seconds. Its count was held
fixed for all six timing pairs; the leading implementation alternated and was
reversed for the other workload.

The machine was an Apple M4, model Mac16,12, arm64, 10 physical/logical cores,
16 GiB memory. C++ used Apple clang 17.0.0; Rust used rustc 1.94.0 with LLVM
21.1.8. Thus compiler generation is a confound as well as language and source
form. No CPU affinity or frequency pinning was applied. Whole-process intervals
include startup, input generation, folding, and output, so these figures must
not be presented as an isolated compression-kernel speed difference.

No scanner, SMT-solving workload, GNFS phase, or overall factoring pipeline
was timed. A port of those systems cannot be justified by this result alone.

## Reproduction and evidence

The recorded build options were:

```text
C++:  -O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic -mcpu=native -flto=full -Isrc
Rust: --edition=2021 -C opt-level=3 -C target-cpu=native -C lto=fat -C codegen-units=1 -C panic=unwind -D warnings
```

The original invocation from the worktree root was:

```sh
mkdir -p results/rust-kernel-2026-09-09/run
python3 results/rust-kernel-2026-09-09/run_experiment.py \
  > results/rust-kernel-2026-09-09/run/driver.log 2>&1
```

The runner refuses to overwrite a completed result. For a fresh repeat that
preserves the recorded run, copy the source files and runner to a new
sibling directory under `results/`, then run that copy:

```sh
mkdir -p results/rust-kernel-repeat
cp results/rust-kernel-2026-09-09/md1_rust.rs \
   results/rust-kernel-2026-09-09/sbox.rs \
   results/rust-kernel-2026-09-09/md1_cpp_bench.cpp \
   results/rust-kernel-2026-09-09/run_experiment.py \
   results/rust-kernel-repeat/
python3 results/rust-kernel-repeat/run_experiment.py \
  > results/rust-kernel-repeat/driver.log 2>&1
```

`run/subprocesses.jsonl` preserves every exact command, stdin where applicable,
complete stdout/stderr, exit code, and parent-measured duration, including all
builds, metadata reads, validation, warmup, calibration, and retained timings.
`run/summary.json` records compiler versions, flags, hardware, source/input and
binary SHA-256 hashes, statistics, and limitations. `run/timings.tsv` contains
every retained timing. `artifact-manifest.json` hashes the source and evidence
files. Binaries reside in ignored `build/rust-kernel/`; rebuild from source.

This directory was produced in branch `codex/rust-kernel-20260909`, based on
`52f5860da06aa740d7fd24200d49e0a46a9051a0`. No production source was edited,
no cloud job was launched, and no result was pushed or published.

The runner's `base_commit` field is an archival baseline label, not a runtime
Git query. It is accurate for this recorded run. Future runs on another
checkout must record their actual `git rev-parse HEAD` separately and must not
interpret the hard-coded label as verification of that checkout's revision.
