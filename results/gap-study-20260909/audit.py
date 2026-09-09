#!/usr/bin/env python3
"""Independently audit C++ observations and retained small-pilot factor draws."""
import argparse
import csv
import importlib.util
import math
from pathlib import Path


def load(path):
    with open(path, newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--observed", required=True)
    parser.add_argument("--factors", required=True)
    parser.add_argument("--samples", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location(
        "reconstruction", root / "src/rsaref_stream_reconstruct.py")
    reconstruction = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reconstruction)
    rows = reconstruction.load_rows(root / "data/solved_decimal_challenge.tsv")
    known = reconstruction.factor_recoveries(rows)
    by_name = {(rows[i]["label"], j + 1): recovery
               for (i, j), recovery in known.items()}
    observed_count = 0
    for row in load(args.observed):
        recovery = by_name[(row["label"], int(row["factor"]))]
        for field, reference in (("value", "prime"), ("bits", "bits"),
                                 ("predecessor", "previous"), ("gap", "gap")):
            assert int(row[field]) == recovery[reference], (row, field)
        observed_count += 1
    sums = {}
    factor_count, predecessor_candidates = 0, 0
    for row in load(args.factors):
        prime, previous = int(row["value"]), int(row["predecessor"])
        bits = int(row["bits"])
        low, width = 3 << (bits-2), 1 << (bits-2)
        assert low <= prime < low+width and low <= previous < low+width
        assert reconstruction.old_rsaref_accepts(prime)
        assert reconstruction.old_rsaref_accepts(previous)
        actual_previous = prime - 2
        if actual_previous < low:
            actual_previous += width
        while not reconstruction.old_rsaref_accepts(actual_previous):
            predecessor_candidates += 1
            actual_previous -= 2
            if actual_previous < low:
                actual_previous += width
        assert actual_previous == previous
        distance = (prime - previous) % width or width
        assert distance == int(row["gap"])
        key = (row["seed"], row["replicate"], row["model"])
        sums[key] = sums.get(key, 0.0) + distance / (2*math.log(prime))
        factor_count += 1
    for row in load(args.samples):
        key = (row["seed"], row["replicate"], row["model"])
        assert math.isclose(sums[key], float(row["normalized_gap_sum"]),
                            rel_tol=1e-14, abs_tol=1e-14)
    print("independent_python_audit=PASSED observed_factors={} simulated_factors={}"
          " rejected_predecessor_candidates={} dataset_scores={}".format(
              observed_count, factor_count, predecessor_candidates, len(sums)))


if __name__ == "__main__":
    main()
