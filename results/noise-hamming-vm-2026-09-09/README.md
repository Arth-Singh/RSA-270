# Complete additive-noise scan on the VM

The full run completed on 2026-09-09 at 08:35:49 UTC with **zero fingerprint,
interior-pattern, candidate-interval, or fresh-candidate hits**. No RSA-270
factor or usable generator state was found.

The scanner evaluated 2,081,200 configured streams, each exactly 16,384 bytes,
using 128 VM threads. It examined 34,083,812,400 eight-byte windows against
1,404 prepared patterns from all 34 known factors. Only starts for which the
whole candidate buffer fits in the stream are eligible. There is no shared
offset or generation-order requirement across the known factors.

The configured family is two digests (MD5 and recovered MD1), 50 binary-sample
encodings, 17 sample counts with every Hamming weight, two additive arithmetic
orders, and both counter transitions. Different configurations can duplicate
states; the count is not distinct entropy coverage. Candidate packing includes
both integer byte orders, the documented byte/word/24-bit lane permutations,
minimal and modeled padded buffers, and top-bit/parity formatting. Masks cover
the exact old-predicate predecessor interval and therefore also include a
fresh candidate equal to the known factor.

## Commands and measurements

The exact build, compiler, source/data hashes, binary hash, and command sequence
are in `RUN.log` and the copied `run_noise_vm.sh`. Source snapshots are included.

```sh
build/current/noise_hamming_scan self-test
timeout 60 build/current/noise_hamming_scan scan --digest md1 --end 65536 --threads 128 --stream-bytes 16384
timeout 600 build/current/noise_hamming_scan scan --digest both --threads 128 --stream-bytes 16384
```

| Phase | Streams | Stream bytes | Elapsed scanner time | Result |
| --- | ---: | ---: | ---: | --- |
| MD1 throughput pilot | 65,536 | 16,384 | 6.39831 s | Zero hits |
| Full MD5/MD1 family | 2,081,200 | 16,384 | 50.0564 s | Zero hits |

The pilot overlaps the full run and adds no separate coverage. The full run's
aggregate rate includes faster MD5 work and should not be applied to an
MD1-only job. Both commands exited 0 with their complete expected counts.

The 16 KiB bound was chosen after the gap controls weakened the fixed global
stream model. Under a rough fresh-candidate model, a first RSA-100 accepted
candidate can require hundreds of candidate draws, putting it beyond a 2 KiB
window. The longer bound is a sensitivity choice, not a recovered historical
offset or a guarantee that the real first prime falls within it.

The VM self-test passed 1,008 seeded packing controls, eight full enumeration
controls, six digest known answers, six carry cases, and two classification
controls. The independent Python MD5 fixtures and local pilot are retained in
`results/noise-hamming-2026-09-09/`. Scanner source SHA-256 is
`48188e1f0e5b39270c7de1f537e3c4063b0a4ee8e3c2f534d34c8db832bd0c6a`.

## Limits and decision

The initial state starts at zero and adds a hash for each individual binary
sample. There is no extra timestamp, unmodeled seed material, or reseeding.
Association with RSA DSP remains unproved. The source's 256-byte readiness gate
is deliberately not enforced, so short sample totals are hypothetical states
rather than complete source-exact initialization sequences. Endian/counter
alternatives and the MD1 substitution are explicitly conditional.

The result excludes only the implemented finite family and offsets under the
stated candidate packings. In particular, lane permutations do not implement
the signed 24-bit digit representation mentioned in the DSP paper. A small
representation sensitivity audit should establish whether that detail yields
different well-defined patterns before it motivates another search.

Raw output and source are preserved even though no hit occurred. A future hit
to a known factor would require independent validation and further work to
reconstruct RSA-270; this scanner's exit codes never certify factorization.
