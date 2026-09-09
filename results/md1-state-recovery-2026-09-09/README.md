# Full-state MD1 SMT feasibility experiment

The blind experiment returned **unknown**. No 128-bit state was recovered and
no SAT or UNSAT conclusion was obtained. One full-round fixture was attempted;
the other two generated fixtures were not submitted to the solver.

The external watchdog stopped the first worker approximately **58.64 seconds**
after its solve marker, within the requested 60-second bound. The original
controller printed `60.0311455` seconds because this runtime exposes different
process-relative origins for `time.monotonic()`. Cross-process subtraction was
invalid. `clock-audit.json` preserves the diagnosis, original output, and the
absolute solve-marker/result-file timestamps. The committed runner now uses
the parent's own clock for its watchdog. No second blind run was performed.

Afterward, a separate known-state validation parsed the unchanged blind
constraint file, supplied the planted sixteen input bytes, and returned
**SAT in 2.279 seconds**. This validates the symbolic encoding for that witness;
the witness was supplied and therefore this result is not state recovery.

| Phase | Measured result |
| --- | --- |
| Production C++ controls | Four MD1 known answers, three explicit postfix carry vectors, and all three planted digest pairs passed |
| Blind constraint construction | 0.289 seconds |
| Blind constraint serialization | 0.037 seconds |
| Blind solving | Unknown; approximately 58.64 seconds between solve marker and result-file creation |
| Separate known-state parse/build | 0.073 seconds |
| Separate known-state solving | SAT, 2.279 seconds |
| Known-state peak solver memory | 617.35 MB reported by Z3; process peak RSS 682,196,992 bytes |

## Exact problem given to the blind solver

The worker received only the first public fixture's two output digests:

```text
D0 = 52da50bc6157624e2f1c8ffd682c06e2
D1 = c354eb72a673cfa9297c0487fde4fe1b
D0 = MD1(S)
D1 = MD1(postincrement_big_endian(S))
```

All sixteen bytes of `S` are unrestricted 8-bit variables. No seed family,
timestamp, Hamming-weight restriction, fixed input prefix, or planted-state
equality appears in the blind constraints. The process command refers only to
`public-fixtures.json`. The independently generated states are preserved in
`planted-states.json` for validation, but the blind worker never reads that file.

Each digest models a sixteen-byte input block, a full sixteen-byte padding
block, and the checksum block. Every block includes all **18 sweeps of 48 byte
updates** and the checksum updates from the preserved implementation. The model
contains two complete digests, 5,280 S-box lookups, 5,296 intermediate byte
definitions, and 5,584 assertions. The S-box is an 8-bit-indexed array with all
256 entries constrained. The logic is `QF_AUFBV`.

The counter is modeled by subtracting one from each byte modulo 256, adding one
to the concatenated big-endian 128-bit integer, and adding one back to each
byte. This is exactly the preserved unusual postfix carry condition. The C++
oracle implements the literal byte loop independently. No ordinary-counter
experiment was added.

The source-exact finalization loop reads each checksum byte before updating
that position. The symbolic model snapshots the checksum at the start of its
final block; subsequent checksum updates cannot change later input positions
before they are read. This preserves the production behavior.

## Bounds and provenance

The run used [the official `z3-solver` package](https://pypi.org/project/z3-solver/),
version **5.1.0.0**, installed only in the worktree's local `results/.venv`.
Installation logs and `dependencies.json` are retained. The first pip command
failed because the installed pip 21.2.4 lacked `--report`; the retry removed that
reporting option and installed the same pinned package without upgrading pip.

Z3 was configured for one thread, a native 59,000 ms timeout, and a native
allocation limit of 1,536 MB. The controller separately bounded constraint
construction and solving. The allocation parameter is documented in
[Z3's parameter reference](https://microsoft.github.io/z3guide/programming/Parameters/);
it is not an operating-system RSS cap. No VM was used. The known-state
validation used a 14,000 ms native timeout inside a 15-second process cap.

The blind worker was externally terminated before its native 59-second timeout
elapsed, owing to the conservative clock-origin mistake described above. It
did not return solver statistics or a model. Their absence is explicitly
recorded rather than treated as evidence of memory exhaustion or native
cancellation failure.

`constraints.smt2` preserves the exact blind formula, SHA-256:

```text
cc066bcf5de42d98345ca76ea326b03674dc8cff55143c17218209a0204a9331
```

`model-metadata.json`, `solver-result.json`, `worker.log`, and `run-output.txt`
are the original blind artifacts. `known-state-constraints.smt2` and the
`known-state-validation*` files are separate validation artifacts; they add
exactly sixteen input equalities. `RUN.json` records commands and source hashes.
The original executed runner is preserved as `source-md1_state_recovery.py`;
the committed runner includes only the subsequent watchdog correction.
`source-map.json` records that distinction. `SHA256SUMS` covers retained text
evidence. The compiled oracle and local virtual environments are not committed.

## Reproduction

Use a fresh output directory; the runner refuses to overwrite existing fixtures.

```sh
python3 -m venv results/.venv
results/.venv/bin/python -m pip install --index-url https://pypi.org/simple --only-binary=:all: z3-solver==5.1.0.0
results/.venv/bin/python scripts/md1_state_recovery.py run --out results/md1-state-recovery-reproduction
results/.venv/bin/python scripts/validate_md1_smt_model.py results/md1-state-recovery-reproduction
```

The second command sequence performs one blind fixture and, separately, one
known-state validation. The corrected watchdog may permit slightly more solving
time than the archived run while remaining below the requested one-minute cap.

## Limits

This is one bounded test of one direct SMT encoding with one solver. Unknown
does not establish MD1 preimage hardness, rule out a stronger encoding, or
constitute a cryptanalytic result. Known-state SAT checks only one witness and
must never be reported as a recovered state. No successful held-out recovery
was demonstrated. The preserved Lotus MD1 implementation also remains an
unverified hypothesis for the original RSA DSP challenge generator.
