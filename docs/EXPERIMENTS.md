# Experiment ledger and known limitations

This ledger records the available September 3, 2026 research history. "Reported" means the result was present in the prior session/agent messages, but a complete raw log was not retained in the new workspace. "Observed" means a completion result was visible in the preserved conversation. "Rechecked" means a local check was run during the September 9 handoff.

No entry establishes a factorization of RSA-270.

## Coverage summary

| Route | Coverage/result | Evidence and limitation |
| --- | --- | --- |
| Small/special factor methods | Preliminary P-1, P+1, and ECM returned no factor | Reported. Bounds, curve counts, seeds, and full output are not retained. Do not claim comprehensive exclusion. |
| GNFS feasibility / CADO | CADO compiled; polynomial/SNFS reconnaissance found no useful shortcut | Reported. No completed RSA-270 GNFS factorization, relation collection, matrix, or square-root result. |
| Small-coefficient SNFS search | No favorable exploitable representation reported | src/snfs_scan.cpp survives; exact historical invocation/output absent. |
| 1982 hP+1-style prime-generation patent | RSA-100's p-1 factor structure contradicted the investigated large-factor requirement | Reported by factor_hunt. Patent ID and exact supporting factorization are not retained; reacquire before relying on a detailed claim. |
| Lotus BSAFE binary recovery | RC2 RNG and RSA-MD1 were recovered and tested against x86 execution | Source and KATs survive; DLL/emulator files disappeared from /tmp. |
| Lotus sparse candidate skeleton | All 34 factors reported impossible under the recovered interior-zero-limb pattern | Source-exact claim for that DLL, not an exclusion of all BSAFE/RSA generators; predecessor-acceptance bound needs careful audit if generalized. |
| BSAFE parity seed scan | 65,536 parity states per tested binary injection value, output offsets/layouts, no anchors or matches | src/bsafe1_rng_scan.cpp defaults to injected values 01,80,ff and 2048 output starts. Historical result reported; exact command log absent. |
| Lotus fixed-timestamp model | 33,091,200 timestamps; initial start 0 and later starts 0..63; no anchor/full matches | Reported range 1990-03-01 through 1991-03-18; exact Julian endpoints/step command absent. Recovered RNG but speculative fixed-time/direct-stream prime model. |
| MD1 binary-sample additive model | 520,300 initial states; 2048 output starts; no anchors/full matches | Reported. Uses an invented MD1 replacement for RSAREF's MD5, not recovered DSP code. |
| Raw MD5 low-32-bit states | Entire 2^32 space with twelve leading zero bytes; no hit to three explicit block targets | Observed completion. Endian runs duplicate the same set when offset=0. Does not cover arbitrary 32-bit seed placement or full 128-bit states. |
| MD5 timestamp families | 18 combinations of seed family, word byte order, and fixed stream anchor offset; all completed with no match | Observed. Old ordinary-counter model only; details below. |
| LCG-filled seed buffers | 18 jobs launched for ANSI/MS/Borland families; final polling interrupted | Outcomes unknown. Do not label completed or negative. Endian duplicates exist because these LCG families ignore seed byte encoding. |
| New RSAREF state scanner | September 9: all five word families over widths 1..4 and all three distinct LCG seed spaces completed with no match | Independent controls and complete logs retained. LCG streams repeat every 2^24 seeds; original interrupted MS run is not counted. See continuation record below. |
| Factor interior statistics | No robust finding after multiple-comparison adjustments | Reported exact summaries below. |
| Conditional stream reconstruction | 512 order variants, three digit widths; no equal/complement full-block repeats | Rechecked at handoff; masks saved. This does not prove the stream model historically correct. |
| Predecessor-gap model | September 9: 131,072 direct datasets/model; 10 upward-search scores at or below observed, plus-one p=0.000083923 | Exact old predicate, matched ranges/bit lengths, cyclic endpoints; exploratory statistic with no global multiple-testing or attribution guarantee. |

## MD1 additive binary-sample model

