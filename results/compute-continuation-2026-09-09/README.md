# Compute continuation and model decisions, 2026-09-09

RSA-270 remains unfactored by this project. No nontrivial factor or usable
generator state was found. The exact target was checked against the preserved
challenge announcement, and the retained FactorDB response still reports `C`
for that exact integer. This status lookup is not a mathematical impossibility
claim.

## Exhaustive conditional seed coverage

The existing VM sweep was observed running at 08:14:35 UTC on all 128 threads.
Its scanner source hash was
`ec3520653e3667a3619c4cf3cf2541e9aa49ca9addfb1a23f361c17edd8ce6e5`.
The source and executable modification times were respectively
08:02:55.490323788 and 08:02:57.874365866 UTC; its retained self-test passed.
The original driver recorded the source hash but did not record a binary hash.
That provenance limitation remains; a later rebuild cannot retroactively
identify an earlier executable.

All 20 word-family jobs completed with full expected variant counts and no
match: `raw-word`, `raw-repeat-word`, `md5-word`, `md5-word-times64`, and
`md5-repeat-buffer`, each over every 1-, 2-, 3-, and 4-byte word. The scanner
tests both byte encodings where applicable, both counter transitions, and
16-, 24-, and 32-bit digit-width models. These counts contain duplicate states;
they are not independent entropy coverage.

The old `ansi-rand` job also completed its full 32-bit range. Its following
`ms-rand` job was deliberately interrupted at 08:19:40 UTC after discovering
redundancy. `stop_redundant_sweep.py` validates the exact driver command and the
final word-family completion before stopping only that driver's LCG work. The
old output remains under `results/rsaref-state-sweep-2026-09-09/`; its interrupted
MS job is not a negative result, and no old Borland completion is claimed.

For each LCG, `s_next = a*s+c mod 2^32`, but the emitted byte is only state bits
16 through 23. If two seeds differ by a multiple of `2^24`, induction through
the recurrence preserves that divisibility. Every emitted byte, the full
256-byte seed buffer, and its MD5 digest are identical. Thus `[0,2^24)` covers
every stream represented by `[0,2^32)` for these exact three implementations.
This is a proved symmetry, not a heuristic truncation of the seed space.

The corrected driver rebuilt the scanner and ran all three distinct LCG ranges
with 128 threads. Each returned no match and all 33,554,432 expected
state-transition variants. The combined scan time was about 0.67 seconds;
build and self-test were additional. Complete output, compiler, source and
driver snapshots, and binary hash are retained under
`results/rsaref-lcg-distinct-2026-09-09/`.

`scan-coverage.json` records 24 completed jobs (including the redundant old ANSI
completion), one interrupted job, and per-log hashes. It checks exact variant
counts, rather than using an exit code alone. The corrected driver also refuses
to overwrite an output directory, stops on unexpected scanner exits, and stops
for investigation on a state or product hit. The retained driver fixture checks
that exit 2 cannot produce a `finished=` marker and that existing evidence is
not overwritten. Actual LCG runs exercised its complete-negative path.

Independent Python-generated controls passed for all eight state families,
counter carries, all three reconstruction widths, and deliberate target
mismatches. See `results/scanner-validation-2026-09-09/README.md`. These controls
validate the conditional source model; synthetic `MATCH` output in their logs
is not RSA-270 factorization evidence.

## Matched predecessor-gap result

The independently checked gap program ran 131,072 direct datasets under each
model with 128 VM threads, seed 20260910, in 245.9913 seconds. All 262,144 jobs
completed. The full 25.7 MB sample table, observed gaps, exact command, source
and binary hashes, compiler, and complete output are retained under
`results/gap-study-20260909/`.

The statistic is the sum over the 34 observed factors of the preceding
old-predicate-accepted gap divided by `2*ln(p)`. Its observed value is
41.26023336585363. Both controls use the exact old Fermat predicate, actual bit
lengths and high-quarter ranges, e=3, and cyclic endpoints. Every possible pair
in each permitted pair of ranges already has its actual decimal-label length.

