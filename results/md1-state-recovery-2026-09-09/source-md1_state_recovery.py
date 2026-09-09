#!/usr/bin/env python3
"""Bounded full-round MD1 state-recovery feasibility on synthetic public pairs.

The worker process receives public digests only. It does not read the separate
planted-state file or restrict the sixteen symbolic input bytes to a seed family.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import platform
import re
import resource
import shlex
import shutil
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
KATS = (
    (b"", "7ae2a21d8793650044ae40277eafaa2e"),
    (b"abc", "74594a0a73b03503efe62e574dbaf816"),
    (b"a", "420965f2da150b44cbfadd695ab5bbe9"),
    (b"1234567890", "34dbffc3e4a9c0ad2d5559c99649c69e"),
)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + "\n")


def sbox_from_source():
    source = (ROOT / "src/md1.cpp").read_text()
    contents = re.search(r"kSbox\s*=\s*\{(.*?)\};", source, re.S).group(1)
    values = [int(value, 16) for value in re.findall(r"0x[0-9a-fA-F]+", contents)]
    assert len(values) == len(set(values)) == 256
    return values


def reference_digest(message, table):
    """Concrete independent transcription, checked against production C++."""
    state = [0] * 48
    checksum = [0] * 16
    last = index = 0

    def consume(value):
        nonlocal index, last, state
        state[16 + index] = value
        state[32 + index] = state[index] ^ value
        checksum[index] ^= table[value ^ last]
        last = checksum[index]
        index = (index + 1) % 16
        if index == 0:
            rolling = 0
            for _ in range(18):
                for position in range(48):
                    rolling = state[position] ^ table[(rolling + 48 - position) % 256]
                    state[position] = rolling

    for value in message:
        consume(value)
    padding = 16 - index
    for _ in range(padding):
        consume(padding)
    for position in range(16):
        consume(checksum[position])
    return bytes(state[:16])


def reference_postincrement(state):
    shifted = bytes((byte - 1) % 256 for byte in state)
    number = (int.from_bytes(shifted, "big") + 1) % (1 << 128)
    return bytes((byte + 1) % 256 for byte in number.to_bytes(16, "big"))


def prepare(output):
    output.mkdir(parents=True, exist_ok=True)
    if (output / "public-fixtures.json").exists():
        raise ValueError("fixtures already exist; preserve them and use a fresh output directory")
    commands = []
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    oracle = output / "md1_state_oracle"
    command = compiler + ["-O2", "-std=c++20", "-Wall", "-Wextra",
                          str(ROOT / "tests/md1_state_oracle.cpp"),
                          str(ROOT / "src/md1.cpp"), "-o", str(oracle)]
    start = time.monotonic()
    built = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    commands.append({"command": command, "returncode": built.returncode,
                     "stdout": built.stdout, "stderr": built.stderr,
                     "elapsed_seconds": time.monotonic() - start})
    (output / "build.log").write_text(built.stdout + built.stderr)
    built.check_returncode()
    table = sbox_from_source()

    def call(mode, value):
        command = [str(oracle), mode, value.hex()]
        result = subprocess.run(command, text=True, capture_output=True, check=True)
        commands.append({"command": command, "returncode": result.returncode,
                         "stdout": result.stdout, "stderr": result.stderr})
        return result.stdout.strip()

    for message, wanted in KATS:
        assert reference_digest(message, table).hex() == wanted
        assert call("digest", message) == wanted
    counter_vectors = (
        ("00" * 16, "01" * 16),
        ("ff" * 16, "ff" * 15 + "00"),
        ("12" * 14 + "ff00", "12" * 14 + "0001"),
    )
    for before, after in counter_vectors:
        original = bytes.fromhex(before)
        assert reference_postincrement(original).hex() == after
        actual = json.loads(call("pair", original))
        assert actual["next_state"] == after
        assert actual["d0"] == reference_digest(original, table).hex()
        assert actual["d1"] == reference_digest(bytes.fromhex(after), table).hex()

    private, public = [], []
    for index in range(3):
        domain = b"RSA-270 MD1 full-state synthetic fixture 2026-09-09\x00"
        state = hashlib.sha256(domain + index.to_bytes(4, "big")).digest()[:16]
        actual = json.loads(call("pair", state))
        assert actual["d0"] == reference_digest(state, table).hex()
        assert actual["d1"] == reference_digest(reference_postincrement(state), table).hex()
        identifier = "fixture-" + str(index)
        public.append({"id": identifier, "d0": actual["d0"], "d1": actual["d1"],
                       "input_bytes": 16, "counter": "post-be", "rounds_per_block": 18})
        private.append({"id": identifier, "state": state.hex(),
                        "next_state": actual["next_state"]})
    write_json(output / "public-fixtures.json", public)
    write_json(output / "planted-states.json", {
        "generation": "SHA-256(domain || 32-bit big-endian index), first sixteen bytes; "
                      "generated separately and never passed to the solving worker",
        "fixtures": private,
    })
    write_json(output / "production-controls.json", {
        "known_answer_tests_passed": 4, "counter_vectors_passed": 3,
        "full_state_pairs_verified": 3, "commands": commands,
    })
    for name in ("md1.cpp", "md1.hpp"):
        shutil.copy2(ROOT / "src" / name, output / ("source-" + name))
    print("PREPARED production_KATs=4 counter_vectors=3 full_state_pairs=3", flush=True)
    return oracle


def worker(public_path, index, output):
    import z3

    started = time.monotonic()
    fixture = json.loads(public_path.read_text())[index]
    assert fixture["input_bytes"] == 16 and fixture["rounds_per_block"] == 18
    assert fixture["counter"] == "post-be"
    z3.set_param("memory_max_size", 1536)
    z3.set_param("parallel.enable", False)
    z3.set_param("smt.threads", 1)
    z3.set_param("sat.threads", 1)
    solver = z3.SolverFor("QF_AUFBV")
    # Leave one second for native cleanup under the independent 60-second
    # external wall-clock cap. Both limits are recorded explicitly.
    solver.set(timeout=59000, threads=1, random_seed=20260909)
    byte_sort = z3.BitVecSort(8)
    bv = lambda value: z3.BitVecVal(value, 8)
    table = z3.Array("md1_sbox", byte_sort, byte_sort)
    for at, value in enumerate(sbox_from_source()):
        solver.add(z3.Select(table, bv(at)) == bv(value))
    inputs = [z3.BitVec("unknown_state_%02d" % at, 8) for at in range(16)]
    definition_count = 0

    def define(name, value):
        nonlocal definition_count
        variable = z3.BitVec(name, 8)
        solver.add(variable == value)
        definition_count += 1
        return variable

    shifted = z3.Concat(*[value - bv(1) for value in inputs])
    advanced = shifted + z3.BitVecVal(1, 128)
    next_inputs = [define("counter_%02d" % at,
                          z3.Extract(127 - 8 * at, 120 - 8 * at, advanced) + bv(1))
                   for at in range(16)]

    def digest(message, name):
        chain = [bv(0) for _ in range(16)]
        checksum = [bv(0) for _ in range(16)]
        last = bv(0)
        blocks = (message, [bv(16) for _ in range(16)], None)
        for block_index, block in enumerate(blocks):
            # During final(), each checksum position is read before that same
            # position is updated. No later position has yet changed; taking
            # this snapshot is therefore identical to the production loop.
            if block is None:
                block = list(checksum)
            state = list(chain) + list(block) + [a ^ b for a, b in zip(chain, block)]
            for at, value in enumerate(block):
                checksum[at] = define("%s_b%d_c%02d" % (name, block_index, at),
                                      checksum[at] ^ z3.Select(table, value ^ last))
                last = checksum[at]
            rolling = bv(0)
            for round_index in range(18):
                for position in range(48):
                    rolling = define("%s_b%d_r%02d_p%02d" %
                                     (name, block_index, round_index, position),
                                     state[position] ^
                                     z3.Select(table, rolling + bv(48 - position)))
                    state[position] = rolling
            chain = state[:16]
        return chain

    for name, message, observed in (("d0", inputs, fixture["d0"]),
                                    ("d1", next_inputs, fixture["d1"])):
        result = digest(message, name)
        for actual, wanted in zip(result, bytes.fromhex(observed)):
            solver.add(actual == bv(wanted))
    build_seconds = time.monotonic() - started
    model_text = solver.to_smt2()
    model_path = output / "constraints.smt2"
    model_path.write_text("; Full 128-bit unknown state; no planted-state or seed constraint.\n"
                          "; QF_AUFBV; all 256 entries of the 8-bit S-box constrained.\n"
                          "(set-option :timeout 59000)\n" + model_text)
    metadata = {
        "fixture": fixture, "z3_version": z3.get_full_version(),
        "logic": "QF_AUFBV", "input_unknown_bits": 128,
        "input_restrictions": "none; sixteen unconstrained 8-bit variables",
        "digests": 2, "blocks_per_digest": 3, "sweeps_per_block": 18,
        "updates_per_sweep": 48, "sbox_table_entries": 256,
        "sbox_lookup_operations": 2 * 3 * (18 * 48 + 16),
        "byte_definitions": definition_count,
        "assertions": len(solver.assertions()),
        "native_timeout_ms": 59000, "external_timeout_seconds": 60,
        "solver_threads": 1, "z3_memory_max_size_mb": 1536,
        "constraint_build_seconds": build_seconds,
        "constraint_serialization_seconds": time.monotonic() - started - build_seconds,
        "constraints_sha256": sha256(model_path),
        "public_fixtures_sha256": sha256(public_path),
    }
    write_json(output / "model-metadata.json", metadata)
    print("MODEL_BUILT " + json.dumps(metadata), flush=True)
    solve_started = time.monotonic()
    write_json(output / "solve-started.json", {"monotonic": solve_started,
                                               "utc": datetime.now(timezone.utc).isoformat()})
    print("SOLVE_STARTED fixture=" + fixture["id"], flush=True)
    try:
        outcome = solver.check()
        result = {"status": str(outcome), "reason_unknown":
                  solver.reason_unknown() if outcome == z3.unknown else None}
        if outcome == z3.sat:
            model = solver.model()
            state = bytes(model.eval(byte, model_completion=True).as_long() for byte in inputs)
            result["recovered_state"] = state.hex()
            (output / "recovered-model.txt").write_text(str(model) + "\n")
    except z3.Z3Exception as error:
        result = {"status": "unknown", "reason_unknown": "z3_exception", "error": str(error)}
    result.update({"solve_seconds": time.monotonic() - solve_started,
                   "maximum_resident_set_size": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
                   "rss_units": "bytes" if sys.platform == "darwin" else "KiB",
                   "statistics": {key: value for key, value in solver.statistics()},
                   "metadata": metadata})
    write_json(output / "solver-result.json", result)
    (output / "statistics.txt").write_text(str(solver.statistics()) + "\n")
    print("SOLVE_RESULT " + json.dumps(result), flush=True)
    return 0


def run(output):
    oracle = prepare(output)
    dependency = {
        "python": sys.version, "executable": sys.executable, "platform": platform.platform(),
        "z3-solver_distribution_version": importlib.metadata.version("z3-solver"),
        "official_package": "https://pypi.org/project/z3-solver/",
        "official_parameter_documentation": "https://microsoft.github.io/z3guide/programming/Parameters/",
    }
    write_json(output / "dependencies.json", dependency)
    command = [sys.executable, str(Path(__file__).resolve()), "worker",
               "--out", str(output), "--public", str(output / "public-fixtures.json"),
               "--fixture", "0"]
    source_names = ("src/md1.cpp", "src/md1.hpp", "tests/md1_state_oracle.cpp",
                    "scripts/md1_state_recovery.py")
    manifest = {
        "base_commit": subprocess.check_output(["git", "rev-parse", "HEAD"],
                                                cwd=ROOT, text=True).strip(),
        "command": [sys.executable, "scripts/md1_state_recovery.py", "run", "--out", str(output)],
        "worker_command": command, "cwd": str(ROOT),
        "source_sha256": {name: sha256(ROOT / name) for name in source_names},
        "oracle_binary_sha256": sha256(oracle),
        "prepared_fixtures": 3, "requested_solves": 1,
        "external_constraint_build_cap_seconds": 60,
        "memory_cap": "Z3 native hard allocation limit of 1536 MB; this is not an OS RSS limit",
    }
    write_json(output / "RUN.json", manifest)
    announced = False
    with (output / "worker.log").open("w") as log:
        process = subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        worker_started = time.monotonic()
        while process.poll() is None:
            marker = output / "solve-started.json"
            if marker.exists():
                try:
                    solve_start = json.loads(marker.read_text())["monotonic"]
                except json.JSONDecodeError:
                    continue
                if not announced:
                    print("SOLVING fixture-0 full_unknown_bits=128 full_sweeps=18 "
                          "native_timeout_ms=59000 external_cap_seconds=60", flush=True)
                    announced = True
                if time.monotonic() - solve_start >= 60:
                    process.kill()
                    process.wait()
                    if not (output / "solver-result.json").exists():
                        write_json(output / "solver-result.json", {
                            "status": "unknown", "reason_unknown": "external_60_second_wall_timeout",
                            "solve_seconds": time.monotonic() - solve_start,
                            "statistics": None,
                        })
                    break
            elif time.monotonic() - worker_started >= 60:
                process.kill()
                process.wait()
                write_json(output / "solver-result.json", {
                    "status": "not_run", "reason_unknown": "constraint_build_wall_timeout",
                    "constraint_build_seconds": time.monotonic() - worker_started,
                    "solve_seconds": 0, "statistics": None,
                })
                break
            time.sleep(0.05)
        manifest["worker_exit"] = process.wait()
    if not (output / "solver-result.json").exists():
        raise RuntimeError("worker failed before recording a result; see worker.log")
    result = json.loads((output / "solver-result.json").read_text())
    if result["status"] == "sat":
        public = json.loads((output / "public-fixtures.json").read_text())[0]
        recovered = result["recovered_state"]
        validation = subprocess.run([str(oracle), "pair", recovered], text=True,
                                    capture_output=True, check=True)
        actual = json.loads(validation.stdout)
        assert actual["d0"] == public["d0"] and actual["d1"] == public["d1"]
        planted = json.loads((output / "planted-states.json").read_text())["fixtures"][0]["state"]
        write_json(output / "recovered-verification.json", {
            "command": [str(oracle), "pair", recovered], "stdout": validation.stdout,
            "stderr": validation.stderr, "returncode": validation.returncode,
            "both_public_digests_match": True, "matches_planted_state": recovered == planted,
        })
    manifest["result"] = result["status"]
    manifest["reason_unknown"] = result.get("reason_unknown")
    manifest["held_out_fixtures_solved"] = 0
    write_json(output / "RUN.json", manifest)
    print("COMPLETED " + json.dumps({"status": result["status"],
                                      "reason_unknown": result.get("reason_unknown"),
                                      "solve_seconds": result.get("solve_seconds"),
                                      "held_out_solves": 0}), flush=True)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("run", "worker"))
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--public", type=Path)
    parser.add_argument("--fixture", type=int, default=0)
    args = parser.parse_args()
    output = args.out.resolve()
    if args.mode == "worker":
        return worker(args.public.resolve(), args.fixture, output)
    return run(output)


if __name__ == "__main__":
    raise SystemExit(main())