The September 3 MD1-only result below remains historical. A separate September
9 extension completed both MD5 and MD1, literal postfix and ordinary counters,
two arithmetic orders, and a 16,384-byte stream bound. Its 2,081,200 configured
streams produced zero fingerprint hits across 34,083,812,400 windows. See
`results/noise-hamming-vm-2026-09-09/` for exact coverage and controls; neither
experiment establishes the historical DSP seeding procedure.

src/md1_hamming_scan.cpp tests this hypothetical model:

```text
state <- state + MD1(seed_block) mod 2^128
output <- MD1(state)
state <- state + 1 mod 2^128
```

If each binary comparator sample is hashed as a separate seed update, order is lost. For n samples, only n+1 Hamming-weight states remain. This is the motivation for the finite seed search; it is not evidence that the DSP sampled this way.

Coverage in source:

- 50 sample encodings: each bit position in 1-, 2-, and 3-byte images, plus 00/ff and ASCII 0/1.
- Both integer byte orders.
- Sample counts: 34, 50, 64, 83, 86, 100, 128, 150, 165, 166, 256, 270, 330, 512, 768, 1000, 1024.
- Every Hamming weight.
- 2048 stream-start positions in the reported run.
- Byte/16-bit/24-bit lane permutations, factor byte orders, and top-bit forcing variants.
- All 34 stored factors.

Reported result: 520,300 states, zero anchor hits, zero full matches.

Caveats: ordinary increment, speculative digest and sampling scheme, finite sample counts/offsets, and a previous-ordinary-prime interval in the final match check. Because reported anchor hits were zero, the last interval check did not create the recorded zero-anchor result. Do not generalize to all MD1 seed mechanisms.

## Explicit raw32 MD5 sweep

The observed runs used the older build/seed_scan:

```sh
./build/seed_scan raw32 0 4294967296 5 0 be \
  1adcee7d7d9f02f0c4469c32a5307426 \
  fb1a4868708b1c03df4af162d3ae871a \
  c8cb3dbe4d62c32a39a49136d61fa921

./build/seed_scan raw32 0 4294967296 5 0 le \
  1adcee7d7d9f02f0c4469c32a5307426 \
  fb1a4868708b1c03df4af162d3ae871a \
  c8cb3dbe4d62c32a39a49136d61fa921
```

Observed results:

```text
be: result=none elapsed=113.379s nominal_rate=37.882 M/s
le: result=none elapsed=113.898s nominal_rate=37.709 M/s
```

These ran concurrently. They test MD5 of states 00*12 || word32 at any chosen matching block, with offset 0. They are not an exhaustive search of MD5(time)-derived states. Both byte orders enumerate the same state set, so do not add their counts as distinct entropy coverage.

## MD5 timestamp-family batch

Common half-open timestamp range:

```text
[567993600, 669513600)
1988-01-01T00:00:00Z through, but not including, 1991-03-21T00:00:00Z
101,520,000 candidate seconds per invocation
```

Three explicit target/offset pairs:

| Digit-width hypothesis | Offset | Target MD5 output |
| --- | ---: | --- |
| 16 | 34 | 1adcee7d7d9f02f0c4469c32a5307426 |
| 24 | 37 | fb1a4868708b1c03df4af162d3ae871a |
| 32 | 43 | c8cb3dbe4d62c32a39a49136d61fa921 |

For each pair, both le and be were run for each of:

- md5-word: state = MD5(encoded 32-bit timestamp).
- md5-word-times64: state = 64 * MD5(encoded word), modulo 2^128.
- md5-repeat-word: state = MD5(256-byte buffer consisting of 64 repetitions of the encoded word).

Invocation pattern:

```text
./build/seed_scan MODE 567993600 669513600 1 OFFSET le|be TARGET_HEX
```

All 18 completed with result=none. Individual jobs took roughly 43–47 seconds for the short-word families and 83 seconds for the repeated buffer, while the batch ran concurrently.

