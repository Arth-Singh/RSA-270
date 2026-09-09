#!/usr/bin/env python3
"""Check exact MD1 structural identities; this is not a preimage attack."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[1]
SOURCE = ROOT / "src/md1.cpp"
SBOX_TEXT = SOURCE.read_text().split("kSbox = {", 1)[1].split("};", 1)[0]
S = [int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]+", SBOX_TEXT)]
assert len(S) == len(set(S)) == 256
INV = [S.index(x) for x in range(256)]


def fixture(label, length):
    return hashlib.shake_256(label.encode("ascii")).digest(length)


def sweep(x, initial):
    y = list(x)
    t = initial
    for j in range(48):
        t = y[j] ^ S[(t + 48 - j) & 255]
        y[j] = t
    return bytes(y)


def p(x):
    return sweep(x, x[47])


def inverse_p(y):
    x = [0] * 48
    for j in range(47, 0, -1):
        x[j] = y[j] ^ S[(y[j - 1] + 48 - j) & 255]
    x[0] = y[0] ^ S[(x[47] + 48) & 255]
    return bytes(x)


def q(x):
    return sweep(x, 0)


def inverse_q(y):
    x = list(inverse_p(y))
    x[0] = y[0] ^ S[48]
    return bytes(x)


def initial_correction(x):
    y = bytearray(x)
    y[0] ^= S[48] ^ S[(x[47] + 48) & 255]
    return bytes(y)


def compression(h, m, rounds=18):
    x = bytes(h) + bytes(m) + bytes(a ^ b for a, b in zip(h, m))
    x = q(x)
    for _ in range(rounds - 1):
        x = p(x)
    return x[:16]


def checksum_update(c, m):
    c = list(c)
    last = c[-1]
    for i, value in enumerate(m):
        c[i] ^= S[value ^ last]
        last = c[i]
    return bytes(c)


def checksum16(m):
    return checksum_update(checksum_update(bytes(16), m), bytes([16] * 16))


def inverse_checksum16(k):
    c = [0] * 16
    for i in range(1, 16):
        c[i] = k[i] ^ S[16 ^ k[i - 1]]
    c[0] = k[0] ^ S[16 ^ c[15]]
    return bytes(INV[c[i]] ^ (c[i - 1] if i else 0) for i in range(16))


def md1(message):
    padding = 16 - len(message) % 16
    padded = message + bytes([padding] * padding)
    h = c = bytes(16)
    for offset in range(0, len(padded), 16):
        m = padded[offset:offset + 16]
        h = compression(h, m)
        c = checksum_update(c, m)
    return compression(h, c)


def main():
    for i in range(512):
        x = fixture("MD1-full-sweep-%d" % i, 48)
        assert inverse_p(p(x)) == x
        assert p(inverse_p(x)) == x
        assert inverse_q(q(x)) == x
        assert q(inverse_q(x)) == x
        assert q(x) == p(initial_correction(x))
    for i in range(512):
        m = fixture("MD1-checksum-message-%d" % i, 16)
        k = fixture("MD1-checksum-output-%d" % i, 16)
        assert inverse_checksum16(checksum16(m)) == m
        assert checksum16(inverse_checksum16(k)) == k

    kats = [
        (b"", "7ae2a21d8793650044ae40277eafaa2e"),
        (b"abc", "74594a0a73b03503efe62e574dbaf816"),
        (b"a", "420965f2da150b44cbfadd695ab5bbe9"),
        (b"1234567890", "34dbffc3e4a9c0ad2d5559c99649c69e"),
    ]
    inputs = [x[0] for x in kats]
    inputs += [fixture("MD1-digest-control-%d" % i, 16) for i in range(128)]
    inputs += [fixture("MD1-padding-control-%d" % i, i) for i in [15, 17, 31, 32, 33, 48, 64]]
    expected = [md1(x).hex() for x in inputs]
    assert expected[:4] == [x[1] for x in kats]
    payload = "".join(x.hex() + "\n" for x in inputs)
    (OUT / "digest-inputs.txt").write_text(payload)
    compiled = subprocess.run(
        [str(OUT / "build/md1_hex")], input=payload, text=True, capture_output=True, check=True
    )
    (OUT / "production-digests.txt").write_text(compiled.stdout)
    (OUT / "production-stderr.txt").write_text(compiled.stderr)
    assert compiled.stdout.splitlines() == expected
    for m in inputs[4:132]:
        h1 = compression(bytes(16), m)
        h2 = compression(h1, bytes([16] * 16))
        assert compression(h2, checksum16(m)) == md1(m)

    # This illustrates a reduced-round bottleneck only, not the full hash.
    # The first 16 output bytes of Q depend only on the incoming chaining value.
    h = fixture("MD1-fixed-chaining-value", 16)
    first_outputs = set()
    two_round_outputs = set()
    boundary_to_output = {}
    for i in range(4096):
        m = fixture("MD1-reduced-round-control-%d" % i, 16)
        x = h + m + bytes(a ^ b for a, b in zip(h, m))
        y = q(x)
        z = p(y)
        first_outputs.add(y[:16])
        two_round_outputs.add(z[:16])
        if y[47] in boundary_to_output:
            assert boundary_to_output[y[47]] == z[:16]
        boundary_to_output[y[47]] = z[:16]
    assert len(first_outputs) == 1
    assert len(two_round_outputs) <= 256

    report = {
        "purpose": "Verify structural identities in the preserved MD1 implementation; no state recovery or factorization.",
        "sbox_is_permutation": True,
        "full_48_byte_sweep_fixtures": 512,
        "sweep_identities_per_fixture": 5,
        "checksum_bijection_fixtures_each_direction": 512,
        "known_answer_tests": 4,
        "independent_python_cpp_digest_controls": len(inputs),
        "exact_16_byte_three_block_compositions": 128,
        "reduced_round_messages": 4096,
        "one_sweep_distinct_first16": len(first_outputs),
        "two_sweep_distinct_first16": len(two_round_outputs),
        "limitations": [
            "Full-sweep inverse needs all 48 output bytes; the digest exposes 16.",
            "The reduced-round observation supplies no full-18-sweep preimage method.",
            "Checksum inversion requires all 16 checksum bytes, which are not exposed by the digest.",
            "No source establishes that RSA-270 used this MD1 implementation.",
        ],
        "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
        "script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    }
    (OUT / "structural-results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
