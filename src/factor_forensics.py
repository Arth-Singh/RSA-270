#!/usr/bin/env python3
"""Forensics on the published factors of the original RSA decimal challenges.

This is deliberately a statistical *falsification* tool.  It looks for signals
that could constrain RSA-270: fixed high bits, congruences, 24-bit lane bias,
cross-prime copies/complements/lag correlation, concatenated-stream
autocorrelation, and low linear complexity.  Every reported search-family
p-value is Bonferroni corrected by the number of hypotheses actually tried.

Only Python's standard library is required.  Input defaults to the 17 solved
RSA-100 ... RSA-260 rows in data/solved_decimal_challenge.tsv.
"""

import csv
import itertools
import math
import os
import sys


def popcount(value):
    # System Python on the recovery host is 3.9 and lacks int.bit_count().
    return bin(value).count("1")


def binomial_two_sided(n, k):
    """Exact fair-binomial two-sided tail, accumulated in double precision."""
    m = min(k, n - k)
    if 2 * m == n:
        return 1.0
    log_mass = (math.lgamma(n + 1) - math.lgamma(m + 1)
                - math.lgamma(n - m + 1) - n * math.log(2.0))
    mass = math.exp(log_mass)
    tail = mass
    j = m
    while j:
        mass *= j / float(n - j + 1)
        tail += mass
        j -= 1
    return min(1.0, 2.0 * tail)


def binomial_upper(n, k, probability):
    """P[Binomial(n, probability) >= k], stable for small probability."""
    if k <= 0:
        return 1.0
    if k > n:
        return 0.0
    log_mass = (math.lgamma(n + 1) - math.lgamma(k + 1)
                - math.lgamma(n - k + 1) + k * math.log(probability)
                + (n - k) * math.log1p(-probability))
    mass = math.exp(log_mass)
    tail = mass
    j = k
    while j < n:
        mass *= ((n - j) / float(j + 1)) * (probability / (1.0 - probability))
        tail += mass
        j += 1
    return min(1.0, tail)


