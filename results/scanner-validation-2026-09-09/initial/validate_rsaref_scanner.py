#!/usr/bin/env python3
"""Generate independent RSAREF scanner fixtures, build a harness, and retain logs.

This uses Python hashlib, integer arithmetic, and a counter conjugation instead
of the production C++ helpers. It verifies the conditional scanner model, not
the historical attribution of that model to the RSA challenge generator.
"""

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import sys
import time


RSA270 = int(
    "233108530344407544527637656910680524145619812480305449042948611968495918245135782867888369318577116418213919268572658314913060672626911354027609793166341626693946596196427744273886601876896313468704059066746903123910748277606548649151920812699309766587514735456594993207"
)
WORD_MODES = (
    "raw-word", "raw-repeat-word", "md5-word", "md5-word-times64",
    "md5-repeat-buffer",
)
LCGS = {
    "ansi-rand": (1103515245, 12345),
    "ms-rand": (214013, 2531011),
    "borland-rand": (22695477, 1),
}
MASK128 = (1 << 128) - 1


def md5(data):
    return hashlib.md5(data).digest()


def advance(state, steps, transition):
    """Advance in one integer operation; no production byte-carry loop is used.

    For the postfix transition, subtract one from each byte modulo 256.
    This changes its old-byte-zero carry condition into an ordinary carry from
    255. Increment the resulting big-endian integer, then undo the byte map.
    """
    if transition == "post":
        state = bytes((byte - 1) % 256 for byte in state)
    number = (int.from_bytes(state, "big") + steps) & MASK128
    result = number.to_bytes(16, "big")
    if transition == "post":
        result = bytes((byte + 1) % 256 for byte in result)
    return result


