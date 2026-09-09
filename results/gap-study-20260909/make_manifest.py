#!/usr/bin/env python3
"""Record current local experiment provenance without relying on /tmp files."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import subprocess

directory = Path(__file__).resolve().parent
root = directory.parents[1]
paths = [root / "data/solved_decimal_challenge.tsv",
         root / "references/rsaref/raw_prime-old.c",
         root / "references/rsaref/raw_r_keygen.c",
         root / "src/rsaref_stream_reconstruct.py",
         root / "build/gap-study/gap_monte_carlo"]
paths += [p for p in directory.iterdir()
          if p.is_file() and p.name != "manifest.json"]
hashes = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
          for path in sorted(paths)}
report = {
    "created_utc": datetime.now(timezone.utc).isoformat(),
    "workspace": str(root),
    "base_commit": "44a3da5",
    "platform": platform.platform(),
    "machine": platform.machine(),
    "python": platform.python_version(),
    "compiler": subprocess.check_output(["c++", "--version"], text=True),
    "gmp_pkg_config_version": subprocess.check_output(
        ["pkg-config", "--modversion", "gmp"], text=True).strip(),
    "sha256": hashes,
    "scope": "Local matched-control pilot; no VM work; no RSA-270 factor recovered.",
}
print(json.dumps(report, indent=2, sort_keys=True))