def fair_tail_tables(max_n):
    """tables[n][min(k,n-k)] = exact two-sided fair-binomial p."""
    tables = [None] * (max_n + 1)
    for n in range(max_n + 1):
        mass = 2.0 ** (-n)
        cumulative = 0.0
        row = []
        for k in range(n // 2 + 1):
            cumulative += mass
            row.append(min(1.0, 2.0 * cumulative))
            if k != n // 2:
                mass *= (n - k) / float(k + 1)
        tables[n] = row
    return tables


def load_rows(path):
    rows = []
    with open(path, newline="") as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            modulus = int(row["modulus"])
            factors = (int(row["factor1"]), int(row["factor2"]))
            if factors[0] * factors[1] != modulus:
                raise ValueError("bad factorization for " + row["label"])
            rows.append((row["label"], modulus, factors))
    if len(rows) != 17:
        raise ValueError("expected exactly RSA-100 ... RSA-260 (17 rows)")
    return rows


def bits_to_int(bits):
    return int(bits, 2) if bits else 0


def reverse_byte_bits(byte):
    return int("{:08b}".format(byte)[::-1], 2)


def transform_bytes(value, endian, lane, reverse_bits):
    size = (value.bit_length() + 7) // 8
    data = list(value.to_bytes(size, byteorder=endian))
    width = len(lane)
    transformed = []
    for base in range(0, len(data), width):
        chunk = data[base:base + width]
        if len(chunk) == width:
            chunk = [chunk[index] for index in lane]
        transformed.extend(chunk)
    if reverse_bits:
        transformed = [reverse_byte_bits(byte) for byte in transformed]
    return bytes(transformed)


def make_transforms(factors):
    candidates = []
    lane_sets = [
        ("byte", (0,)),
        ("word-id", (0, 1)),
        ("word-swap", (1, 0)),
    ]
    lane_sets.extend(("24-" + "".join(map(str, p)), p)
                     for p in itertools.permutations(range(3)))
    for endian in ("big", "little"):
        for lane_name, lane in lane_sets:
            for reverse_bits in (False, True):
                name = "{}:{}:{}".format(
                    "be" if endian == "big" else "le", lane_name,
                    "bitrev" if reverse_bits else "msb")
                streams = []
                for value in factors:
                    data = transform_bytes(value, endian, lane, reverse_bits)
                    # Four bytes at each edge exclude forced high bits and the
                    # odd/mod-3/prime-search tail from correlation tests.
                    data = data[4:-4]
                    bit_string = "".join("{:08b}".format(byte) for byte in data)
                    streams.append(bit_string)
                candidates.append((name, streams))

    # Several endian/lane combinations are identical on every input.  Counting
    # duplicates as hypotheses would only weaken correction and waste time.
    unique = []
    seen = set()
    for name, streams in candidates:
        signature = tuple(streams)
        if signature not in seen:
            seen.add(signature)
            unique.append((name, streams))
    return unique


def fixed_prefix_test(factor_records):
    count = len(factor_records)
    bits = [format(value, "b") for _, value in factor_records]
    best = None
    tests = 0
    for position in range(1, 64):  # top bit is tautologically one
        ones = sum(stream[position] == "1" for stream in bits)
        p = binomial_two_sided(count, ones)
        tests += 1
        if best is None or p < best[0]:
            best = (p, position + 1, ones)
    p, position, ones = best
    print("prefix factors={} searched_positions={} best_position={} ones={}"
          " raw_two_sided_p={:.12g} bonferroni_p={:.12g}".format(
              count, tests, position, ones, p, min(1.0, tests * p)))


def congruence_test(factor_records):
    residues3 = [value % 3 for _, value in factor_records]
    residues6 = [value % 6 for _, value in factor_records]
    print("congruence mod3_counts={} mod6_counts={} all_mod3_eq_2={}".format(
        {r: residues3.count(r) for r in sorted(set(residues3))},
        {r: residues6.count(r) for r in sorted(set(residues6))},
        all(r == 2 for r in residues3)))


def periodic_bias_test(factor_records):
    # Canonical numeric bits.  Remove known fixed prefix bits and 32 low bits
    # influenced by oddness, p mod 3, and incremental primality search.
    interiors = []
    for _, value in factor_records:
        bits = format(value, "b")
        interiors.append(bits[2:-32])

    hypotheses = []
    all_bits = "".join(interiors)
    hypotheses.append(("global-bit", len(all_bits), all_bits.count("1")))
    transitions = []
    for bits in interiors:
        transitions.extend(a != b for a, b in zip(bits, bits[1:]))
    hypotheses.append(("global-transition", len(transitions), sum(transitions)))

    for origin in ("msb", "lsb"):
        for residue in range(24):
            selected = []
            selected_transitions = []
            for bits in interiors:
                length = len(bits)
                for position, bit in enumerate(bits):
                    coordinate = position if origin == "msb" else length - 1 - position
                    if coordinate % 24 == residue:
                        selected.append(bit)
                for position, (left, right) in enumerate(zip(bits, bits[1:])):
                    coordinate = position if origin == "msb" else length - 2 - position
                    if coordinate % 24 == residue:
                        selected_transitions.append(left != right)
            hypotheses.append(("{}-bit-r{}".format(origin, residue),
                               len(selected), selected.count("1")))
            hypotheses.append(("{}-transition-r{}".format(origin, residue),
                               len(selected_transitions), sum(selected_transitions)))

    scored = [(binomial_two_sided(n, k), name, n, k)
              for name, n, k in hypotheses]
    scored.sort()
    p, name, n, k = scored[0]
    print("bit_bias tests={} bits={} ones={} transitions={} changed={}".format(
        len(hypotheses), len(all_bits), all_bits.count("1"),
        len(transitions), sum(transitions)))
    print("bit_bias_best name={} n={} count={} raw_two_sided_p={:.12g}"
          " bonferroni_p={:.12g}".format(
              name, n, k, p, min(1.0, len(hypotheses) * p)))


def zero_lane_test(factor_records):
    hypotheses = []
    for endian in ("big", "little"):
        byte_streams = []
        for _, value in factor_records:
            size = (value.bit_length() + 7) // 8
            data = value.to_bytes(size, byteorder=endian)[2:-2]
            byte_streams.append(data)
        for width in (2, 3, 4):
            for residue in range(width):
                lane = [byte for data in byte_streams
                        for position, byte in enumerate(data)
                        if position % width == residue]
                zeros = lane.count(0)
                hypotheses.append((binomial_upper(len(lane), zeros, 1.0 / 256.0),
                                   endian, width, residue, len(lane), zeros))
    hypotheses.sort()
    p, endian, width, residue, n, zeros = hypotheses[0]
    print("zero_lane tests={} best={}:width{}:r{} n={} zeros={}"
          " raw_upper_p={:.12g} bonferroni_p={:.12g}".format(
              len(hypotheses), endian, width, residue, n, zeros, p,
              min(1.0, len(hypotheses) * p)))


def extract(integer, length, start, count):
    return (integer >> (length - start - count)) & ((1 << count) - 1)


def lag_correlation_test(rows, factor_records, transforms):
    max_n = max(len(bits) for _, streams in transforms for bits in streams)
    tails = fair_tail_tables(max_n)
    all_best = None
    within_best = None
    zero_best = None
    all_tests = within_tests = zero_tests = 0

    modulus_index = []
    for index in range(len(rows)):
        modulus_index.extend((index, index))

    for transform_name, streams in transforms:
        encoded = [(bits_to_int(bits), len(bits)) for bits in streams]
        for left in range(len(encoded)):
            left_integer, left_length = encoded[left]
            for right in range(left + 1, len(encoded)):
                right_integer, right_length = encoded[right]
                same_modulus = modulus_index[left] == modulus_index[right]
                for lag in range(-128, 129):
                    left_start = max(lag, 0)
                    right_start = max(-lag, 0)
                    overlap = min(left_length - left_start,
                                  right_length - right_start)
                    if overlap < 64:
                        continue
                    left_part = extract(left_integer, left_length,
                                        left_start, overlap)
                    right_part = extract(right_integer, right_length,
                                         right_start, overlap)
                    matches = overlap - popcount(left_part ^ right_part)
                    p = tails[overlap][min(matches, overlap - matches)]
                    record = (p, transform_name, left, right, lag,
                              overlap, matches)
                    all_tests += 1
                    if all_best is None or record < all_best:
                        all_best = record
                    if same_modulus:
                        within_tests += 1
                        if within_best is None or record < within_best:
                            within_best = record
                    if lag == 0 and same_modulus:
                        zero_tests += 1
                        if zero_best is None or record < zero_best:
                            zero_best = record

    def report(prefix, best, tests):
        p, transform_name, left, right, lag, overlap, matches = best
        print("{} tests={} best={}/{} vs {}/{} transform={} lag={} n={}"
              " matches={} raw_two_sided_p={:.12g} bonferroni_p={:.12g}".format(
                  prefix, tests, factor_records[left][0], left % 2 + 1,
                  factor_records[right][0], right % 2 + 1,
                  transform_name, lag, overlap, matches, p,
                  min(1.0, tests * p)))

    report("cross_lag", all_best, all_tests)
    report("within_modulus_lag", within_best, within_tests)
    report("within_modulus_zero_lag", zero_best, zero_tests)


def longest_cross_overlap(factor_records, transforms):
    """Longest copied or complemented interior substring across factors."""
    max_length = max(len(bits) for _, streams in transforms for bits in streams)
    hit = None
    hit_length = 0
    for length in range(max_length, 0, -1):
        mask = (1 << length) - 1
        for transform_name, streams in transforms:
            seen = {}
            for factor_index, bits in enumerate(streams):
                if len(bits) < length:
                    continue
                integer = bits_to_int(bits)
                for position in range(len(bits) - length + 1):
                    value = extract(integer, len(bits), position, length)
                    for complemented, wanted in ((False, value),
                                                 (True, value ^ mask)):
                        prior = seen.get(wanted)
                        if prior is not None and prior[0] != factor_index:
                            hit = (transform_name, prior[0], factor_index,
                                   prior[1], position, complemented)
                            hit_length = length
                            break
                    if hit is not None:
                        break
                    # First occurrence suffices: any later different factor
                    # will collide with it; same-factor repeats do not erase it.
                    if value not in seen:
                        seen[value] = (factor_index, position)
                if hit is not None:
                    break
            if hit is not None:
                break
        if hit is not None:
            break

    # Conservative union bound for finding an exact OR complemented match of
    # this length anywhere in the full family searched.
    comparisons = 0
    for _, streams in transforms:
        for left in range(len(streams)):
            left_windows = max(0, len(streams[left]) - hit_length + 1)
            for right in range(left + 1, len(streams)):
                right_windows = max(0, len(streams[right]) - hit_length + 1)
                comparisons += 2 * left_windows * right_windows
    transform_name, left, right, left_pos, right_pos, complemented = hit
    union_bound = min(1.0, comparisons * (2.0 ** (-hit_length)))
    print("cross_substring transforms={} max_bits={} relation={}"
          " best={}/{}@{} vs {}/{}@{} transform={} comparisons_at_L={}"
          " union_bound={:.12g}".format(
              len(transforms), hit_length,
              "complement" if complemented else "equal",
              factor_records[left][0], left % 2 + 1, left_pos,
              factor_records[right][0], right % 2 + 1, right_pos,
              transform_name, comparisons, union_bound))


def ordered_sequences(rows, transforms):
    """Plausible global chronology and p/q orientation choices (112 total)."""
    for transform_name, streams in transforms:
        for reverse_moduli in (False, True):
            indices = list(range(len(rows)))
            if reverse_moduli:
                indices.reverse()
            for swap_pair in (False, True):
                ordered = []
                factor_indices = []
                for index in indices:
                    pair = [2 * index, 2 * index + 1]
                    if swap_pair:
                        pair.reverse()
                    factor_indices.extend(pair)
                    ordered.extend(streams[item] for item in pair)
                name = "{}:{}:{}".format(
                    transform_name,
                    "descending" if reverse_moduli else "ascending",
                    "high-first" if swap_pair else "low-first")
                yield name, ordered, factor_indices


def boundary_overlap_test(rows, factor_records, transforms):
    best = None
    tests = 0
    for name, streams, indices in ordered_sequences(rows, transforms):
        for boundary in range(len(streams) - 1):
            left = streams[boundary]
            right = streams[boundary + 1]
            for complemented in (False, True):
                tests += 1
                overlap = 0
                limit = min(len(left), len(right))
                for length in range(1, limit + 1):
                    suffix = left[-length:]
                    prefix = right[:length]
                    if complemented:
                        prefix = "".join("1" if bit == "0" else "0"
                                         for bit in prefix)
                    if suffix == prefix:
                        overlap = length
                record = (overlap, name, indices[boundary],
                          indices[boundary + 1], complemented)
                if best is None or record > best:
                    best = record
    overlap, name, left, right, complemented = best
    # Each boundary/relation is one test of overlap >= observed length.
    union_bound = min(1.0, tests * (2.0 ** (-overlap)))
    print("boundary_overlap tests={} max_bits={} relation={} left={}/{}"
          " right={}/{} layout={} union_bound={:.12g}".format(
              tests, overlap, "complement" if complemented else "equal",
              factor_records[left][0], left % 2 + 1,
              factor_records[right][0], right % 2 + 1, name, union_bound))


def concatenated_autocorrelation(rows, transforms):
    layouts = list(ordered_sequences(rows, transforms))
    best_by_lag = {}
    lag24 = None
    tests = 0
    for name, streams, _ in layouts:
        bits = "".join(streams)
        integer = bits_to_int(bits)
        total = len(bits)
        for lag in range(1, 513):
            overlap = total - lag
            left = integer >> lag
            right = integer & ((1 << overlap) - 1)
            matches = overlap - popcount(left ^ right)
            minority = min(matches, overlap - matches)
            record = (minority, name, overlap, matches)
            tests += 1
            prior = best_by_lag.get(lag)
            if prior is None or record < prior:
                best_by_lag[lag] = record
            if lag == 24 and (lag24 is None or record < lag24):
                lag24 = record

    best = None
    for lag, (minority, name, overlap, matches) in best_by_lag.items():
        p = binomial_two_sided(overlap, matches)
        record = (p, lag, name, overlap, matches)
        if best is None or record < best:
            best = record
    p, lag, name, overlap, matches = best
    p24 = binomial_two_sided(lag24[2], lag24[3])
    print("concat_autocorrelation layouts={} lags=512 tests={} best_layout={}"
          " lag={} n={} matches={} raw_two_sided_p={:.12g}"
          " bonferroni_p={:.12g}".format(
              len(layouts), tests, name, lag, overlap, matches, p,
              min(1.0, tests * p)))
    print("concat_lag24 best_layout={} n={} matches={} raw_two_sided_p={:.12g}"
          " bonferroni_over_layouts_p={:.12g}".format(
              lag24[1], lag24[2], lag24[3], p24,
              min(1.0, len(layouts) * p24)))


def berlekamp_massey(bits):
    sequence = [bit == "1" for bit in bits]
    n = len(sequence)
    connection = [False] * n
    previous = [False] * n
    connection[0] = previous[0] = True
    complexity = 0
    last_update = -1
    for position in range(n):
        discrepancy = sequence[position]
        for offset in range(1, complexity + 1):
            discrepancy ^= connection[offset] and sequence[position - offset]
        if not discrepancy:
            continue
        saved = connection[:]
        shift = position - last_update
        for index in range(n - shift):
            connection[index + shift] ^= previous[index]
        if 2 * complexity <= position:
            complexity = position + 1 - complexity
            last_update = position
            previous = saved
    return complexity


def linear_complexity_test(factor_records, transforms):
    per_factor = [[] for _ in factor_records]
    global_best = None
    for transform_name, streams in transforms:
        for index, bits in enumerate(streams):
            complexity = berlekamp_massey(bits)
            per_factor[index].append((complexity, transform_name, len(bits)))
            record = (complexity, index, transform_name, len(bits))
            if global_best is None or record < global_best:
                global_best = record
    complexity, index, transform_name, length = global_best
    print("linear_complexity transforms={} global_min_L={} n={} factor={}/{}"
          " transform={}".format(
              len(transforms), complexity, length, factor_records[index][0],
              index % 2 + 1, transform_name))
    for index, (label, _) in enumerate(factor_records):
        if label == "RSA-260":
            minimum = min(per_factor[index])
            print("linear_complexity_RSA260 factor={} min_L={} n={} transform={}".format(
                index % 2 + 1, minimum[0], minimum[2], minimum[1]))


def rsa270_constraint():
    modulus = int(
        "233108530344407544527637656910680524145619812480305449042948611968495918245135782867888369318577116418213919268572658314913060672626911354027609793166341626693946596196427744273886601876896313468704059066746903123910748277606548649151920812699309766587514735456594993207")
    # All solved decimal-list pairs use the balanced RSAREF allocation
    # ceil(B/2),floor(B/2).  If that and prefix 11 persist, 895-bit RSA-270
    # is 448x447 bits; (3*2^446)^2 > N rules out 448x448.  Congruence 2 mod 3
    # then makes the smaller factor 5 mod 6.
    lower = max(3 << 444, modulus // ((1 << 448) - 1) + 1)
    upper = min((1 << 447) - 1, modulus // (3 << 446))
    first = lower + (5 - lower) % 6
    last = upper - (upper - 5) % 6
    candidates = (last - first) // 6 + 1
    print("RSA270 inferred_if_RSAREF_constraints bits={} mod3={} factor_bits=447x448"
          " smaller_lower={} smaller_upper={} admissible_5mod6_count={}"
          " count_bits={}".format(
              modulus.bit_length(), modulus % 3, first, last, candidates,
              candidates.bit_length()))


def main():
    default_path = os.path.join(os.path.dirname(os.path.dirname(__file__)),
                                "data", "solved_decimal_challenge.tsv")
    path = sys.argv[1] if len(sys.argv) > 1 else default_path
    rows = load_rows(path)
    factor_records = []
    for label, _, pair in rows:
        factor_records.extend(((label, pair[0]), (label, pair[1])))
    factors = [value for _, value in factor_records]
    transforms = make_transforms(factors)
    print("dataset rows={} factors={} transforms={}".format(
        len(rows), len(factors), len(transforms)))
    fixed_prefix_test(factor_records)
    congruence_test(factor_records)
    periodic_bias_test(factor_records)
    zero_lane_test(factor_records)
    lag_correlation_test(rows, factor_records, transforms)
    longest_cross_overlap(factor_records, transforms)
    boundary_overlap_test(rows, factor_records, transforms)
    concatenated_autocorrelation(rows, transforms)
    linear_complexity_test(factor_records, transforms)
    rsa270_constraint()


if __name__ == "__main__":
    main()
