#!/usr/bin/env python3
"""Summarize direct independent Monte Carlo draws; no bootstrap or gap weights."""
import argparse
import csv
import json
import math
import statistics


def binomial_cdf(k, n, probability):
    """P[X <= k], evaluated by stable tail summation and symmetry."""
    if k < 0:
        return 0.0
    if k >= n or probability <= 0:
        return 1.0
    if probability >= 1:
        return 0.0
    if k >= n * probability:
        return 1.0 - binomial_cdf(n - k - 1, n, 1.0 - probability)
    log_mass = (math.lgamma(n + 1) - math.lgamma(k + 1)
                - math.lgamma(n - k + 1) + k * math.log(probability)
                + (n - k) * math.log1p(-probability))
    mass, total = 1.0, 1.0
    for j in range(k, 0, -1):
        mass *= j * (1.0 - probability) / ((n - j + 1) * probability)
        total += mass
        if mass < total * 1e-16:
            break
    return min(1.0, math.exp(log_mass) * total)


def invert_cdf(k, n, target):
    low, high = 0.0, 1.0
    for _ in range(80):
        middle = (low + high) / 2
        if binomial_cdf(k, n, middle) > target:
            low = middle
        else:
            high = middle
    return (low + high) / 2


def clopper_pearson(k, n, alpha=0.05):
    """Equal-tail exact binomial interval (up to floating point evaluation)."""
    lower = 0.0 if k == 0 else invert_cdf(k - 1, n, 1 - alpha / 2)
    upper = 1.0 if k == n else invert_cdf(k, n, alpha / 2)
    return [lower, upper]


def self_test():
    # Independent finite binomial sum validates the CDF in both tails.
    worst = 0.0
    for n in (1, 2, 10, 50, 100):
        for p in (0.001, 0.1, 0.5, 0.9, 0.999):
            for k in range(n + 1):
                direct = math.fsum(math.comb(n, j) * p**j * (1-p)**(n-j)
                                   for j in range(k + 1))
                worst = max(worst, abs(binomial_cdf(k, n, p) - direct))
    assert worst < 1e-12, worst
    zero = clopper_pearson(0, 100)
    assert abs(zero[1] - (1 - 0.025**0.01)) < 1e-12
    middle = clopper_pearson(50, 100)
    assert abs(middle[0] + middle[1] - 1) < 1e-12
    print(json.dumps({"analyzer_self_test": "PASSED", "worst_cdf_error": worst}))


def load(path):
    with open(path, newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("samples", nargs="*")
    parser.add_argument("--observed")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not args.observed or not args.samples:
        parser.error("--observed and sample TSV files are required")
    observed = load(args.observed)
    if len(observed) != 34:
        raise ValueError("expected exactly 34 observed factors")
    statistic = sum(float(row["normalized_gap"]) for row in observed)
    groups = {"upward_scan": [], "uniform_accepted": []}
    seen = set()
    for path in args.samples:
        for row in load(path):
            key = (int(row["seed"]), int(row["replicate"]), row["model"])
            if key in seen:
                raise ValueError("duplicate Monte Carlo draw: " + repr(key))
            seen.add(key)
            groups[row["model"]].append(float(row["normalized_gap_sum"]))
    report = {
        "statistic": "sum(previous accepted cyclic gap / (2 * ln(output value)))",
        "observed": statistic,
        "factors": 34,
        "sampling": "Direct independent datasets with actual 34 bit lengths; no empirical-pool bootstrap or importance weights.",
        "interpretation": "Conditional model comparison only; statistic was selected after inspecting this corpus, so p values are not a confirmatory historical-attribution test.",
        "models": {},
    }
    for model, values in groups.items():
        if not values:
            continue
        n = len(values)
        k = sum(value <= statistic for value in values)
        values.sort()
        report["models"][model] = {
            "datasets": n,
            "mean": statistics.mean(values),
            "sd": statistics.stdev(values) if n > 1 else None,
            "min": values[0],
            "median": statistics.median(values),
            "max": values[-1],
            "lower_tail_count": k,
            "lower_tail_frequency": k / n,
            "lower_tail_probability_95pct_clopper_pearson": clopper_pearson(k, n),
            "monte_carlo_plus_one_p": (k + 1) / (n + 1),
            "mean_mc_standard_error": statistics.stdev(values) / math.sqrt(n) if n > 1 else None,
        }
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