def stream(state, transition, length):
    # The production function returns complete digest blocks, including the
    # unused tail of the final block. Match that documented interface here.
    return b"".join(md5(advance(state, i, transition))
                    for i in range((length + 15) // 16))


def initial_states(mode, candidate, word_bytes):
    if mode in LCGS:
        multiplier, increment = LCGS[mode]
        word = candidate
        buffer = bytearray()
        for _ in range(256):
            word = (multiplier * word + increment) % (1 << 32)
            buffer.append((word // (1 << 16)) % 256)
        return [(mode, md5(buffer))]
    result = []
    for order, suffix in (("big", "be"), ("little", "le")):
        word = candidate.to_bytes(word_bytes, order)
        name = mode + "-" + suffix
        if mode == "raw-word":
            for position in range(17 - word_bytes):
                result.append((name + "-at" + str(position),
                               bytes(position) + word + bytes(16 - position - word_bytes)))
        elif mode == "raw-repeat-word":
            result.append((name, (word * 16)[:16]))
        elif mode == "md5-word":
            result.append((name, md5(word)))
        elif mode == "md5-word-times64":
            result.append((name, ((int.from_bytes(md5(word), "big") * 64)
                                   & MASK128).to_bytes(16, "big")))
        elif mode == "md5-repeat-buffer":
            result.append((name, md5((word * 256)[:256])))
        else:
            raise ValueError("unknown mode " + mode)
    return result


def accepted(value):
    # This is the preserved Fermat predicate, deliberately not a modern
    # primality test. For example, the composite 8321 must be accepted.
    return (value % 3 == 2 and all(value % p for p in (5, 7, 11))
            and pow(2, value, value) == 2)


def prime_from_raw(raw, bits):
    # Enumerate the cyclic odd-candidate range by its index, independently of
    # the production loop's subtract-range-then-add-two wrap implementation.
    span = 1 << (bits - 2)
    lower = 3 * span
    first_odd_index = (raw % span) // 2
    for distance in range(span // 2):
        value = lower + 1 + 2 * ((first_odd_index + distance) % (span // 2))
        if accepted(value):
            return value
    raise ValueError("no accepted value in prime range")


def geometry(modulus_bits, width):
    whole_digits = (modulus_bits + width - 1) // width
    return ((whole_digits + 1) // 2) * width // 8


def read_models(root):
    source = (root / "src/rsaref_md5_state_scan.cpp").read_text()
    models = {}
    expression = r'\{(16|24|32), (\d+), (\d+),\s*\{(.*?)\}\}\}'
    for match in re.finditer(expression, source, re.S):
        width, offset, block_bytes = map(int, match.group(1, 2, 3))
        anchors = [(int(i), bytes.fromhex(value)) for i, value in re.findall(
            r'\{(\d+), Hex\("([0-9a-f]+)"\)\}', "{" + match.group(4) + "}")]
        models[width] = (offset, block_bytes, anchors)
    assert set(models) == {16, 24, 32}
    assert [len(models[width][2]) for width in (16, 24, 32)] == [23, 22, 18]
    return models


def verify_real_anchors(root, models, emit, report):
    with (root / "data/solved_decimal_challenge.tsv").open(newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    assert len(rows) == 17
    recoveries = []
    ambiguous = []
    for row_index, row in enumerate(rows):
        n = int(row["modulus"])
        factors = (int(row["factor1"]), int(row["factor2"]))
        assert factors[0] * factors[1] == n
        assert sorted(p.bit_length() for p in factors) == [n.bit_length() // 2,
                                                         (n.bit_length() + 1) // 2]
        if factors[0].bit_length() == factors[1].bit_length():
            ambiguous.append(row_index)
        recovered = []
        for p in factors:
            assert accepted(p)
            b = p.bit_length()
            span = 1 << (b - 2)
            previous = p - 2
            while not accepted(previous):
                previous -= 2
            low, high = previous - 3 * span + 1, p - 3 * span
            assert 0 <= low <= high < span
            varying = (low ^ high).bit_length()
            mask = (span - 1) ^ ((1 << varying) - 1)
            recovered.append((low & mask, mask))
        recoveries.append((n.bit_length(), factors, recovered))
    assert len(ambiguous) == 9
    ambiguity_position = {row: i for i, row in enumerate(ambiguous)}
    for width in (16, 24, 32):
        offset, size, anchors = models[width]
        assert offset == sum(2 * geometry(bits, width) for bits, _, _ in recoveries)
        assert size == geometry(RSA270.bit_length(), width)
        exact_values = {}
        for choice in range(512):
            value = mask = bit_length = 0
            for row_index, (bits, factors, residues) in enumerate(recoveries):
                byte_length = geometry(bits, width)
                if row_index in ambiguity_position:
                    swap = (choice >> ambiguity_position[row_index]) & 1
                    order = (1, 0) if swap else (0, 1)
                else:
                    order = tuple(sorted((0, 1),
                                         key=lambda i: factors[i].bit_length(), reverse=True))
                for index in order:
                    value = (value << (8 * byte_length)) | residues[index][0]
                    mask = (mask << (8 * byte_length)) | residues[index][1]
                    bit_length += 8 * byte_length
            for index, expected in anchors:
                shift = bit_length - 128 * (index + 1)
                assert ((mask >> shift) & MASK128) == MASK128
                actual = ((value >> shift) & MASK128).to_bytes(16, "big")
                assert actual == expected, (width, choice, index)
                exact_values[index] = actual
        emit("REAL_WIDTH", width, offset, size, len(anchors))
        for index, _ in anchors:
            emit("REAL_ANCHOR", width, index, exact_values[index].hex())
        report(f"reference real_anchors width={width} anchors={len(anchors)} "
               "orders=512 result=PASS")


def generate_fixtures(root, output):
    messages = []
    lines = ["# Independent Python reference fixtures; see RUN.json for source hashes."]
    emit = lambda *values: lines.append("\t".join(map(str, values)))
    report = lambda message: messages.append(message)
    emit("TARGET", RSA270)

    # These constants check the conjugation itself against explicit raw-source
    # transitions before it is used to generate any expected outputs.
    explicit = [
        ("post", "00" * 16, "01" * 16),
        ("pre", "00" * 16, "00" * 15 + "01"),
        ("post", "ff" * 16, "ff" * 15 + "00"),
        ("pre", "ff" * 16, "00" * 16),
        ("post", "12" * 14 + "ff00", "12" * 14 + "0001"),
        ("pre", "12" * 14 + "ffff", "12" * 13 + "130000"),
    ]
    for transition, before, after in explicit:
        assert advance(bytes.fromhex(before), 1, transition).hex() == after
        emit("STEP", transition, before, 1, after)
    report("reference explicit_counter_vectors=6 result=PASS")

    mode_groups = []
    for mode in WORD_MODES:
        for word_bytes in (1, 2, 3, 4):
            largest = (1 << (8 * word_bytes)) - 1
            for candidate in (0, largest // 3, largest):
                mode_groups.append((mode, candidate, word_bytes))
    for mode in LCGS:
        for candidate in (0, 123456, (1 << 32) - 1):
            mode_groups.append((mode, candidate, 4))
        # Independent high-byte-period witnesses also become production
        # InitialStates fixtures. All 256 aliases share these same outputs.
        expected = initial_states(mode, 123456, 4)
        for high_byte in (1, 17, 255):
            candidate = 123456 + high_byte * (1 << 24)
            assert initial_states(mode, candidate, 4) == expected
            mode_groups.append((mode, candidate, 4))
        report(f"reference lcg_alias mode={mode} base=123456 stride=16777216 "
               f"state={expected[0][1].hex()} result=PASS")
    # 60 word groups, 18 LCG groups, and nine endpoint pairs chosen below.
    for mode in LCGS:
        for candidate in ((1 << 24) - 1, 1 << 24, (1 << 24) + 1):
            mode_groups.append((mode, candidate, 4))
    for mode, candidate, word_bytes in mode_groups:
        states = initial_states(mode, candidate, word_bytes)
        emit("INITSET", mode, candidate, word_bytes, len(states))
        for name, state in states:
            emit("INIT", mode, candidate, word_bytes, name, state.hex())
    assert len(mode_groups) == 87
    report("reference initial_state_groups=87 modes=8 word_widths=1,2,3,4 result=PASS")

    states = [bytes(16), bytes([255]) * 16, bytes(range(16)),
              bytes.fromhex("1234abcd" + "00" * 12),
              bytes.fromhex("12" * 14 + "ff00"),
              bytes.fromhex("12" * 14 + "ffff")]
    for state in states:
        for transition in ("post", "pre"):
            for steps in (0, 1, 2, 6, 8, 74, 90, 256, 257, 513):
                emit("STEP", transition, state.hex(), steps,
                     advance(state, steps, transition).hex())
    for state in states[:3]:
        for transition in ("post", "pre"):
            for length in (1, 15, 16, 17, 31, 32, 33, 1440):
                emit("STREAM", transition, state.hex(), length,
                     stream(state, transition, length).hex())
    for value in (3, 5, 7, 11, 17, 19, 23, 341, 561, 2047, 8321, 65537):
        emit("ACCEPT", value, int(accepted(value)))
    assert accepted(8321) and 8321 == 53 * 157
    for bits in (8, 10, 16):
        span = 1 << (bits - 2)
        for raw in (0, 1, 2, span - 2, span - 1, span, 3 * span + 17):
            emit("PRIME", raw, bits, prime_from_raw(raw, bits))

    models = read_models(root)
    verify_real_anchors(root, models, emit, report)
    for width, mode, first_candidate, name in (
            (16, "raw-word", 0x1234abcd, "raw-word-be-at0"),
            (24, "md5-word", 0x00c0ffee, "md5-word-le"),
            (32, "ansi-rand", 0x00123456, "ansi-rand")):
        candidate = first_candidate
        for _ in range(4096):
            state = dict(initial_states(mode, candidate, 4))[name]
            # Force a real carry early enough that the two transitions differ
            # at the first anchor, including for the hashed-state families.
            if state[-1] == 0:
                break
            candidate += 1
        else:
            raise ValueError("could not find bounded carry control")
        for transition in ("post", "pre"):
            identity = f"w{width}-{transition}"
            offset, size, _ = models[width]
            output_stream = stream(state, transition, offset + 2 * size)
            raw_p = int.from_bytes(output_stream[offset:offset + size], "big")
            raw_q = int.from_bytes(output_stream[offset + size:offset + 2 * size], "big")
            p = prime_from_raw(raw_p, 448)
            q = prime_from_raw(raw_q, 447)
            emit("CONTROL", identity, mode, candidate, 4, transition, width,
                 state.hex(), 448, 447, p * q, p, q)
            for other_width in (16, 24, 32):
                other_offset, other_size, anchors = models[other_width]
                emit("CONTROL_WIDTH", identity, other_width, other_offset, other_size)
                for position, (index, _) in enumerate(anchors):
                    digest = md5(advance(state, index, transition))
                    if other_width != width and position == 0:
                        digest = bytes([digest[0] ^ 1]) + digest[1:]
                    emit("CONTROL_ANCHOR", identity, other_width, index, digest.hex())
            report(f"reference control={identity} mode={mode} candidate={candidate} "
                   f"state={state.hex()} p={p} q={q}")
    fixture = output / "reference-fixtures.tsv"
    fixture.write_text("\n".join(lines) + "\n")
    (output / "reference.log").write_text("\n".join(messages) + "\n")
    for message in messages:
        print(message, flush=True)
    return fixture


def digest_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=False)
    fixture = generate_fixtures(root, output)
    executable = output / "scanner-control-harness"
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    command = compiler + ["-O2", "-std=c++20", "-Wall", "-Wextra",
                          "-Wno-deprecated-declarations"]
    if platform.system() == "Darwin":
        for prefix in (os.environ.get("GMP_PREFIX", "/opt/homebrew/opt/gmp"),
                       os.environ.get("SSL_PREFIX", "/opt/homebrew/opt/openssl@3")):
            command += ["-I" + prefix + "/include", "-L" + prefix + "/lib"]
    command += [str(root / "tests/rsaref_scanner_control_harness.cpp"),
                "-lgmpxx", "-lgmp", "-lcrypto", "-pthread", "-o", str(executable)]
    version = subprocess.run(compiler + ["--version"], text=True,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True)
    (output / "compiler.txt").write_text(version.stdout)
    manifest = {
        "base_commit": subprocess.check_output(["git", "rev-parse", "HEAD"],
                                                cwd=root, text=True).strip(),
        "python": sys.version,
        "platform": platform.platform(),
        "compile_command": command,
        "harness_command": [str(executable), str(fixture)],
        "reproduce_command": ["python3", "scripts/validate_rsaref_scanner.py",
                              "--out", "results/scanner-validation-reproduction"],
        "threads": 2,
        "source_sha256": {
            name: digest_file(root / name) for name in (
                "src/rsaref_md5_state_scan.cpp", "scripts/validate_rsaref_scanner.py",
                "tests/rsaref_scanner_control_harness.cpp",
                "data/solved_decimal_challenge.tsv", "references/rsaref/raw_r_random.c",
                "references/rsaref/raw_prime-old.c")},
        "fixture_sha256": digest_file(fixture),
    }
    started = time.monotonic()
    with (output / "build.log").open("w") as log:
        built = subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT)
    manifest["compile_exit"] = built.returncode
    manifest["compile_seconds"] = time.monotonic() - started
    if built.returncode == 0:
        manifest["binary_sha256"] = digest_file(executable)
        started = time.monotonic()
        with (output / "harness.log").open("w") as log:
            checked = subprocess.run(manifest["harness_command"], cwd=root,
                                     stdout=log, stderr=subprocess.STDOUT)
        manifest["harness_exit"] = checked.returncode
        manifest["harness_seconds"] = time.monotonic() - started
        returncode = checked.returncode
        print((output / "harness.log").read_text(), end="")
    else:
        returncode = built.returncode
        print((output / "build.log").read_text(), end="")
    (output / "RUN.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"validation_exit={returncode} output={output}", flush=True)
    return returncode


if __name__ == "__main__":
    raise SystemExit(main())
