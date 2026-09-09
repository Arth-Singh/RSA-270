# Direct MD1 structure analysis

The user requested a mechanism that could reduce the cost of recovery, rather
than additional seed or storage-layout enumeration. This investigation studies
the exact preserved MD1 primitive and published preimage techniques. It has not
produced an RSA-270 factor, a recovered state, or a practical attack.

## Findings

The [primary-source analysis](literature/MD1_MD2_APPLICABILITY.md) derives two
exact identities from `src/md1.cpp`:

- After its first sweep, the compressor repeats one invertible permutation of
  48 bytes. Its inverse requires all 48 output bytes. The digest retains only
  16 bytes, so the inverse alone does not recover a digest preimage.
- For an exactly 16-byte message, the checksum after padding is a bijective
  function of that message. A proposed checksum therefore fixes the message
  and its earlier chaining value; those values cannot be chosen independently
  for a compression-function meet-in-the-middle attack.

Exactly 16 input bytes require three full compression calls: the message, a
complete padding block, and the checksum. The published MD2 long-message
preimage construction does not preserve these restrictions, and its published
memory requirement is far beyond the available VM. This is not a security
proof for MD1 or a claim that every possible new attack has been excluded.

The implementation was recovered from a historical Lotus binary. Its use by
the RSA challenge generator remains a separate, unproved hypothesis. Even
successful synthetic MD1 state recovery would need validation against actual
challenge evidence before it could support factor reconstruction.

The separate [full-state solver experiment](../md1-state-recovery-2026-09-09/README.md)
encoded two adjacent full digests with all 128 state bits unknown. It returned
no recovered state before its watchdog stopped the search. Supplying the
separately planted state made the saved equations satisfiable; that control
validates the encoding for the fixture and does not count as state recovery.

## Independent implementation checks

`structural_checks.py` uses a separate Python implementation and the S-box
literal from the preserved source. `md1_hex.cpp` is a binary-safe adapter to
the production C++ implementation. The compiler used `-Werror`; all checks
passed. Complete commands, source and binary hashes, compiler output, timings,
test inputs and production output are retained here.

- 512 arbitrary full-state fixtures checked both directions of the repeated
  sweep inverse, both directions of the first-sweep inverse, and the identity
  that absorbs the first-sweep reset into a one-byte correction.
- 512 message fixtures and 512 arbitrary checksum fixtures checked the exact
  checksum bijection in both directions.
- 139 Python/C++ digest comparisons included all four preserved known answers,
  128 arbitrary 16-byte inputs, and seven padding-boundary lengths.
- All 128 exact-length controls matched the explicit three-block composition.
- A diagnostic over 4,096 messages with a fixed chaining value found one
  distinct first-16-byte result after one sweep and 256 after two sweeps.
  This illustrates the one-byte boundary dependence of each sweep. It supplies
  no shortcut for the full 18-sweep, three-block digest.

The algebraic identities hold by the equations in the analysis, not by assuming
that a finite set of passing controls proves every case. The controls check
that the equations and implementations agree on independent examples.

## Reproduction

From the repository root, build and run:

```sh
mkdir -p results/primitive-analysis-2026-09-09/build
c++ -O2 -std=c++20 -Wall -Wextra -Werror -Isrc \
  results/primitive-analysis-2026-09-09/md1_hex.cpp src/md1.cpp \
  -o results/primitive-analysis-2026-09-09/build/md1_hex
python3 results/primitive-analysis-2026-09-09/structural_checks.py
```

These checks are local mathematical controls. No cloud scan was launched for
them. Publisher PDFs and renderings remain local; source URLs and hashes are
recorded in the literature provenance file.
