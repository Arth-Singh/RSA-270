#!/usr/bin/env python3
"""Recover masked RSAREF GenerateDigits output from solved RSA factors.

Model (references/rsaref/raw_r_keygen.c and raw_prime-old.c):

  GenerateDigits(c) consumes a fixed-width big-endian random byte block.
  FindRSAPrime returns the first accepted odd value reached from
      3*2^(b-2) + (c mod 2^(b-2)).

For a published output prime p, the exact raw residue is therefore a short
contiguous interval.  Everything above its varying suffix is known.  This
script lays those masks into the plausible sequential RSA-100 ... RSA-260
stream for 16-, 24-, and 32-bit multiprecision digits, enumerating generation
order where equal-bit primes were sorted after generation.

The old RSAREF acceptance predicate is reproduced exactly: trial division by
3,5,7,11, Fermat base 2, and gcd(candidate-1, e=3).  A modern primality oracle
would be the wrong boundary predicate if a base-2 pseudoprime occurred.
"""

import argparse
import csv
import heapq
import math
import os


def popcount(value):
    return bin(value).count("1")


def binomial_two_sided(n, k):
    m = min(k, n - k)
    if 2 * m == n:
        return 1.0
    log_mass = (math.lgamma(n + 1) - math.lgamma(m + 1)
                - math.lgamma(n - m + 1) - n * math.log(2.0))
    mass = math.exp(log_mass)
    tail = mass
    while m:
        mass *= m / float(n - m + 1)
        tail += mass
        m -= 1
    return min(1.0, 2.0 * tail)


def load_rows(path):
    rows = []
    with open(path, newline="") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            modulus = int(row["modulus"])
            factors = (int(row["factor1"]), int(row["factor2"]))
            if factors[0] * factors[1] != modulus:
                raise ValueError("bad factorization for " + row["label"])
            rows.append({"label": row["label"], "modulus": modulus,
                         "factors": factors})
    if len(rows) != 17:
        raise ValueError("expected the 17 solved decimal-list rows")
    return rows


def old_rsaref_accepts(candidate):
    if candidate % 3 != 2:
        return False
    if any(candidate % divisor == 0 for divisor in (5, 7, 11)):
        return False
    # FermatTest in raw_prime-old.c compares 2^candidate mod candidate with 2.
    return pow(2, candidate, candidate) == 2


def recover_residue(prime, target_bits):
    if prime.bit_length() != target_bits:
        raise ValueError("factor has wrong target bit length")
    base = 3 << (target_bits - 2)
    modulus = 1 << (target_bits - 2)
    if not (base <= prime < base + modulus):
        raise ValueError("factor lacks RSAREF prefix 11")
    if not old_rsaref_accepts(prime):
        raise ValueError("published factor fails old RSAREF predicate")

    previous = prime - 2
    while not old_rsaref_accepts(previous):
        previous -= 2

    # For every odd starting candidate s in (previous, prime], parity folding
    # maps the two raw residues s-base-1 and s-base to s.  Their union is this
    # exact contiguous interval, containing prime-previous possibilities.
    low = previous - base + 1
    high = prime - base
    if low < 0 or high >= modulus:
        raise ValueError("wraparound case needs separate treatment")
    varying_low_bits = (low ^ high).bit_length()
    fixed_bits = target_bits - 2 - varying_low_bits
    fixed_mask = ((1 << fixed_bits) - 1) << varying_low_bits
    return {
        "prime": prime,
        "bits": target_bits,
        "previous": previous,
        "gap": prime - previous,
        "low": low,
        "high": high,
        "varying_low_bits": varying_low_bits,
        "fixed_bits": fixed_bits,
        "residue_mask": fixed_mask,
        "residue_value": low & fixed_mask,
    }


def block_geometry(modulus_bits, digit_bits):
    modulus_digits = (modulus_bits + digit_bits - 1) // digit_bits
    prime_digits = (modulus_digits + 1) // 2
    return prime_digits * digit_bits


def ambiguous_rows(rows):
    return [index for index, row in enumerate(rows)
            if row["factors"][0].bit_length()
            == row["factors"][1].bit_length()]


def factor_assignment(row, swap_equal):
    """Return factor indices for first ceil(B/2), then floor(B/2) call."""
    first_bits = (row["modulus"].bit_length() + 1) // 2
    factors = row["factors"]
    if factors[0].bit_length() != factors[1].bit_length():
        if factors[0].bit_length() == first_bits:
            return (0, 1)
        return (1, 0)
    return (1, 0) if swap_equal else (0, 1)


