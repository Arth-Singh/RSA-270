#!/usr/bin/env python3
"""Analyze signed 24-bit storage without changing a scanner or testing seeds."""
import csv
import hashlib
import itertools
import json
import random
from pathlib import Path

B = 1 << 24
H = B // 2
OUT = Path(__file__).resolve().parent
REPO = OUT.parents[1]


def encode_centered(value, width=None):
    """Canonical balanced digits, obtained by centered Euclidean division."""
    words = []
    while value:
        value, remainder = divmod(value + H, B)
        words.append((remainder - H) % B)
    words = words or [0]
    if width is not None:
        assert len(words) <= width
        words += [0] * (width - len(words))
    return words


def encode_rebalanced(value, width=None):
    """Independent method: ordinary digits followed by explicit carry/borrow."""
    sign = -1 if value < 0 else 1
    value = abs(value)
    digits = []
    while value:
        value, digit = divmod(value, B)
        digits.append(sign * digit)
    digits = digits or [0]
    i = 0
    while i < len(digits):
        carry = 0
        if digits[i] >= H:
            digits[i] -= B
            carry = 1
        elif digits[i] < -H:
            digits[i] += B
            carry = -1
        if carry:
            if i + 1 == len(digits):
                digits.append(0)
            digits[i+1] += carry
        i += 1
    while len(digits) > 1 and digits[-1] == 0:
        digits.pop()
    words = [d % B for d in digits]
    if width is not None:
        assert len(words) <= width
        words += [0] * (width - len(words))
    return words


def decode_horner(words):
    value = 0
    for word in reversed(words):
        value = value * B + (word if word < H else word - B)
    return value


def concat(words):
    return sum(word << (24*i) for i, word in enumerate(words))


def decode_correction(words):
    return concat(words) - sum(B << (24*i) for i, word in enumerate(words) if word >= H)


def accepts(n):
    return n > 11 and n % 2 == 1 and n % 3 == 2 and all(n % d for d in (5, 7, 11)) and pow(2, n, n) == 2


def previous(n):
    p = n - 2
    while not accepts(p):
        p -= 2
    return p


def consensus(values, bits):
    and_bits = (1 << bits) - 1
    or_bits = 0
    for value in values:
        and_bits &= value
        or_bits |= value
    mask = ((1 << bits) - 1) ^ (and_bits ^ or_bits)
    return mask, and_bits & mask


def permute_chunks(data, permutation):
    output = bytearray(len(data))
    width = len(permutation)
    for base in range(0, len(data), width):
        available = min(width, len(data)-base)
        for numeric, raw in enumerate(i for i in permutation if i < available):
            output[base+raw] = data[base+numeric]
    return bytes(output)


LANES = [(0,), (0, 1), (1, 0), *itertools.permutations(range(3))]
PERMS = list(itertools.permutations(range(3)))


def storage(value, width, reverse_words, permutation):
    words = [(value >> (24*i)) & (B-1) for i in range(width)]
    if reverse_words:
        words.reverse()
    return b"".join(permute_chunks(w.to_bytes(3, "little"), permutation) for w in words)


def anchor(mask):
    choices = [4] + [i for i in range(len(mask)-7) if i != 4]
    return next(i for i in choices if i+8 <= len(mask) and mask[i:i+8] == b"\xff"*8)


def controls():
    fixtures = {
        H-1: [H-1], H: [H, 1], H+1: [H+1, 1], B-1: [B-1, 1],
        B: [0, 1], -1: [B-1], -H: [H], -H-1: [H-1, B-1], -B: [0, B-1],
    }
    for n, expected in fixtures.items():
        assert encode_centered(n) == expected == encode_rebalanced(n)
    rng = random.Random(0x5600024)
    values = set(fixtures)
    for k in range(1, 9):
        for center in (B**k, H*B**(k-1), H + (H-1)*sum(B**i for i in range(1, k))):
            for delta in (-3, -2, -1, 0, 1, 2, 3):
                values.add(center+delta)
                values.add(-center+delta)
    values.update(rng.randrange(-(1 << bits), 1 << bits) for bits in [rng.randrange(1, 901) for _ in range(2048)])
    for n in values:
        a, b = encode_centered(n), encode_rebalanced(n)
        assert a == b and decode_horner(a) == n == decode_correction(a)
        assert all(-H <= (word if word < H else word-B) < H for word in a)
        assert decode_horner(a+[0, 0]) == n
        if n > 0:
            assert 0 < a[-1] < H
            if len(a) > 1:
                assert n > (H-1)*(B**(len(a)-1)-1)//(B-1)
    # A sign-threshold crossing changes the next stored word even though the
    # ordinary integer high word has not changed. Exercise a longer cascade.
    cascades = []
    for width in range(1, 6):
        center = H + (H-1)*sum(B**i for i in range(1, width))
        interval = range(center-2, center+3)
        signed = [concat(encode_centered(n, width+1)) for n in interval]
        independent = [concat(encode_rebalanced(n, width+1)) for n in interval]
        assert signed == independent
        mask, expected = consensus(signed, 24*(width+1))
        assert all((s & mask) == expected for s in signed)
        cascades.append({"center": str(center), "width": width+1,
                         "signed_mask_hex": hex(mask), "signed_values_hex": list(map(hex, signed))})
    assert accepts(8321) and 8321 == 53*157
    return {"independent_roundtrips": len(values), "explicit_word_fixtures": len(fixtures),
            "carry_borrow_cascades": cascades}


