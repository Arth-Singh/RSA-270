# Independent validation of the conditional RSAREF scanner

The final run passed on 2026-09-09. It validates production scanner source
SHA-256 `ec3520653e3667a3619c4cf3cf2541e9aa49ca9addfb1a23f361c17edd8ce6e5`
from base commit `44a3da5b4cbc2f7ab3d711a4cc10e66eed59ea03`. No production scanner,
sweep driver, or existing documentation was changed by this validation work.

The executed command, from the scanner-controls worktree, was:

```sh
python3 scripts/validate_rsaref_scanner.py --out results/scanner-validation-2026-09-09/final
```

The runner refuses to reuse an existing output directory. To reproduce, choose
a fresh directory:

```sh
python3 scripts/validate_rsaref_scanner.py --out results/scanner-validation-reproduction
```

The runner compiles one C++ translation unit and uses at most two scanner
threads. Dependencies are a C++20 compiler, Python 3, GMP, and OpenSSL. On macOS,
the default library locations are Homebrew's `/opt/homebrew/opt/gmp` and
`/opt/homebrew/opt/openssl@3`; `GMP_PREFIX`, `SSL_PREFIX`, and `CXX` can override
the relevant build settings. No VM was accessed by this validation.

## Independence and coverage

The Python reference uses `hashlib.md5`, Python integer arithmetic, and a
counter conjugation: subtract one from each state byte modulo 256, add the
number of steps as a big-endian integer, then add one to each byte. Six explicit
carry vectors check that conjugation before it supplies expected values. The
prime reference enumerates cyclic odd-candidate indices and uses the preserved
Fermat predicate. It does not call the production C++ functions or import the
existing reconstruction script.

The C++ harness includes the production translation unit with its CLI `main`
renamed. It compares production functions against the independently generated
fixtures, then calls the real `RunScan` function for product matches, deliberate
target mismatches, and ranges without the planted seed.

| Check | Final coverage |
| --- | --- |
| Initial state generation | 471 expected named states across 87 seed groups and all eight modes |
| Word formatting | Widths 1, 2, 3, and 4; zero, an asymmetric byte pattern, and maximum seed; both byte orders and every raw-word placement |
| LCG bounds | Three families, 24-bit boundary values, and equal-output witnesses at seeds separated by `2^24` |
| Counter advancement | 126 fixtures, including zero and all-ones states, full carries, wrap, and multiple steps through 513 |
| MD5 stream generation | 48 fixtures spanning digest boundaries and the 1440-byte target endpoint |
| Acceptance predicate | 12 fixtures, including accepted composite `8321 = 53 * 157` |
| Prime search | 21 fixtures, including raw-residue reduction and upper-bound wrap |
| Actual challenge anchors | All 63 values independently reconstructed and checked across all 512 prime-order choices for each digit width |
| Target geometry | All three offsets/block sizes and the exact RSA-270 integer |
| Real search path | Six product controls: each of widths 16, 24, and 32 under both counter transitions |
| False success prevention | Six otherwise identical anchor hits with target increased by one, all returning `Outcome::kStateOnly` |
| Negative ranges | Six ranges without the planted seed, all returning `Outcome::kNone` |

Each planted model deliberately invalidates the first anchor of the other two
width models. The harness asserts that only the intended width verifies and
that `Probe` selects that width. This prevents a width-16 hit from hiding an
untested width-24 or width-32 reconstruction path. Every planted initial state
ends with a zero byte, making the two transition models diverge before the
first anchor.

## Retained evidence

`final/RUN.json` records exact compiler and harness arguments, tool versions,
source hashes, fixture and binary hashes, exit codes, and measured durations.
`final/reference-fixtures.tsv` contains all expected values.
`final/reference.log`, `final/build.log`, and `final/harness.log` retain complete
output from their respective phases. The build log is empty because compilation
completed without diagnostics. `SHA256SUMS` covers retained text artifacts.
Compiled local binaries are ignored; rebuild them instead of trusting copies.

The first passing run is preserved under `initial/`, including snapshots of its
two new validation sources. Its word fixtures used a symmetric repeated-byte
pattern, so the final run replaced that pattern with `0x1234abcd` truncated to
the selected word width to exercise byte-order differences. The initial
manifest retains the actual paths used before its output was archived.

## Limits

These checks validate the implemented conditional models. They do not establish
that RSA DSP used RSAREF, MD5, any particular seed family, a continuous stream,
or the assumed generation order. The planted products are synthetic controls,
not RSA-270 factorizations. The acceptance test intentionally follows the old
Fermat routine and therefore is not a primality certificate.

This local run does not benchmark VM throughput or certify which executable an
external sweep used. Compare that executable's provenance with the recorded
source and build data before transferring these results to a running job.