**Critical limitation:** these completions predate the postfix-transition patch. They use ordinary state + offset, assume a continuous stream from RSA-100 with no other consumption, and do not search arbitrary initial output skips or per-prime reseeding.

The patched seed_scan source accepts post-le/post-be, but the copied build/seed_scan was built at 20:18:28 and the source was changed at 20:25:32. No post-transition rerun or rebuild was recorded.

## Interrupted LCG-buffer batch

The same range/offsets were launched for ansi-rand, ms-rand, and borland-rand, with both endian labels. Each creates 256 bytes using the low eight bits of a 15-bit high-word rand() result, then hashes the buffer with MD5. The LCG constants are in source.

All jobs were still running at the last completed poll. A subsequent poll was aborted. No final outputs were preserved. On September 9 no relevant scanner process was observed, but absence of a process does not reveal whether a job completed, failed, or was terminated.

Old PTY session IDs, solely for historical trace lookup:

```text
ANSI:   62084 24916 88439 68198 24122 72569
MS:     6776 59423 60604 47025 92693 88074
Borland:20827 96762 87002 47055 6444 65133
```

Do not poll these IDs in a new session or assume they identify current processes. No raw logs for them are included.

## Statistical forensics reported by structure_attack

src/factor_forensics.py operates on factor interiors to avoid forced high bits and prime-search changes at the low end. It includes transition, lag, substring, concatenation-order, and Berlekamp–Massey-style probes.

Reported September 3 results:

- All 34 factors have second-most-significant bit 1 and residue 2 modulo 3.
- Best 24-bit/bit-transition Bonferroni-adjusted p-value: 0.504.
- Cross-factor lag comparisons: 3,504,732; best raw p approximately 1.28e-6, adjusted to 1.
- Longest copied/complemented substring: 29 bits; multiplicity-adjusted bound 1.
- Concatenated autocorrelation: 57,344 comparisons; best exact raw p 3.49e-6, adjusted p 0.200.
- Lag-24 p approximately 0.270.
- RSA-260 Berlekamp–Massey minimum reported linear complexities 183/181 for 368-bit interiors.
- Conditional RSAREF masked autocorrelation: 3 widths * 512 orders * 512 lags = 786,432 tests; best exact raw p 7.3359e-6, adjusted p 1.

These are exploratory and depend on null models and the finite family of tests. They provide no validated shortcut.

The handoff did not rerun the full statistical suite. It did rerun the stream block inventory:

```text
w16: 440832 block-pair comparisons, 0 equal, 0 complement
w24: 484352 block-pair comparisons, 0 equal, 0 complement
w32: 253952 block-pair comparisons, 0 equal, 0 complement
```

Counts include repeated comparisons across the 512 order hypotheses. They are not independent samples.

## Predecessor-gap follow-up

The exact old RSAREF loop maps a uniform raw residue to accepted primes with probability proportional to the preceding accepted-prime gap. After parity folding, a gap g gives g/2 odd starting candidates, each with two raw preimages.

The feasibility agent reported no discrepancy between the relevant previous old-predicate-accepted values and the previous genuine primes congruent to 2 modulo 3 in the tested corpus. Approximate normalized total gap 41.26 was smaller than the length-biased expectation 68. A matched Monte Carlo test was requested but not completed in available records.

FactorLab's p=5+6*start followed by +6 has the same basic gap weighting if start is uniform over a suitable interval. Whether it otherwise matches candidate formatting, factor sizes, or noise use is unestablished.

## September 9 handoff checks

- Full original project copied to Downloads/RSA-270 and compared with rsync checksum dry run; no differences before handoff additions.
- All 17 stored p*q products checked exactly.
- All 34 factors checked for prefix 11, residue 2 modulo 3, and balanced bit allocation.
- Four MD1 known-answer vectors reproduced with copied executable.
- Exact conditional predecessor intervals, block inventories, and 24-bit masks regenerated from preserved Python source.
- Dataset SHA-256: 19d713f80b853a85210215498debeaf98ccf18f06c32761d7ddd1be34f27ca99.
- No broad seed sweep, GNFS job, or new factoring attack was started.
- Missing /tmp artifacts were explicitly recorded rather than represented as saved.

