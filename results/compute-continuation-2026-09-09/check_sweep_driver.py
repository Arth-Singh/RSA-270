#!/usr/bin/env python3
"""Check that the sweep driver preserves evidence and propagates a failed job."""
import hashlib
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
out = Path(__file__).resolve().parent / "driver-check"
out.mkdir(exist_ok=False)
fixture = out / "fixture"
(fixture / "scripts").mkdir(parents=True)
(fixture / "src").mkdir()
shutil.copy2(root / "scripts/rsaref_state_sweep.sh", fixture / "scripts")
(fixture / "src/rsaref_md5_state_scan.cpp").write_text("// Driver failure fixture; not a factoring implementation.\n")
(fixture / "failing-scanner.sh").write_text(
    '#!/usr/bin/env bash\n'
    'if [ "$1" = self-test ]; then echo "self-test PASSED"; exit 0; fi\n'
    'echo "intentional scanner argument failure" >&2\nexit 2\n')
(fixture / "Makefile").write_text(
    'build/current/rsaref_md5_state_scan:\n'
    '\tmkdir -p build/current\n'
    '\tcp failing-scanner.sh $@\n'
    '\tchmod +x $@\n')
run = out / "failed-run"
command = ["bash", str(fixture / "scripts/rsaref_state_sweep.sh"), str(run), "1", "lcg"]
first = subprocess.run(command, text=True, capture_output=True)
(out / "failure.stdout").write_text(first.stdout)
(out / "failure.stderr").write_text(first.stderr)
ledger = (run / "RUN.txt").read_text()
assert first.returncode == 2
assert "FAILED: scanner exit=2 for ansi-rand w4" in ledger
assert "finished=" not in ledger
assert "driver_exit=2" in ledger
before = hashlib.sha256((run / "RUN.txt").read_bytes()).hexdigest()
second = subprocess.run(command, text=True, capture_output=True)
(out / "reuse.stdout").write_text(second.stdout)
(out / "reuse.stderr").write_text(second.stderr)
assert second.returncode == 2
assert before == hashlib.sha256((run / "RUN.txt").read_bytes()).hexdigest()
assert "refusing to overwrite" in second.stderr
print("PASS: scanner error propagated; no false finished marker; existing output preserved")
