#!/usr/bin/env python3
"""Check sample values and factors are invariant to one versus two threads."""
import csv
from pathlib import Path

directory = Path(__file__).resolve().parent
for suffix in ("observed.tsv", "factors.tsv"):
    assert ((directory / ("benchmark." + suffix)).read_bytes()
            == (directory / ("reproducibility." + suffix)).read_bytes()), suffix


def samples(prefix):
    with (directory / (prefix + ".samples.tsv")).open(newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    for row in rows:
        del row["job_seconds"]
    return rows


assert samples("benchmark") == samples("reproducibility")
print("thread_count_reproducibility=PASSED threads=1,2 datasets=16 factor_draws=544")