## September 9 VM continuation

Full commands, source and executable provenance, per-family output, LCG
symmetry proof, model decisions, and limitations are in
`results/compute-continuation-2026-09-09/README.md`.

- `results/rsaref-state-sweep-2026-09-09/`: 20 complete word-family negatives
  and one complete but redundant full-32-bit ANSI-LCG negative. The following
  MS-LCG job was intentionally stopped; no completed result is assigned to it.
- `results/rsaref-lcg-distinct-2026-09-09/`: fresh build and all three LCG
  ranges `[0,16777216)`, 128 threads, no matches. This covers every stream in
  their nominal 32-bit seed spaces by exact recurrence symmetry.
- `results/scanner-validation-2026-09-09/`: independent expected vectors,
  carry cases, all three width paths, all real anchors, and false-product
  controls. Synthetic hits in those logs are not RSA-270 factors.
- `results/gap-study-20260909/`: direct pilot and VM Monte Carlo controls,
  independent Python audit, complete sample tables, and exact confidence
  intervals. The observed gap sum is unusually small under uniform-start
  upward search; that weakens this conditional construction model without
  identifying an alternative implementation or recovering a seed.
- `results/generator-evidence-2026-09-09/`: restored primary DSP paper and
  generation announcement mirrors. No firmware, entropy-conditioning code,
  or challenge-specific candidate loop was recovered.
- `results/noise-hamming-vm-2026-09-09/`: complete 16 KiB additive-noise scan,
  with 2,081,200 configured streams and zero fingerprint/interior/interval
  hits. This relaxes global stream alignment and covers fresh or exact-upward
  candidate construction, but retains finite encodings/counts/offsets and
  unproved additive seeding and packing hypotheses.

All eight scanner seed families still depend on the documented continuous
stream, formatting, and generator hypotheses. Their completed ranges do not
exclude other seed mechanisms, skips, reseeding, or prime-generation loops.

## September 9 structural pivot

- `results/signed24-sensitivity-2026-09-09/`: completed algebraic audit of
  balanced signed 24-bit digits, exact predecessor intervals, masks, carries,
  and layouts. Of 408 layouts, 328 selected fingerprints are absent from the
  old scanner's fingerprint set. This documents a conditional coverage gap;
  no signed-layout streams were scanned, and the user redirected the work
  toward a recovery mechanism rather than further layout enumeration.
- `results/primitive-analysis-2026-09-09/`: exact repeated-sweep inversion and
  exact-length checksum bijection proofs, supported by independent Python/C++
  checks. All 139 digest controls passed. Full-state inversion requires hidden
  bytes; a checksum choice fixes the original message. The published MD2
  long-message preimage construction therefore does not establish a practical
  attack on the preserved 16-byte MD1 state model.
- `results/md1-state-recovery-2026-09-09/`: one blind full-round SMT attempt
  with all 128 state bits unknown and two complete adjacent digests. The
  watchdog stopped it after approximately 58.64 seconds without a result.
  A separate known-state control made the saved equations SAT in 2.279 seconds;
  this validates one witness, not recovery. Two held-out fixtures were not
  solved. Original logs, a clock-origin correction, and complete constraints
  are preserved. This experiment does not establish a preimage hardness bound.
- `results/rust-kernel-2026-09-09/`: safe Rust translation compared with
  unchanged production C++ on Apple M4, using native optimization and full LTO.
  All 181 digest cases and 21 carry cases agreed. Six alternating timing pairs
  for each of two workloads gave less than 1% median Rust throughput gains,
  with overlapping ranges. This is a limited local near-parity result, not
  universal language performance, an AMD VM measurement, or a new factoring
  algorithm. No production scanner was replaced.

These findings are mathematical constraints and experimental controls, not
factorization results. MD1's historical use by this challenge remains unproved.
