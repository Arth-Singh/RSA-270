#!/usr/bin/env python3
"""Check the bounded scanner against independent Python hashlib MD5 fixtures."""

import argparse
import hashlib
import json
import subprocess


def encodings():
    result = []
    for width in (1, 2, 3):
        for bit in range(8 * width):
            one = bytearray(width)
            one[bit // 8] = 1 << (bit % 8)
            result.append((bytes(width), bytes(one)))
    return result + [(b"\0", b"\xff"), (b"0", b"1")]


def step(state, endian, counter):
    # This reference models the literal loop by its stopping condition rather
    # than using the C++ scanner's arithmetic or transition helpers.
    state = bytearray(state)
    positions = range(16) if endian == "little" else range(15, -1, -1)
    for position in positions:
        old = state[position]
        state[position] = (old + 1) % 256
        if (old != 0) if counter == "post" else (old != 255):
            break
    return bytes(state)


def md5_stream(state, endian, counter, length):
    result = bytearray()
    while len(result) < length:
        result.extend(hashlib.md5(state).digest())
        state = step(state, endian, counter)
    return bytes(result[:length])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    records = []

    def run(arguments, expected):
        command = [args.binary, *map(str, arguments)]
        completed = subprocess.run(command, text=True, capture_output=True)
        records.append({"command": command, "returncode": completed.returncode,
                        "stdout": completed.stdout, "stderr": completed.stderr,
                        "expected_stdout": expected})
        if completed.returncode != 0 or completed.stdout != expected:
            raise AssertionError("fixture mismatch: " + repr(command))

    counts = (34, 50, 64, 83, 86, 100, 128, 150, 165, 166,
              256, 270, 330, 512, 768, 1000, 1024)
    seed_fixtures = [(i, counts[i % len(counts)], (7*i+3) % (counts[i % len(counts)]+1))
                     for i in range(50)]
    seed_fixtures += [(0, n, w) for n in counts for w in (0, n//2, n)]
    pairs = encodings()
    try:
        for endian in ("big", "little"):
            for counter in ("pre", "post"):
                for index, samples, weight in seed_fixtures:
                    zero, one = pairs[index]
                    h0 = int.from_bytes(hashlib.md5(zero).digest(), endian)
                    h1 = int.from_bytes(hashlib.md5(one).digest(), endian)
                    state = ((samples-weight)*h0 + weight*h1) % 2**128
                    state = state.to_bytes(16, endian)
                    expected = ("initial_state=" + state.hex() + "\nstream="
                                + md5_stream(state, endian, counter, 65).hex() + "\n")
                    run(["stream", "md5", index, samples, weight, endian, counter, 65], expected)
                for state in (bytes(16), bytes([255])*16,
                              bytes.fromhex("0000000000000000000000000000ff00"),
                              bytes.fromhex("00ff0000000000000000000000000000")):
                    run(["counter", state.hex(), endian, counter],
                        step(state, endian, counter).hex() + "\n")
                    run(["state-stream", "md5", state.hex(), endian, counter, 65],
                        md5_stream(state, endian, counter, 65).hex() + "\n")
        # The C++ tests contain recovered MD1 KATs and seeded discovery through
        # both digests; do not silently substitute MD2 as an MD1 reference.
        command = [args.binary, "self-test"]
        completed = subprocess.run(command, text=True, capture_output=True)
        records.append({"command": command, "returncode": completed.returncode,
                        "stdout": completed.stdout, "stderr": completed.stderr})
        assert completed.returncode == 0 and "SELF_TEST PASSED" in completed.stdout
    finally:
        with open(args.output, "w") as handle:
            json.dump({"reference": "Python hashlib MD5 plus independent byte-loop semantics",
                       "records": records}, handle, indent=2)
            handle.write("\n")
    print("HASHLIB_FIXTURES PASSED independent_checks=" + str(len(records)-1))
    print(records[-1]["stdout"], end="")


if __name__ == "__main__":
    main()
