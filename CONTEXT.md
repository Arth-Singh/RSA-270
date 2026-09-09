# RSA-270 research handoff

Saved 2026-09-09 in /Users/arth_rogerthat/Downloads/RSA-270. Research described here was performed on 2026-09-03, with local preservation checks on 2026-09-09. This is a substantive handoff of available findings and decisions, not a verbatim transcript or a substitute for missing experiment logs.

## Objective and actual outcome

The user wanted a novel route to factor RSA-270 in one day. We investigated whether the original 1991 challenge generator left an exploitable construction or random-number-generation weakness.

**No RSA-270 factor, reproducible weak seed, or usable generator state was found.** Do not describe historical-generator archaeology, statistical structure, or a modeled output block as a factorization breakthrough.

The exact target is:

```text
233108530344407544527637656910680524145619812480305449042948611968495918245135782867888369318577116418213919268572658314913060672626911354027609793166341626693946596196427744273886601876896313468704059066746903123910748277606548649151920812699309766587514735456594993207
```

It has 270 decimal digits and 895 bits. The recorded diagnostic remainder is N mod 991889 = 10237. The modeled balanced factor lengths are 448 and 447 bits. The project treated FactorDB's status as composite on September 3; no external factorization-status check was made during the September 9 handoff.

The stored dataset is 17 pairs / 34 factors: RSA-100, 110, ..., 260. It is not "34 solved challenges" and is not the full set of all solved RSA-labelled numbers. RSA-129 has different provenance; later bit-labelled BSAFE challenge numbers must not be mixed into the original-list model. The prior session reported RSA-260's new factorization on September 3 by Eric Lu / @penlume; the data file contains the factors and their exact product was rechecked at handoff. Historical attribution was not freshly researched during handoff.

Original workspace: /Users/arth_rogerthat/rsa-270. Full copy: /Users/arth_rogerthat/Downloads/RSA-270. The original was preserved. The old goal was paused when checked on September 9. The September 4 18:04:30 KST deadline is historical, not a current instruction to run indefinitely.

## Strategy and evidence boundaries

Generic GNFS on this laptop is not a plausible one-day route for a balanced 895-bit RSA modulus. The prior investigation estimated roughly 21,000 reference core-years, but the exact benchmark derivation is not preserved; treat that as an old order-of-magnitude assessment, not a verified present benchmark.

The main alternative was to use known factors as evidence about prime formatting and potentially as partial outputs of an old RNG. A cheap attack requires a real reduction in unknown entropy, an implementation defect, or some other structure; finding a familiar prime-generation pattern alone provides none.

Keep four evidence levels separate:

1. Arithmetic directly checked in stored data.
2. Behavior recovered from a particular historical binary or source file.
3. A hypothesis that the challenge used that implementation or a related one.
4. A search result within a precisely bounded hypothesis.

Negative tests exclude only their documented models. Successful arithmetic checks do not establish historical attribution. Several progress messages in the old session overstated the strength of the RSAREF connection; the corrections below are the intended handoff interpretation.

## Confirmed data properties

All 34 stored factors begin with binary 11. Every factor is 2 modulo 3. For each modulus, the factor bit lengths are floor(B/2) and ceil(B/2), where B is the modulus bit length.

These properties fit a prime range [3*2^(b-2), 2^b) and public exponent e=3 compatibility. They fit old RSAREF's formatting, but are shared by other implementations. The naive statement "probability 2^-34" is conditional on an inappropriate unconditioned null if used as general attribution evidence. It must not be presented as proof of RSAREF.

A contemporary paper found during the search reports the same broad high-bit property and explicitly rejects unique implementation or weakness inference. Its citation and limitations are in docs/SOURCES.md.

The stored statistical probes found no robust interior correlation, periodicity, repeat structure, or low-complexity generator after accounting for the many comparisons. Exact historical outputs are summarized in docs/EXPERIMENTS.md; some were reported by the former research agents and are not backed by retained raw logs.

## Historical RSA DSP hardware

The preserved RSA challenge-list description says the original numbers came from two randomly chosen, approximately equal-length primes, both 2 modulo 3, checked with a probabilistic primality routine. It says the list was generated in about 30 minutes using RSA DSP, a PC board with a Motorola 56000 processor, and the primes were discarded.