| Conditional construction | Simulated mean | Datasets at or below observed | Estimated lower-tail probability | 95% binomial interval |
| --- | ---: | ---: | ---: | ---: |
| Uniform-start upward search | 66.7210 | 10 / 131,072 | 0.000076294 | [0.000036586, 0.000140303] |
| Uniform accepted candidate | 33.9915 | 117,263 / 131,072 | 0.894646 | [0.892971, 0.896303] |

The plus-one Monte Carlo rank p-value for upward search is 0.000083923. The
statistic was selected after inspection of this corpus and other probes, so
this is an exploratory conditional-model diagnostic, not a multiplicity-
adjusted test that identifies or formally rejects a historical generator.
Matching the uniform-accepted control does not prove fresh-candidate sampling.

Research decision: down-rank expanded absolute-offset seed searches based on
the uniform-start upward loop. The observation weakens a premise needed for
that route; it does not recover entropy, a state, or a factor.

## Primary-source recovery and next bounded test

The original Dusse/Kaliski DSP paper is now preserved locally with verified
page renders and provenance under `results/generator-evidence-2026-09-09/`.
It confirms a physical-noise source, signed 24-bit arithmetic, and configurable
host byte transfers. It does not supply the challenge's seeding or prime loop.

A separate, bounded noise-seeding test completed. The exact preserved
RSAREF seed-update operation adds hashes and therefore loses sample order. If
binary comparator samples were hashed separately, a fixed sample count has
only one state per Hamming weight. Associating that operation with the DSP is
still a hypothesis. The test covers MD5 and the recovered MD1,
explicit arithmetic/counter alternatives, and finite sample encodings/counts,
while allowing arbitrary starts inside a bounded output stream instead of
assuming one aligned stream through the entire challenge list. At a 16 KiB
stream bound, all 2,081,200 configured streams completed in 50.0564 seconds on
128 VM threads. There were zero fingerprint, interior-pattern, or candidate
hits across 34,083,812,400 windows. Full output and exact coverage are under
`results/noise-hamming-vm-2026-09-09/`.

A small signed-24-bit representation audit is next. Byte-lane permutations
alone do not cover balanced signed digits; the primary DSP paper establishes
the arithmetic representation but does not establish RNG candidate packing.
Validate the mapping and any new masks before using it to expand a scan.

## Reproduction and operations

The following commands were run from the root workspace. SSH used the existing
authorized key and host; no other VM project was changed.

```sh
ssh -i ~/.ssh/share_nebius_ed25519 -o BatchMode=yes -o ConnectTimeout=10 arth@89.169.111.225 python3 - < results/compute-continuation-2026-09-09/stop_redundant_sweep.py > results/compute-continuation-2026-09-09/stop-redundant-sweep.log 2>&1
scp -i ~/.ssh/share_nebius_ed25519 -o BatchMode=yes scripts/rsaref_state_sweep.sh arth@89.169.111.225:RSA-270/scripts/rsaref_state_sweep.sh
ssh -i ~/.ssh/share_nebius_ed25519 -o BatchMode=yes -o ConnectTimeout=10 arth@89.169.111.225 'cd ~/RSA-270 && scripts/rsaref_state_sweep.sh results/rsaref-lcg-distinct-2026-09-09 128 lcg' > results/compute-continuation-2026-09-09/lcg-launch.log 2>&1
ssh -i ~/.ssh/share_nebius_ed25519 -o BatchMode=yes -o ConnectTimeout=10 arth@89.169.111.225 'tmux new-session -d -s gap-20260909 "bash ~/RSA-270/results/compute-continuation-2026-09-09/run_gap_vm.sh"'
python3 results/gap-study-20260909/analyze.py --observed results/gap-study-20260909/vm.observed.tsv results/gap-study-20260909/vm.samples.tsv > results/gap-study-20260909/vm.summary.json
python3 results/compute-continuation-2026-09-09/check_sweep_driver.py > results/compute-continuation-2026-09-09/driver-check.log 2>&1
```

`run_gap_vm.sh` contains the exact build and simulation command. Each remote
results directory was copied back with `rsync -a` over the same SSH connection.
Use fresh output directories when reproducing guarded drivers. Do not reuse
the historical process identifiers in the stop script.