def main():
    control = controls()
    factors = []
    with (REPO / "data/solved_decimal_challenge.tsv").open() as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            n, p, q = (int(row[k]) for k in ("modulus", "factor1", "factor2"))
            assert p*q == n
            for which, value in enumerate((p, q), 1):
                assert accepts(value)
                prev = previous(value)
                bits = value.bit_length()
                assert prev >= 3 << (bits-2)
                factors.append({"label": row["label"]+"-f"+str(which), "p": value,
                                "previous": prev, "bits": bits, "modulus_bits": n.bit_length()})
    assert len(factors) == 34
    old_anchors = set()
    old_pattern_count = 0
    for f in factors:
        varying = ((f["previous"]+1) ^ f["p"]).bit_length()
        mask = ((1 << (f["bits"]-2))-1) & ~((1 << varying)-1)
        f["unsigned_mask"] = mask
        sizes = {(f["bits"]+7)//8}
        for digit_bits in (16, 24, 32):
            sizes.add((((f["modulus_bits"]+digit_bits-1)//digit_bits+1)//2)*(digit_bits//8))
        for size, endian, permutation in itertools.product(sizes, ("big", "little"), LANES):
            raw_mask = permute_chunks(mask.to_bytes(size, endian), permutation)
            raw_value = permute_chunks((f["p"] & mask).to_bytes(size, endian), permutation)
            at = anchor(raw_mask)
            old_anchors.add(raw_value[at:at+8])
            old_pattern_count += 1
    rows, patterns = [], []
    enumerated = high_lift_checks = endpoint_checks = 0
    for f in factors:
        p, prev, bits = f["p"], f["previous"], f["bits"]
        words = encode_centered(p)
        width = len(words)
        raw = concat(words)
        low, high = prev+1, p
        for value, expected, accepted_basin in ((prev, prev, False),
                                               (low, prev+2, True),
                                               (p-1, p, True),
                                               (p, p, True),
                                               (p+1, p+2, False)):
            decoded = decode_correction(encode_rebalanced(value, width))
            formatted = ((3 << (bits-2)) + (decoded % (1 << (bits-2)))) | 1
            assert formatted == expected
            assert (prev < formatted <= p) == accepted_basin
            endpoint_checks += 1
        values = []
        for n in range(low, high+1):
            first = encode_centered(n, width)
            second = encode_rebalanced(n, width)
            assert first == second and decode_horner(first) == decode_correction(first) == n
            values.append(concat(first))
        enumerated += len(values)
        exact_mask, exact_value = consensus(values, 24*width)
        # High candidate formatting is erased. Lower raw storage coordinates
        # are invariant under adding multiples of 2^(bits-2) to the decoded value.
        retained_mask = exact_mask & ((1 << (bits-2))-1)
        retained_value = exact_value & retained_mask
        assert all((value & retained_mask) == retained_value for value in values)
        modulus = 1 << (bits-2)
        minimum = -H*(B**width-1)//(B-1)
        maximum = (H-1)*(B**width-1)//(B-1)
        for n in {low, (low+high)//2, high}:
            kmin = -((n-minimum)//modulus)
            kmax = (maximum-n)//modulus
            for k in {kmin, kmax, -2, -1, 0, 1, 2}:
                lifted = n+k*modulus
                if minimum <= lifted <= maximum:
                    lifted_words = encode_centered(lifted, width)
                    assert lifted_words == encode_rebalanced(lifted, width)
                    assert concat(lifted_words) & retained_mask == retained_value
                    candidate = ((3 << (bits-2)) + (decode_correction(lifted_words) % modulus)) | 1
                    assert prev < candidate <= p
                    high_lift_checks += 1
        changed_words = [i for i, word in enumerate(words) if word != ((p >> (24*i)) & (B-1))]
        row = {"label": f["label"], "bits": bits, "factor": str(p), "previous_accepted": str(prev),
               "interval_low": str(low), "interval_high": str(high), "interval_size": high-low+1,
               "unsigned_min_words": (bits+23)//24, "signed_positive_words": width,
               "extra_positive_words": width-(bits+23)//24,
               "stored_words_low_to_high_hex": [f"{word:06x}" for word in words],
               "changed_word_indices": changed_words,
               "exact_storage_mask_hex": exact_mask.to_bytes(3*width, "little").hex(),
               "exact_storage_value_hex": exact_value.to_bytes(3*width, "little").hex(),
               "retained_storage_mask_hex": retained_mask.to_bytes(3*width, "little").hex(),
               "retained_storage_value_hex": retained_value.to_bytes(3*width, "little").hex(),
               "fresh_storage_hex": raw.to_bytes(3*width, "little").hex(),
               "signed_fixed_retained_bits": bin(retained_mask).count("1"),
               "unsigned_fixed_retained_bits": bin(f["unsigned_mask"]).count("1"),
               "changed_common_fixed_bits": bin((raw^p) & retained_mask & f["unsigned_mask"]).count("1")}
        rows.append(row)
        for reverse, permutation in itertools.product((False, True), PERMS):
            raw_mask = storage(retained_mask, width, reverse, permutation)
            raw_value = storage(retained_value, width, reverse, permutation)
            at = anchor(raw_mask)
            selected = raw_value[at:at+8]
            fixed_windows = [raw_value[i:i+8] for i in range(len(raw_mask)-7) if raw_mask[i:i+8] == b"\xff"*8]
            patterns.append({"label": f["label"], "word_order": "high-to-low" if reverse else "low-to-high",
                             "raw_position_for_numeric_byte": permutation, "bytes": 3*width,
                             "mask_hex": raw_mask.hex(), "value_hex": raw_value.hex(),
                             "anchor": at, "fingerprint_hex": selected.hex(),
                             "selected_fingerprint_is_new": selected not in old_anchors,
                             "any_fixed_window_is_old_anchor": any(x in old_anchors for x in fixed_windows)})
    summary = {
        "factors": len(rows), "interval_integer_encodings_checked": enumerated,
        "lifted_high_bits_checks": high_lift_checks,
        "inclusive_exclusive_endpoint_checks": endpoint_checks,
        "factors_with_changed_common_fixed_bits": sum(row["changed_common_fixed_bits"] > 0 for row in rows),
        "changed_common_fixed_bits_total": sum(row["changed_common_fixed_bits"] for row in rows),
        "factors_requiring_extra_positive_word": [row["label"] for row in rows if row["extra_positive_words"]],
        "old_patterns": old_pattern_count, "old_unique_fingerprints": len(old_anchors),
        "signed_patterns": len(patterns),
        "signed_selected_fingerprints_new": sum(p["selected_fingerprint_is_new"] for p in patterns),
        "signed_unique_fingerprints_new": len({p["fingerprint_hex"] for p in patterns if p["selected_fingerprint_is_new"]}),
        "signed_patterns_without_any_old_fixed_window": sum(not p["any_fixed_window_is_old_anchor"] for p in patterns),
        "signed_fixed_retained_bits_min": min(row["signed_fixed_retained_bits"] for row in rows),
        "signed_fixed_retained_bits_max": max(row["signed_fixed_retained_bits"] for row in rows),
        "no_seed_streams_scanned": True,
    }
    output = {"summary": summary, "controls": control, "factors": rows, "patterns": patterns,
              "source_hashes": {str(p.relative_to(REPO)): hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in [Path(__file__).resolve(), REPO/"data/solved_decimal_challenge.tsv",
                                          REPO/"src/noise_hamming_scan.cpp", REPO/"references/rsaref/raw_prime-old.c"]}}
    (OUT/"sensitivity.json").write_text(json.dumps(output, indent=2)+"\n")
    fields = ["label", "bits", "interval_size", "unsigned_min_words", "signed_positive_words",
              "extra_positive_words", "signed_fixed_retained_bits", "changed_common_fixed_bits"]
    with (OUT/"factor-summary.tsv").open("w") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t", extrasaction="ignore")
        writer.writeheader(); writer.writerows(rows)
    print("SIGNED24_SENSITIVITY PASSED")
    print(json.dumps(summary, indent=2))
    print("independent_roundtrips=" + str(control["independent_roundtrips"]))


if __name__ == "__main__":
    main()