Dusse and Kaliski's EUROCRYPT '90 paper describes a related RSA cryptographic board:

- A 20 MHz XSP56001, 24-bit DSP.
- External P program memory and X data memory; the PC can address external memory through a custom interface.
- A hardware noise circuit with a CND6002A noise diode, amplification, and TTL comparator.
- The comparator is connected to the DSP and status registers.
- The authors discuss decoupling the circuit to avoid predictable coupling.
- RSA-MD1 is described as a proprietary digest related to MD2.

This is positive evidence for a physical entropy source, not evidence that the challenge was time-seeded. The paper does not give the challenge's exact firmware, driver, seed conditioning, sampling cadence, or prime-generation loop. Those artifacts were not recovered.

## Recovered 1991 Lotus BSAFE implementation

A Lotus Notes 2.1 package dated 1991-11-16 supplied _BSAFE.DLL and _LIB.DLL. These are adjacent historical implementations, not established copies of RSA DSP firmware. The original DLL downloads were in /tmp and are now missing. URLs and available hashes are preserved in docs/SOURCES.md.

### RSA-MD1

src/md1.cpp and src/md1.hpp implement the digest recovered from _BSAFE.DLL. The original session reported independent x86/Unicorn execution as validation. The emulator scripts are now missing; the C++ implementation and known-answer vectors survive.

The context/finalization shape resembles MD2, but the S-box and compression differ. Each 16-byte input block fills the message and XOR portions of a 48-byte state. The compressor runs 18 rounds of 48 updates using a position countdown added to the S-box index. Do not replace it with standard MD2.

Known answers, all reproduced by the copied md1_cli during handoff:

| Input | MD1 |
| --- | --- |
| empty string | 7ae2a21d8793650044ae40277eafaa2e |
| abc | 74594a0a73b03503efe62e574dbaf816 |
| a | 420965f2da150b44cbfadd695ab5bbe9 |
| 1234567890 | 34dbffc3e4a9c0ad2d5559c99649c69e |

### BSAFE byte RNG

Use src/bsafe1_rng_scan.cpp, not the older Boost-based bsafe_rng_scan.cpp draft.

Recovered state begins as bytes 00..0f. Seed updates XOR bytes into a rotating 16-byte state. Before generating a block, the state bytes 12..15 are incremented as a little-endian counter. RC2 uses state[0..7] as an 8-byte key with effective key size 64, encrypts state[8..15], and yields eight output bytes. This ordering is taken from surviving source; a compressed earlier summary was ambiguous about the increment position.

Source records the offsets: seed update 0x1e76; random-byte 0x1ebb; RC2 setup 0xcc1c; encrypt 0xccb4; PITABLE 0x2863.

RC2 known-answer test: key 0001020304050607, plaintext 08090a0b0c0d0e0f, ciphertext 9f0f9b51d9f31938. This checks the RC2 primitive, not the entire initialized/incremented RNG stream.

### _LIB ordinal 243 and seeding

The session mapped ordinal 243 RANDOM to NE segment 3:30de, physical offset 0x721e. With a TIMEDATE byte array t:

```text
t0 = LE32(t[0:4])
w0,w1,w2,w3 = consecutive LE16 words of t
A = (((A XOR low16(t0/7)) * 0xaf63) >> 11) AND 0xffff
B = (((B >> 1) OR ((B << 1) AND 0xffff))
     XOR w0 XOR w1 XOR w2 XOR w3) AND 0xffff
return A XOR B
```

The BSAFE caller uses the low byte. TIMEDATE represents hundredths since midnight GMT plus a Julian-day field in the low 24 bits of the second word; byte 7 is cleared in the recovered use.

Reported call-site audit: initialization makes 150 calls; a wrapper injects two time bytes initially and every 20 calls; another helper conditionally injects 3*16 samples. The MAKEKEYPAIR prime/exponent path uses the raw RNG, without the wrapper's time injection during key generation.

### Sparse prime-candidate construction

