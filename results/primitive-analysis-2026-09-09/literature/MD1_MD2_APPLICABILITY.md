# MD2 preimage literature versus exact 16-byte recovered MD1

## Result

No attack below `2^64` work, or with feasible memory, is established for
`MD1(S)` and `MD1(counter(S))`, where `S` is exactly 16 bytes. The published MD2
`2^73` figure does not transfer to this problem. This is a rejection of that
transfer and of a specific published attack family's claimed affordability,
not a security proof for MD1 or a lower bound against every possible attack.

## What the primary paper actually supplies

Thomsen's attack separates message and chaining-input lists. Section 4.1 requires
`b+c >= 136`; its list-generation stages cost approximately `2^(b-1)` and
`2^(c+8)` MD2 compression evaluations. The selected parameters are `b=72`,
`c=64`, and seven fixed message bytes. Section 4.2 repairs the checksum using
a `2^128`-member multicollision with 128-block messages and a checksum
meet-in-the-middle. The resulting preimage has 129 padded data blocks before
the checksum block. Reported total work and storage are about `2^73`
compression evaluations and `2^73` message blocks, respectively.
These statements concern MD2 and freely constructed long messages.
[Thomsen, ePrint 2008/089, Sections 3–4, PDF pp. 3–8](https://eprint.iacr.org/2008/089.pdf).

The later journal record confirms the MD2 result and authorship. Its full text
was not obtained or used to assert any additional attack.
[Knudsen, Mathiassen, Muller and Thomsen, Journal of Cryptology 23, 72–90,
DOI 10.1007/s00145-009-9054-1](https://orbit.dtu.dk/en/publications/cryptanalysis-of-md2/).

## Exact reduction imposed by this code

Let `F(h,m)` denote the recovered MD1 compression function, `B=10...10` the
16-byte hexadecimal padding block, and `U_m(c)` its checksum update on block
`m`. For an exactly 16-byte input `S`, the source computes

```text
c1 = U_S(0)              h1 = F(0, S)
c2 = U_B(c1)             h2 = F(h1, B)
MD1(S) = F(h2, c2)
```

There are three compression calls. Padding and initial IV are fixed. Appending
the checksum does not grant another free message block. Although the final
loop mutates checksum bytes as it consumes them, each byte is read before its
own mutation and earlier iterations do not modify future checksum entries, so
the appended block is the pre-loop `c2`.
These are direct consequences of [md1.cpp](../../../src/md1.cpp), lines 40–81,
and the zero-initialized arrays in [md1.hpp](../../../src/md1.hpp), lines 26–29.

The checksum gives a particularly useful exact constraint. Write the recovered
S-box as `s`; all 256 entries are distinct. For the first block, with zero
initial checksum,

```text
c1[0] = s(S[0])
c1[i] = s(S[i] xor c1[i-1])             (i >= 1)
S[0]  = s^-1(c1[0])
S[i]  = s^-1(c1[i]) xor c1[i-1]         (i >= 1)
```

For any fixed block `m`, the checksum map `d=U_m(c)` is also invertible:

```text
c[i] = d[i] xor s(m[i] xor d[i-1])      (i >= 1)
c[0] = d[0] xor s(m[0] xor c[15])
```

Thus `K(S)=U_B(U_S(0))` is a permutation of 128-bit strings. This does not lose
entropy. It means a proposed final checksum block `C` fixes the original
16-byte input uniquely as `S=K^-1(C)`. Every legal final-compression input must
lie on the coupled set

```text
(h, m) = (F(F(0, K^-1(C)), B), C).
```

An arbitrary compression pseudo-preimage `(h,C)` need not satisfy this relation.
The published long-message checksum repair cannot impose it while preserving
the exact input length. At the first compression the IV is fixed, so the
independently chosen chaining-input list is unavailable there as well.

## Identical sweeps: real structure, insufficient attack

MD2 uses its own S-box and a round-dependent addition to `t`; recovered MD1
uses `s((t+48-j) mod 256)` at byte position `j` and no round-dependent addition.
[RFC 1319, Appendix A.3](https://www.rfc-editor.org/rfc/rfc1319.txt) supplies the
MD2 reference code; the local source supplies the MD1 comparison. The position
addition and S-box remain invertible, so the triangular “two known values
determine the third” reasoning still works. This preserves an attack technique,
not its cost or applicability to constrained full hashes.

There is an exact repeated permutation. After the first sweep, `t` equals
state byte 47. Subsequent sweeps apply the same permutation `P` to 48-byte
states. For `y=P(x)`, its inverse is

```text
x[j] = y[j] xor s((y[j-1] + 48-j) mod 256)    (j=1,...,47)
x[0] = y[0] xor s((x[47] + 48) mod 256).
```

This inverse proves the permutation property algebraically for all 48-byte
states; it is not inferred from a finite sample. It does not recover a full
state from the 16-byte projection, and no such recovery algorithm is established
here.

Let `Q` be the first sweep with `t=0`, `E(h,m)=h||m||(h xor m)`, and `pi`
retain the first 16 bytes. Then `F(h,m)=pi(P^17(Q(E(h,m))))`. Equivalently,
`Q=P o J`, where `J` changes only byte 0 by XORing
`s(48) xor s((x[47]+48) mod 256)`. The initial reset can therefore be absorbed
into preprocessing; it alone is not a proof against slide attacks.

The actual barriers are the constrained input embedding, projection that hides
32 output bytes, the fixed IV, and three block transitions with linked padding
and checksum. The available digest pair does not expose a pair of full
384-bit inputs and outputs related by `P`. No equation connecting the input
counter directly to `P` has been derived.

## Work, memory, and related-output barriers

Within the published list attack's rounded cost model, even ignoring matching
and checksum work, define its dominant-stage work proxy `W`. Its requirement
gives

```text
W >= max(2^(b-1), 2^(c+8))
  >= 2^(((b-1)+(c+8))/2)
  >= 2^71.5.
```

The `2^71.5` value concerns that published MD2 independent-list construction and
its rounded compression-equivalent estimates, not an exact instruction-count
lower bound and not any lower bound on MD1. Removing round constants
does not by itself remove its independent-list size requirement or supply a
new cheaper matching relation. A distinct construction is needed for a
sub-`2^64` claim.

The paper's storage figure corresponds to about `2^77` bytes when each message
block is counted as only 16 bytes: `2^38` times 512 GiB. Even a hypothetical
`2^64`-entry table with only 16 bytes per entry requires `2^68` bytes. No
replacement time–memory tradeoff achieving the requested work and memory
bounds has been derived here.

For a known counter permutation `T`, the two checksum blocks are related by
`C' = K(T(K^-1(C)))`. They cannot be selected independently. Even granting two
genuine complete digests and a known exact counter, the two targets constrain
one 128-bit unknown; they do not create a birthday collision search or an
independently computable 64+64-bit meet-in-the-middle. An ideal-function
benchmark still requires work on the scale of searching the 128-bit input
space; this benchmark is not a claim that the structured MD1 function achieves
ideal security. Additional related outputs can verify candidates without
establishing a method to enumerate them cheaply.

To establish a useful new method, one must exhibit the matching relation or
differential/slide trail, preserve the exact IV/padding/checksum constraints,
and count surviving candidates, work, memory, and required data. Identical
sweeps and local invertibility alone supply none of those bounds. The separate
question of whether the challenge generator actually used this recovered MD1
remains unproved.

## Preservation

The complete ePrint was downloaded locally; equation/algorithm pages 5–8 were
rendered and visually checked. `provenance.json` records source and PDF hashes,
the S-box permutation check, and which primary material was actually read.
Downloaded PDF, extracted text, and page images are excluded through local
`.git/info/exclude`; nothing was published. No scanner source or cloud job was
changed by this analysis.