def factor_recoveries(rows):
    result = {}
    for row_index, row in enumerate(rows):
        modulus_bits = row["modulus"].bit_length()
        target_bits = ((modulus_bits + 1) // 2, modulus_bits // 2)
        for factor_index, factor in enumerate(row["factors"]):
            # Equal-bit pair: either call has same target.  Unequal pair:
            # identify target from observed bit length.
            if factor.bit_length() == target_bits[0]:
                bits = target_bits[0]
            elif factor.bit_length() == target_bits[1]:
                bits = target_bits[1]
            else:
                raise ValueError("factor allocation mismatch")
            result[(row_index, factor_index)] = recover_residue(factor, bits)
    return result


def build_stream(rows, recoveries, digit_bits, choice):
    value = 0
    mask = 0
    records = []
    offset_bits = 0
    ambiguous = ambiguous_rows(rows)
    choices = {row_index: bool((choice >> position) & 1)
               for position, row_index in enumerate(ambiguous)}
    for row_index, row in enumerate(rows):
        modulus_bits = row["modulus"].bit_length()
        targets = ((modulus_bits + 1) // 2, modulus_bits // 2)
        block_bits = block_geometry(modulus_bits, digit_bits)
        assignment = factor_assignment(row, choices.get(row_index, False))
        for call_index, factor_index in enumerate(assignment):
            recovery = recoveries[(row_index, factor_index)]
            if recovery["bits"] != targets[call_index]:
                raise ValueError("generation-order bit mismatch")
            padding = block_bits - (recovery["bits"] - 2)
            if padding < 0:
                raise ValueError("digit block too short")
            # Residue is already in the low b-2 bits of the raw block; leading
            # padding and varying low suffix remain unmasked.
            block_value = recovery["residue_value"]
            block_mask = recovery["residue_mask"]
            value = (value << block_bits) | block_value
            mask = (mask << block_bits) | block_mask
            records.append({
                "row": row_index,
                "factor": factor_index,
                "call": call_index,
                "offset_bits": offset_bits,
                "block_bits": block_bits,
                "padding_bits": padding,
                "value": block_value,
                "mask": block_mask,
                "recovery": recovery,
            })
            offset_bits += block_bits
    return value, mask, offset_bits, records


def aligned_chunk(integer, total_bits, start, length):
    return (integer >> (total_bits - start - length)) & ((1 << length) - 1)


def digest_block_inventory(rows, recoveries, digit_bits, dump):
    choices = 1 << len(ambiguous_rows(rows))
    all_full = None
    full_counts = {}
    values = {}
    repeat_tests = repeat_hits = complement_hits = 0
    total_bits = None
    for choice in range(choices):
        value, mask, total_bits, _ = build_stream(
            rows, recoveries, digit_bits, choice)
        full_here = set()
        block_values = []
        for block_index in range(total_bits // 128):
            start = 128 * block_index
            block_mask = aligned_chunk(mask, total_bits, start, 128)
            if block_mask != (1 << 128) - 1:
                continue
            block_value = aligned_chunk(value, total_bits, start, 128)
            full_here.add(block_index)
            full_counts[block_index] = full_counts.get(block_index, 0) + 1
            values.setdefault(block_index, set()).add(block_value)
            block_values.append((block_index, block_value))
        all_full = full_here if all_full is None else all_full & full_here
        for left in range(len(block_values)):
            for right in range(left + 1, len(block_values)):
                repeat_tests += 1
                if block_values[left][1] == block_values[right][1]:
                    repeat_hits += 1
                if block_values[left][1] ^ block_values[right][1] == (1 << 128) - 1:
                    complement_hits += 1

    deterministic = [index for index in sorted(all_full)
                     if len(values[index]) == 1]
    choice_dependent = [index for index in sorted(all_full)
                        if len(values[index]) > 1]
    some = [index for index, count in full_counts.items()
            if count and index not in all_full]
    print("digest_blocks digit_bits={} layouts={} stream_bytes={} total_blocks={}"
          " full_every_layout={} deterministic={} choice_dependent={}"
          " full_some_layout={}".format(
              digit_bits, choices, total_bits // 8, total_bits // 128,
              len(all_full), len(deterministic), len(choice_dependent), len(some)))
    print("digest_repeat digit_bits={} pair_tests_with_layout_multiplicity={}"
          " equal_hits={} complement_hits={}".format(
              digit_bits, repeat_tests, repeat_hits, complement_hits))
    if dump:
        for block_index in deterministic:
            only_value = next(iter(values[block_index]))
            print("digest_exact digit_bits={} index={} stream_offset={} hex={:032x}".format(
                digit_bits, block_index, 16 * block_index, only_value))
        for block_index in choice_dependent:
            candidates = ",".join("{:032x}".format(item)
                                  for item in sorted(values[block_index]))
            print("digest_order_choices digit_bits={} index={} stream_offset={}"
                  " candidates={}".format(
                      digit_bits, block_index, 16 * block_index, candidates))


def masked_autocorrelation(rows, recoveries, digit_widths, max_lag):
    # Scan all equal-bit generation-order choices.  Keep the strongest 1024
    # standardized candidates, then evaluate those with exact binomial tails;
    # this avoids ~10^9 tail-recurrence operations.  With n in one narrow band,
    # exact and standardized rankings coincide far beyond the retained margin.
    heap = []
    tests = 0
    kept = 1024
    for digit_bits in digit_widths:
        choices = 1 << len(ambiguous_rows(rows))
        for choice in range(choices):
            value, mask, total_bits, _ = build_stream(
                rows, recoveries, digit_bits, choice)
            for lag in range(1, max_lag + 1):
                low_mask = (1 << (total_bits - lag)) - 1
                pair_mask = (mask >> lag) & mask & low_mask
                n = popcount(pair_mask)
                differences = popcount(((value >> lag) ^ value) & pair_mask)
                matches = n - differences
                score = abs(2 * matches - n) / math.sqrt(n)
                record = (score, digit_bits, choice, lag, n, matches)
                tests += 1
                if len(heap) < kept:
                    heapq.heappush(heap, record)
                elif record > heap[0]:
                    heapq.heapreplace(heap, record)
    exact = []
    for score, digit_bits, choice, lag, n, matches in heap:
        exact.append((binomial_two_sided(n, matches), score, digit_bits,
                      choice, lag, n, matches))
    best = min(exact)
    p, score, digit_bits, choice, lag, n, matches = best
    print("masked_autocorrelation tests={} retained_for_exact={} best_digit_bits={}"
          " order_choice={} lag={} known_pairs={} matches={} z={:.6f}"
          " exact_two_sided_p={:.12g} bonferroni_p={:.12g}".format(
              tests, len(heap), digit_bits, choice, lag, n, matches, score,
              p, min(1.0, tests * p)))


def dump_factor_masks(rows, recoveries, digit_bits):
    # Show factor-local block masks.  For equal-bit pairs, either listed stream
    # offset is possible; unequal-bit pairs have one exact call position.
    _, _, _, records_zero = build_stream(rows, recoveries, digit_bits, 0)
    ambiguous = set(ambiguous_rows(rows))
    record_by_factor = {(record["row"], record["factor"]): record
                        for record in records_zero}
    for row_index, row in enumerate(rows):
        for factor_index in range(2):
            record = record_by_factor[(row_index, factor_index)]
            offsets = [record["offset_bits"] // 8]
            if row_index in ambiguous:
                peer = record_by_factor[(row_index, 1 - factor_index)]
                offsets.append(peer["offset_bits"] // 8)
            recovery = record["recovery"]
            byte_length = record["block_bits"] // 8
            print("raw_mask digit_bits={} label={} factor={} offsets={} block_bytes={}"
                  " target_bits={} leading_discard_bits={} residue_options={}"
                  " varying_low_bits={} fixed_bits={} value={} mask={}".format(
                      digit_bits, row["label"], factor_index + 1,
                      ",".join(map(str, sorted(offsets))), byte_length,
                      recovery["bits"], record["padding_bits"], recovery["gap"],
                      recovery["varying_low_bits"], recovery["fixed_bits"],
                      recovery["residue_value"].to_bytes(byte_length, "big").hex(),
                      recovery["residue_mask"].to_bytes(byte_length, "big").hex()))


def target_offsets(rows, digit_widths):
    rsa270_bits = int(
        "233108530344407544527637656910680524145619812480305449042948611968495918245135782867888369318577116418213919268572658314913060672626911354027609793166341626693946596196427744273886601876896313468704059066746903123910748277606548649151920812699309766587514735456594993207").bit_length()
    for digit_bits in digit_widths:
        offset = sum(2 * block_geometry(row["modulus"].bit_length(), digit_bits)
                     for row in rows) // 8
        block_bytes = block_geometry(rsa270_bits, digit_bits) // 8
        print("RSA270_stream digit_bits={} assumption=continuous_from_RSA100"
              " first_offset={} second_offset={} end_offset={} block_bytes={}"
              " digest16_start={}:{} first_target_bits=448 second_target_bits=447".format(
                  digit_bits, offset, offset + block_bytes,
                  offset + 2 * block_bytes, block_bytes,
                  offset // 16, offset % 16))


def main():
    parser = argparse.ArgumentParser()
    default_path = os.path.join(os.path.dirname(os.path.dirname(__file__)),
                                "data", "solved_decimal_challenge.tsv")
    parser.add_argument("--input", default=default_path)
    parser.add_argument("--max-lag", type=int, default=512)
    parser.add_argument("--dump-width", type=int, choices=(16, 24, 32))
    args = parser.parse_args()
    rows = load_rows(args.input)
    recoveries = factor_recoveries(rows)
    ambiguous = ambiguous_rows(rows)
    print("model=RSAREF-GenerateDigits+FindRSAPrime rows={} factors=34"
          " equal_bit_order_choices={} ambiguous_rows={}".format(
              len(rows), 1 << len(ambiguous),
              ",".join(rows[index]["label"] for index in ambiguous)))
    for digit_bits in (16, 24, 32):
        digest_block_inventory(rows, recoveries, digit_bits,
                               args.dump_width == digit_bits)
    masked_autocorrelation(rows, recoveries, (16, 24, 32), args.max_lag)
    target_offsets(rows, (16, 24, 32))
    if args.dump_width:
        dump_factor_masks(rows, recoveries, args.dump_width)


if __name__ == "__main__":
    main()