The recovered DLL candidate routine calls random-byte once per little-endian 16-bit limb and puts that byte only in the low eight bits, leaving each interior high byte zero. It forces the top two bits, clears bit zero, and searches upward. The source locates this beyond MAKEKEYPAIR 0x6382, candidate routine 0x6960, relocation 0x6a48.

All 34 known challenge factors were reported incompatible with this sparse interior pattern, independent of the seed. Surviving code includes the exclusion check. Its search bound uses the preceding ordinary prime; review the exact acceptance predicate before generalizing the proof to any variant that can skip earlier primes. This observation rejects the recovered Lotus path as the challenge generator, not all BSAFE versions, all RSA products, or the byte RNG used with different prime formatting.

## Conditional RSAREF reconstruction

The preserved raw source is under references/rsaref/raw_*.c. Files without raw_ were downloaded from Plan 9's source browser and contain HTML wrappers despite their .c extension.

The stock RSAREF 1.0 release postdates the challenge (the historical release date reported was March 2, 1992). Its normal maximum modulus size is 1024 bits, so it cannot generate the entire original challenge list unchanged. An internal predecessor or DSP port remains a hypothesis.

The source pipeline:

1. nDigits = ceil(B / digit_bits); pDigits = ceil(nDigits / 2).
2. GenerateDigits consumes pDigits * digit_bits / 8 bytes, interpreted big-endian. It obtains both raw p and q blocks before finding their primes.
3. For b = ceil(B/2) and floor(B/2), FindRSAPrime starts at 3*2^(b-2) + (raw mod 2^(b-2)); if even, add 1.
4. Search upward by 2, wrapping within the high-quarter interval.
5. Accept iff no factor in {3,5,7,11}, Fermat base 2 passes, and gcd(candidate-1,e)=1. For e=3, factors are 2 mod 3.
6. Sort the two final primes.

This old routine is not a Miller-Rabin implementation. When inverting its interval, the correct predecessor is the previous value accepted by that exact predicate, including any possible base-2 pseudoprime.

src/rsaref_stream_reconstruct.py implements the inverse. If p_prev is the previous accepted value, L=3*2^(b-2), the raw residue lies in [p_prev-L+1, p-L]. This interval has p-p_prev possibilities. Bits above its varying low suffix are fixed; discarded leading bits and the suffix remain unknown.

The stored corpus has nine equal-bit pairs, producing 512 possible generation orders. For odd-bit pairs, larger-bit p is generated first under this model. Digit widths 16, 24, and 32 were considered. The 24-bit width is motivated by DSP arithmetic, not proven to be the generator's actual byte layout.

Handoff regeneration confirmed:

| Digit width | Stream bytes through RSA-260 | Fully known 16-byte blocks in every order | Values independent of order |
| --- | ---: | ---: | ---: |
| 16 | 1292 | 42 | 23 |
| 24 | 1314 | 44 | 22 |
| 32 | 1328 | 32 | 18 |

The phrase "recovered PRNG output blocks" is only valid **conditional on the model**. No observed runtime RNG output or physical random sample was captured.

The model assumes one shared continuous byte stream, challenges generated in the listed order, no extra draws/reseeds, correct byte packing, and the same prime routine. Skipped/injected test draws, retries, a fresh random candidate on rejection, or other generation order can invalidate the absolute alignment.

Modeled RSA-270 offsets:

| Digit width | First raw block offset | Second raw block offset | Bytes per block | End offset | First 16-byte digest index:byte |
| --- | ---: | ---: | ---: | ---: | --- |
| 16 | 1292 | 1348 | 56 | 1404 | 80:12 |
| 24 | 1314 | 1371 | 57 | 1428 | 82:2 |
| 32 | 1328 | 1384 | 56 | 1440 | 83:0 |

All known block values, order alternatives, and the 24-bit factor masks were regenerated and saved in results/handoff-2026-09-09/rsaref-reconstruction.txt.

### The postincrement trap

The actual preserved r_random.c contains:

```c
for (i = 0; i < 16; i++)
  if (randomStruct->state[15-i]++)
    break;
```

This is not normal big-endian +1. It carries when the old byte is zero, not when the new byte is zero. For example, from an all-zero state one step increments all sixteen bytes to 01.

