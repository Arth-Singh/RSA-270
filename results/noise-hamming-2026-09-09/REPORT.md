# Bounded additive-noise scanner

Implementation date: 2026-09-09. Branch: `codex/noise-hamming-20260909`.

## Outcome

A separate `src/noise_hamming_scan.cpp` is ready for a bounded VM run. Its local controls pass. The local pilot found no fingerprint, interior-pattern, or candidate-interval hits. No factorization or recovered challenge-generator state is claimed.

No existing scanner, sweep driver, Makefile, or project documentation was modified. The companion `scripts/test_noise_hamming_scan.py` verifies the MD5 paths with Python `hashlib`.

## Exact coverage and qualifications

The default full family contains 2,081,200 configured streams, including duplicates if different hypotheses yield the same initial state:

- Two digests: MD5 and the existing recovered RSA-MD1 implementation.
- Fifty binary-sample encodings, identical to `md1_hamming_scan.cpp`: every single comparator-bit location in one-, two-, and three-byte memory images; byte 00/ff; and ASCII 0/1.
- Seventeen sample counts: 34, 50, 64, 83, 86, 100, 128, 150, 165, 166, 256, 270, 330, 512, 768, 1000, and 1024. Every weight from zero through the sample count is included. These give 5,203 count/weight pairs.
- Big- and little-endian additive state arithmetic, each paired with a counter in the same endian order.
- Prefix-increment and literal postfix-increment loops. In both cases the old state is hashed before the increment, as in the preserved source. The option names refer to the C increment expression, not output timing.

The initial state is `(n-w)*Hash(zero) + w*Hash(one) mod 2^128`, starting from zero. MD5 with big-endian addition and the postfix loop matches the state operations in `references/rsaref/raw_r_random.c`. This does **not** establish that RSA DSP used those operations.

The preserved `R_GenerateBytes` refuses output until at least 256 seed bytes have been supplied. This experiment deliberately bypasses that gate. Cases with `sample_count * sample_encoding_length < 256` represent hypothetical initialized states or a changed threshold, not that source's complete seeding call sequence. MD1 substitution, little-endian arithmetic, the prefix loop, and all candidate layouts are also explicit hypotheses. The runtime configuration prints `seed_byte_gate=not-enforced`.

Every possible complete candidate-buffer start inside the declared stream is considered independently for every known factor. There is no shared RSA-100 through RSA-260 offset or ordering assumption. The default bound is exactly 2,048 bytes, without hidden lookahead beyond it. A larger `--stream-bytes` bound is available for a separately recorded experiment; the parent agent owns any VM benchmark and full-run decision.

Integer decoding covers both byte orders, byte lanes, both two-byte lane permutations, and all six three-byte lane permutations. For a final partial lane, the permutation is restricted to existing byte positions in its stated order. This is a bijective compact-tail hypothesis; the legacy fallback could duplicate a byte. Buffer lengths include minimal factor bytes and the per-factor buffer allocation implied by 16-, 24-, or 32-bit RSAREF digits, using the associated modulus length. Duplicate lengths are tested once. This is not the signed 24-bit arithmetic representation described in the recovered DSP paper.

Candidate construction truncates to the factor bit length, optionally forces one or two top bits, and forces odd parity. The exact preceding **accepted** value is found with the old predicate: no small factor 3/5/7/11, base-2 Fermat acceptance, and exponent-3 compatibility. The fixed interior mask is derived from the complete interval `[previous+1, factor]`, so carry across a byte boundary does not invalidate an anchor. No ordinary-prime predecessor is substituted. First-in-range wraparound targets are explicitly rejected during setup; none of the 34 corpus targets has that case.

## Meaning of results

- `fingerprint_hits` counts eight-byte anchor matches, before full-mask verification.
- `PATTERN_ONLY` requires every fixed interior bit to match but no decoded candidate to lie in the exact accepted interval.
- `CANDIDATE_INTERVAL` verifies the complete decoded candidate lies in `(previous_accepted, factor]`. It therefore maps to the known factor under the specified upward-search model. `fresh_candidate=yes` additionally means the formatted candidate equals that factor directly, which also covers fresh candidate sampling.
- Every hit is tagged `factorization=no`. Exit codes are 0 for no full interior patterns, 3 for pattern-only results, 4 for at least one candidate interval, and 2 for input/setup/control errors. No scanner exit is a factorization success signal.

## Validation

`controls.txt` records:

```text
HASHLIB_FIXTURES PASSED independent_checks=436
SELF_TEST PASSED seeded_packing_controls=1008 enumeration_controls=8 digest_kats=6 counter_carries=6 classification_controls=2
```

The 436 independent checks cover all 50 encodings, every sample count, boundary weights, both endians, both counter loops, explicit carry states, and partial digest output lengths. Full commands, actual output, expected output, return codes, and stderr are retained in `hashlib-fixtures.json`. The C++ controls additionally discover planted factors from seeded streams across packing paths and padded/partial buffers, exercise the actual family-enumeration path for all eight digest/arithmetic/counter combinations, verify fresh-versus-pattern-only classification, and enforce the stream boundary. The old predicate control accepts the composite base-2 pseudoprime 8321, ensuring a modern primality predicate cannot silently replace it. MD1 uses the four existing recovered known-answer vectors; it is not treated as MD2.

The final build used C++20, `-O3 -Wall -Wextra -Wno-deprecated-declarations`, GMP, OpenSSL, and pthreads. It completed with no diagnostics. `pilot-manifest.json` records the exact macOS build command, all final validation/pilot commands, full stdout/stderr, exit codes, platform, and source/data/binary hashes.

## Local pilot

Both pilots used two threads and 2,048 bytes per stream against all 34 stored factors and 1,404 prepared patterns:

| Digest | Configured state interval tested | Streams | Scanner time | Throughput | Hits |
| --- | --- | ---: | ---: | ---: | ---: |
| MD5 | [0, 16384) in MD5-only numbering | 16,384 | 0.0863162 s | 189,814 streams/s | 0 |
| MD1 | [0, 4096) in MD1-only numbering | 4,096 | 1.49923 s | 2,732.07 streams/s | 0 |

The MD5 sample is short and its rate is approximate. MD1 dominates projected runtime. Linear extrapolation gives about 381 seconds for the complete MD1 family on these two local cores at 2,048 bytes, or about 3,047 seconds at 16,384 bytes. Ideal 128-way scaling would be about 48 seconds at the latter bound, but hardware differences, physical-core count, threading, and scheduling require a VM benchmark. These are projections, not completed-run results.

The pilots test only their recorded intervals; they do not exclude the full family. In `--digest both` numbering, the MD5 family precedes MD1; each digest-only run numbers its own family from zero. `--begin` is inclusive and `--end` exclusive.

## Reproduction

Linux build from the worktree root:

```sh
c++ -O3 -std=c++20 -Wall -Wextra -Wno-deprecated-declarations src/noise_hamming_scan.cpp src/md1.cpp -lgmpxx -lgmp -lcrypto -pthread -o noise_hamming_scan
./noise_hamming_scan self-test
python3 scripts/test_noise_hamming_scan.py ./noise_hamming_scan --output hashlib-fixtures.json
./noise_hamming_scan scan --digest md1 --begin 0 --end 4096 --threads 2 --stream-bytes 2048
```

The root-level result directory preserves the generated local executable for reproducibility, but `.gitignore` excludes it from commits. All text reports, fixture outputs, and manifests are retained. Final scanner SHA-256: `48188e1f0e5b39270c7de1f537e3c4063b0a4ee8e3c2f534d34c8db832bd0c6a`.
