#!/usr/bin/env python3
"""Validate saved full-round constraints with a known witness, not recovery."""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--worker", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not args.worker:
        command = [sys.executable, str(Path(__file__).resolve()), str(output), "--worker"]
        started = time.monotonic()
        try:
            run = subprocess.run(command, text=True, capture_output=True, timeout=15)
            record = {"command": command, "returncode": run.returncode,
                      "stdout": run.stdout, "stderr": run.stderr,
                      "process_seconds": time.monotonic() - started}
        except subprocess.TimeoutExpired as error:
            record = {"command": command, "returncode": None,
                      "stdout": (error.stdout or b"").decode(),
                      "stderr": (error.stderr or b"").decode(),
                      "process_seconds": time.monotonic() - started,
                      "status": "unknown", "reason": "external15s validation cap"}
        (output / "known-state-validation-run.json").write_text(json.dumps(record, indent=2) + "\n")
        (output / "known-state-validation.log").write_text(record["stdout"] + record["stderr"])
        print(json.dumps(record, indent=2))
        return 0

    import z3
    z3.set_param("memory_max_size", 1536)
    z3.set_param("parallel.enable", False)
    z3.set_param("smt.threads", 1)
    z3.set_param("sat.threads", 1)
    model_path = output / "constraints.smt2"
    original_hash = hashlib.sha256(model_path.read_bytes()).hexdigest()
    started = time.monotonic()
    solver = z3.SolverFor("QF_AUFBV")
    solver.from_file(str(model_path))
    private = json.loads((output / "planted-states.json").read_text())["fixtures"][0]
    state = bytes.fromhex(private["state"])
    for at, value in enumerate(state):
        solver.add(z3.BitVec("unknown_state_%02d" % at, 8) == z3.BitVecVal(value, 8))
    solver.set(timeout=14000, threads=1)
    bound_path = output / "known-state-constraints.smt2"
    bound_path.write_text("; Known witness validation only; not a state recovery.\n" + solver.to_smt2())
    build_seconds = time.monotonic() - started
    print("KNOWN_STATE_VALIDATION_STARTED build_seconds=" + str(build_seconds), flush=True)
    started = time.monotonic()
    outcome = solver.check()
    record = {
        "status": str(outcome), "known_state_supplied": True,
        "counts_as_recovery": False, "fixture": private["id"],
        "original_constraints_sha256": original_hash,
        "bound_constraints_sha256": hashlib.sha256(bound_path.read_bytes()).hexdigest(),
        "assertions_after_binding": len(solver.assertions()),
        "input_equalities_added": 16, "build_seconds": build_seconds,
        "solve_seconds": time.monotonic() - started,
        "reason_unknown": solver.reason_unknown() if outcome == z3.unknown else None,
        "statistics": {key: value for key, value in solver.statistics()},
        "rss": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "rss_units": "bytes" if sys.platform == "darwin" else "KiB",
        "native_timeout_ms": 14000, "external_process_cap_seconds": 15,
    }
    assert hashlib.sha256(model_path.read_bytes()).hexdigest() == original_hash
    (output / "known-state-validation.json").write_text(json.dumps(record, indent=2) + "\n")
    print("KNOWN_STATE_VALIDATION_RESULT " + json.dumps(record), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