src/rsaref_md5_state_scan.cpp explicitly tests both the literal postfix transition and the intended-looking prefix-increment alternative. src/seed_scan.cpp was patched at the very end to support post-le/post-be using a bytewise subtract-one conjugation around ordinary integer addition. That last patch was not followed by a recorded rebuild or verification. Its copied executable is older than the source.

The MD1 Hamming scanner and older noise_time_scan use ordinary increment and are hypothesis tests, not source-exact RSAREF tests.

### Latest scanner and a success-reporting caveat

src/rsaref_md5_state_scan.cpp supports raw-word, raw-repeat-word, md5-word, md5-word-times64, md5-repeat-buffer, ansi-rand, ms-rand, and borland-rand. It tests 1..4-byte word seeds, both byte orders where relevant, both counter transitions, and the three digit-width models. It probes early exact anchors, verifies all deterministic anchors on a hit, then reconstructs modeled RSA-270 primes.

A binary was built on September 3 at 20:25:36, but no completed exhaustive run of this new scanner is established in the available handoff context.

**Code caveat (resolved 2026-09-09):** the September 3 build set found=true on an anchor-confirmed state, called BuildRsa270, and ignored the returned product-match boolean, so its exit 0 did not certify anything. The scanner now exits 0 only on product_match=yes, 3 for an anchor-only state hit, and 1 for no match, and `rsaref_md5_state_scan self-test` plants known seeds through the full path including a postincrement carry case. Exact independent multiplication and primality validation are still required after any exit 0.

Do not use seed_scan's hard-coded default targets or default offset 77 as authoritative. They were not substantiated by the later reconstruction. Supply explicit anchors/offsets or use the newer scanner.

## Unresolved predecessor-gap evidence

The former feasibility agent reported unusually small gaps before the known primes relative to a uniform-start upward-scan model: normalized gap sum 41.26 vs a rough length-biased expectation of 68; approximate lower-tail p=8.5e-5 and scan/uniform-prime likelihood ratio ~0.0067.

The basin-size derivation is exact for the old loop: a final accepted prime is weighted by its preceding accepted-prime gap. The statistical interpretation is provisional. The requested matched simulation accounting for range, labels, endpoints, and selection was not delivered in available context. Do not call this a rejection of RSAREF or proof of fresh-candidate sampling. It is a useful bounded next task because it could test whether more absolute-offset seed work is justified.

## Where to continue

1. Read the experiment ledger and avoid treating unrecorded interrupted scans as completed.
2. Rebuild and validate the newest scanner with synthetic known-seed examples, including a postfix carry case, before expanding seed ranges. Ensure its success signal depends on exact target multiplication.
3. Validate the absolute-stream assumptions or test the predecessor-gap evidence with matched controls. High bits alone do not justify exact-source attribution.
4. Recover actual RSA DSP firmware, host driver, manual, or a generation-specific account if possible. Existing archive searches are documented; do not repeat them blindly.
5. Only then choose a bounded, motivated low-entropy family, including both actual and alternative counter transitions. Direct full-state MD5 inversion is not a practical fallback; no practical state-recovery attack was established.
6. Preserve outputs and exact source versions under results/ for every new run.

The original team split was: Mencius / attack_feasibility (source provenance, archive documents, gap model); Boyle / factor_hunt (hardware and software archaeology); Avicenna / structure_attack (exact reconstruction and statistical probes). These are historical roles, not live workers to assume in a new session.

## Preservation status

The core project was copied checksum-identically before this handoff. On September 9 the dataset arithmetic, MD1 vectors, and modeled block inventory were reproduced. No full factorization or expensive seed search was restarted.

Most old /tmp files were absent, including the recovered Lotus DLLs, emulator/disassembly scripts, decoded papers, archive downloads, and raw experiment logs. A surviving Netscape 1.0 directory was copied under references/recovered-temp; its provenance and relevance are limited. Missing artifacts and reacquisition links are recorded in docs/SOURCES.md.

No relevant active factoring/scanner process was observed during handoff. Old PTY IDs, agent IDs, function-store entries, and timer state cannot be transferred as durable execution state. The source and written records are the restart state.

