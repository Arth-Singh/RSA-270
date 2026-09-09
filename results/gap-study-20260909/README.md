# Matched predecessor-gap experiment

This experiment addresses the unfinished matched-control test in `CONTEXT.md`.
It does not factor RSA-270 or establish which historical implementation produced
the challenge numbers.

## Local pilot result

The 4,096-dataset-per-model pilot completed in 200.518 seconds with two worker
threads. The observed normalized gap sum is `41.260233365853622`.

| Conditional model | Simulated mean | Scores at or below observed | 95% interval for lower-tail probability |
| --- | ---: | ---: | ---: |
| Uniform raw start followed by upward scan | 66.863828 | 0 / 4,096 | [0, 0.00090020] |
| Uniform predicate-accepted output | 33.988033 | 3,681 / 4,096 | [0.88903240, 0.90775785] |

Intervals are Clopper–Pearson binomial intervals. The upward model's plus-one
Monte Carlo rank statistic is `1/4097 = 0.000244081`; the pilot has insufficient
tail events to estimate the old approximate `8.5e-5` probability precisely.
Zero tail observations do not imply zero probability. The observed statistic is
unusual under this specific uniform-start scanning model and lies near the 90th
percentile of the uniform-accepted control. This remains an exploratory model
comparison: the statistic and corpus were already inspected, other tests were
tried, and no global multiple-testing correction has been established.

The C++ basin tests, independent Python audit (34 observed factors, 544 simulated
factors, and 157,841 rejected predecessor candidates), binomial-CDF tests, and
one-versus-two-thread reproducibility check all passed. The final build produced
no warnings. Full values are in `pilot.summary.json` and the retained TSVs/logs.

## Models and sampling

The dataset is exactly the 17 stored decimal-list challenges RSA-100 through
RSA-260, with 34 factors. Each simulated dataset uses those same 34 observed
factor bit lengths. A factor of length `b` is confined to
`[3 * 2^(b-2), 2^b)`. The loader checks every stored product and the balanced bit
allocation. It also verifies with exact integer arithmetic that **every** product
in each pair's two ranges already has the observed decimal-label length. Thus
conditioning on the 17 decimal labels requires no further weighting or rejection.
The nine equal-bit pairs require no generation-order choice because the statistic
is symmetric in the two factors.

Acceptance reproduces `references/rsaref/raw_prime-old.c` on its odd-candidate
domain: reject divisibility by 3, 5, 7, or 11; require `2^n mod n == 2`; and require
`gcd(n-1, 3) == 1`. Reordering the inexpensive checks does not change this
predicate. This is deliberately not a modern prime test: composite 8321 is
accepted. No Miller–Rabin or larger trial-division sieve filters simulated output
or predecessor values. GMP probable-prime classifications in the observed table
are diagnostics only.

The two models are sampled directly:

1. `upward_scan`: draw a uniform raw residue of `b-2` bits, force odd parity, and
   follow the old upward search with its exact wrap at the upper endpoint.
2. `uniform_accepted`: repeatedly draw a new uniform raw residue and force odd
   parity until that candidate passes the same predicate. Every odd candidate has
   exactly two raw preimages, so this rejection sampler is uniform over accepted
   values. It does not use next-prime sampling, sampled windows, inverse-gap
   weights, or a bootstrap from an empirical control pool.

Predecessors use that same acceptance predicate and cyclic interval. For width
`W=2^(b-2)` and cyclic predecessor gap `g(p)`, exactly `g(p)` raw residues reach
`p` under upward scanning. Consequently `P_scan(p)=g(p)/W`, while
`P_uniform(p)=1/A` for the unknown total number `A` of accepted values. Neither
simulation needs an estimate of `A`.

The primary statistic is `S = sum(g(p) / (2 ln(p)))` over all 34 outputs. This
reproduces the previously reported statistic and equals
`41.260233365853622` in the stored corpus. The divisor is a descriptive
normalization, not an assumed exact mean gap; both control models compute the
same statistic on their own draws. A secondary sum of log normalized gaps is
retained for inspection but is not reported as an exact likelihood ratio.

Each dataset gets its own `std::mt19937_64` stream seeded by
`splitmix64(master_seed + 2 * replicate + model_index)`, with uniform index 0 and
scan index 1. Explicitly imported random words avoid standard-library differences
in integer distribution algorithms. Results are reproducible across thread
counts. Different models use different streams; they are matched by design and
factor lengths, not by reusing the same prime draws.

