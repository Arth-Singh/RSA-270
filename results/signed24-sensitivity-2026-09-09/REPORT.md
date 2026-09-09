# Signed 24-bit storage sensitivity

Analysis only, 2026-09-09. No production scanner was changed and no seed streams were scanned.

## Result

Canonical balanced 24-bit storage produces materially different fixed patterns for all 34 known factors. Across the 408 word-order and byte-permutation layouts, 328 selected eight-byte fingerprints are absent from the current scanner's 784 unique fingerprints. For 322 layouts, no fully fixed eight-byte window equals any current fingerprint. The common fixed positions contain 390 changed bits in total across the 34 factors.

This is a conditional algebraic sensitivity result. The DSP paper establishes a signed arithmetic representation, not the format into which its RNG wrote bytes. It supplies no firmware evidence that random digest bytes were interpreted directly as signed digits. A finite alternative can be defined mathematically, but no uniquely motivated historical RNG-to-candidate mapping has been established. Work stops at this artifact.

## Representation and padding

The [Dusse/Kaliski paper](https://link.springer.com/content/pdf/10.1007/3-540-46877-3_21.pdf), printed p. 236, uses base `B = 2^24` with digits in `[-2^23, 2^23-1]`. Let `H = B/2`. A canonical encoding follows:

```text
q, r = divmod(x + H, B)
digit = r - H
stored_word = digit mod B
x = q
```

Repeat until the quotient is zero, retaining one zero word for zero. Each word is a 24-bit two's-complement image of its individual signed digit. It is not a conventional unsigned limb and not a single conventional two's-complement integer spread over multiple words.

For stored words `w_i` in least-significant-word-first order, decoding is

```text
S = sum((w_i if w_i < H else w_i - B) * B^i)
  = sum(w_i * B^i) - B * sum([w_i >= H] * B^i).
```

Balancing carries into the next word. For example, `H-1` stores as `[0x7fffff]`, `H` as `[0x800000, 0x000001]`, and `B-1` as `[0xffffff, 0x000001]`. The first two examples show when an extra positive top word is required. A sequence of `n` balanced digits can represent positive integers only up to `(H-1)*(B^n-1)/(B-1)`. Leading zero digit padding preserves the value; it does not repair an insufficient width.

None of the 34 actual factors needs more words than `ceil(factor_bits/24)`. Their signed positive encodings use 7 through 18 words. Explicit threshold and carry-chain fixtures verify cases that do need an additional word, including the corresponding negative borrow cases.

## Exact masks and endpoints

For each factor `p`, the script computes the previous value accepted by the exact preserved predicate: small-factor exclusions 3/5/7/11, base-2 Fermat acceptance, and exponent-3 compatibility. It does not replace this boundary with an ordinary previous prime. All 34 targets lie away from the high-quarter wraparound boundary.

After parity folding, the raw integer interval mapping upward to `p` is exactly

```text
I = [previous_accepted + 1, p].
```

Every integer in this interval is independently encoded by two methods. The exact storage consensus mask is computed by bitwise AND and OR over the complete set, not inferred from an ordinary unsigned prefix. Fresh candidate `p` is included and its complete stored representation is separately recorded. The `p-1` input also maps directly to `p` after odd-parity forcing.

Two masks are saved per factor:

1. The exact consensus of the fully formatted positive candidate's storage, including its top words.
2. A retained-bit mask with storage coordinates at or above `factor_bits-2` erased. This accounts for the two high bits that the conditional RSAREF formatting model overwrites.

Why the second mask is valid under the signed-word hypothesis: centered division makes the lowest `r` stored bits depend only on the decoded integer modulo `2^r`. Adding a multiple of `2^(factor_bits-2)` cannot change those retained stored bits. The script checks positive and negative high-bit lifts, including the representable-range extremes. A full candidate check would still be necessary because a consensus mask generally describes more values than the exact interval.

Endpoint controls explicitly verify that `previous_accepted` is excluded, `previous_accepted+1` maps into the basin, `p-1` and `p` map to the fresh candidate, and `p+1` is outside the basin. Constructed intervals crossing one or several digit sign thresholds verify sign carry/borrow and the resulting storage-mask changes.

## Comparison with the completed scanner

The baseline reconstruction reproduces the current 1,404 patterns: both integer byte orders; its nine lane permutations; minimal byte buffers plus 16-, 24-, and 32-bit RSAREF allocation lengths; and its exact predecessor-derived masks. It reproduces 784 distinct selected eight-byte fingerprints.

The signed comparison uses minimal positive signed width, two whole-word orders, and all six permutations of the three bytes in each word: `34 * 2 * 6 = 408` layouts. Each tuple explicitly describes which raw byte position receives each numeric word byte. Complete three-byte words are used, so no partial-lane policy is needed.

The masks retain 154 through 420 fixed bits, depending on the factor. All 34 differ from the ordinary unsigned representation on positions fixed in both models. The detailed values, masks, word lists, selected anchors, and old-anchor comparisons are in `sensitivity.json`; `factor-summary.tsv` is the compact per-factor table.

An existing fingerprint appearing elsewhere in a signed pattern does not automatically prove that the previous bounded run covered the whole signed layout: the old candidate-buffer start and end constraints also matter near stream boundaries. Conversely, the 322 layouts without any old fixed window demonstrate that the earlier zero-fingerprint result cannot be transferred wholesale to this representation.

## Conditional bounded test, not a recovered implementation

A precisely specified further hypothesis would take consecutive full three-byte words from a bounded digest stream, map them to signed digits using one of the 12 listed storage layouts, decode `S`, apply `3*2^(b-2) + (S mod 2^(b-2))` and odd-parity forcing, and verify the exact interval or fresh candidate. The existing 50 encodings, 17 sample counts, weights, digests, and counter hypotheses could remain unchanged. This would add a representation interpretation, not new seed entropy or an unbounded offset search.

That test is mathematically well-defined **only after choosing the direct-signed-word input hypothesis**. The recovered paper does not establish it. An equally compatible implementation could generate an ordinary byte-string integer and convert it to signed arithmetic afterward, in which case these stored-word patterns would never occur in RNG output. This audit does not choose between those implementations or recommend another exhaustive job without an explicit scope decision.

## Independent checks and retained evidence

The final run passed in under one second and performed:

- 2,370 independent centered-division versus ordinary-digit-rebalancing roundtrips, with two distinct decode formulas.
- Nine explicit word fixtures, including sign carries, negative borrows, positive top padding, and leading-zero padding.
- Five constructed multiword carry/borrow threshold intervals.
- 17,334 full interval-integer encoding comparisons across the 34 known factors.
- 678 high-bit lift checks, including negative signed values and width endpoints.
- 170 inclusive/exclusive candidate endpoint checks.

`run-manifest.json` contains the exact command, final complete stdout/stderr, return code, timing, and hashes. It also records an initial interpreter-compatibility error (`int.bit_count` unavailable) and its replacement with `bin(value).count("1")`; that change did not alter the arithmetic. `output.txt`, `factor-summary.tsv`, and `sensitivity.json` preserve the output and detailed results.

Reproduce from the isolated worktree root:

```sh
python3 results/signed24-sensitivity-2026-09-09/signed24_sensitivity.py
```

The unchanged production scanner SHA-256 remains `48188e1f0e5b39270c7de1f537e3c4063b0a4ee8e3c2f534d34c8db832bd0c6a`.