## Validation and recorded runs

`--self-test` exhaustively checks 4,080 raw residues across bit lengths 6 through
13. All 250 accepted basins have exactly their cyclic-gap number of scan
preimages, and exactly two rejection-sampler candidate preimages. Eight first
accepted values exercise cyclic endpoint gaps. The accepted-composite check
prevents silently replacing the source-exact predicate with modern primality.

`audit.py` independently recomputes the 34 observed predecessors using the
existing Python reconstruction and audits every saved factor and predecessor in
the small benchmark, including all rejected candidates between predecessor and
output. It also recomputes each dataset score. A second one-thread benchmark
checks thread-count reproducibility, excluding elapsed-time fields.

`analyze.py` reports direct lower-tail counts, the finite-simulation plus-one rank
statistic `(k+1)/(R+1)`, and a 95% Clopper–Pearson interval for the underlying
conditional-model tail probability. Its binomial-CDF implementation is checked
against explicit finite binomial sums and boundary cases. It rejects duplicate
`(seed, replicate, model)` draws when combining files. It must not combine the
eight-dataset benchmark with the pilot, which intentionally reuses its first
eight replicate identifiers.

The benchmark retains all 544 simulated factors and predecessors. The main pilot
uses 4,096 independent datasets per model, 278,528 simulated output factors total,
on at most two local worker threads. It retains every dataset score and counter;
the deterministic source and seed reproduce individual factor draws. No VM job
was run by this worker. `commands.txt`, `manifest.json`, complete logs, TSVs, and
JSON summaries record the exact invocations, inputs, source hashes, output hashes,
compiler, and measurements. Compiled binaries stay in ignored `build/` and must
be rebuilt elsewhere.

## Interpretation limits

- This statistic was selected after inspecting this corpus and after other
  exploratory probes. A calibrated conditional-model tail probability is not a
  multiplicity-adjusted, confirmatory historical-attribution test.
- The models assume independent, uniformly distributed raw residues within the
  matched ranges. They do not characterize arbitrary correlated PRNG output,
  reseeding schemes, unknown DSP firmware, extra selection rules, or every prime
  construction method. They say nothing directly about seed entropy.
- The corpus contains the 17 solved stored decimal-list pairs. Any dependence of
  historical publication or factorization selection on the gap statistic is not
  modeled. No claim of an unbiased random sample of all original challenges is
  made.
- A small pilot is useful to check scale and implementation. Sparse or zero tail
  counts have wide Monte Carlo uncertainty; zero observations do not establish a
  zero probability or reproduce the old approximate `8.5e-5` estimate.
- Rejecting a conditional uniform-start statistical model would not exclude an
  RSAREF-derived implementation with different randomness or selection. Favoring
  uniform accepted outputs would not prove fresh-candidate historical sampling.
- No factor, useful generator state, or new entropy reduction is produced.

## Bounded follow-up on the existing VM

After copying or cherry-picking this directory, rebuild from source on Linux:

```sh
mkdir -p build/gap-study
c++ -O3 -std=c++20 -Wall -Wextra -Wpedantic \
  results/gap-study-20260909/gap_monte_carlo.cpp \
  -lgmpxx -lgmp -pthread -o build/gap-study/gap_monte_carlo
build/gap-study/gap_monte_carlo --self-test
timeout 600 build/gap-study/gap_monte_carlo \
  --replicates 131072 --threads 64 --seed 20260910 \
  --output-prefix results/gap-study-20260909/vm \
  > results/gap-study-20260909/vm.log 2>&1
python3 results/gap-study-20260909/analyze.py \
  --observed results/gap-study-20260909/vm.observed.tsv \
  results/gap-study-20260909/vm.samples.tsv \
  > results/gap-study-20260909/vm.summary.json
```

Use only free cores; an already saturated seed scan should finish or yield
capacity first. Benchmark a small run on the VM before trusting linear runtime
extrapolation from the laptop. The 600-second cap is a new experimental bound,
not a renewal of the historical one-day deadline. If interrupted, the program
has not completed: it writes its final score table after joining all workers, so
an empty score table must not be labeled a negative result. Smaller complete
shards can use disjoint `--first-replicate` intervals with the same seed and can
then be combined by `analyze.py`.
