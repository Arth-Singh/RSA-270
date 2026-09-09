#!/usr/bin/env python3
"""Stop only the identified old sweep after its word families have completed.

The remaining LCG ranges repeat every 2^24 seeds. This script is sent to the
authorized VM over SSH. It preserves the old logs and does not treat an
interrupted LCG range as a completed negative result.
"""
import datetime
import os
from pathlib import Path
import signal
import subprocess
import sys

driver = 36939
root = Path.home() / "RSA-270"
run = root / "results/rsaref-state-sweep-2026-09-09/RUN.txt"
expected = b"scripts/rsaref_state_sweep.sh\0results/rsaref-state-sweep-2026-09-09\0"
command = Path(f"/proc/{driver}/cmdline").read_bytes()
print("utc=", datetime.datetime.now(datetime.timezone.utc).isoformat(), flush=True)
print("driver=", driver, "command=", repr(command), flush=True)
if expected not in command:
    sys.exit("Refusing: driver command changed")
if "md5-repeat-buffer w4 exit=1 summary mode=md5-repeat-buffer candidates=4294967296 tested_state_transition_variants=17179869184 result=none" not in run.read_text():
    sys.exit("Refusing: final word-family completion is not yet recorded")
os.kill(driver, signal.SIGSTOP)
try:
    children = Path(f"/proc/{driver}/task/{driver}/children").read_text().split()
    scans = []
    for item in children:
        pid = int(item)
        path = Path(f"/proc/{pid}/cmdline")
        if not path.exists():
            continue
        cmd = path.read_bytes()
        print("child=", pid, "command=", repr(cmd), flush=True)
        if b"rsaref_md5_state_scan\0" in cmd:
            if not any(b"\0" + mode + b"\x000\x004294967296\0" in cmd
                       for mode in (b"ansi-rand", b"ms-rand", b"borland-rand")):
                raise RuntimeError("Refusing to interrupt an unexpected scanner")
            scans.append(pid)
    with run.open("a") as handle:
        handle.write("operator_stop=" + datetime.datetime.now(datetime.timezone.utc).isoformat()
                     + " reason=LCG streams repeat every 2^24 seeds; remaining LCG coverage will run separately; interrupted ranges are not completed negatives\n")
    for pid in scans:
        os.kill(pid, signal.SIGTERM)
        print("sent SIGTERM to redundant LCG scanner", pid, flush=True)
    os.kill(driver, signal.SIGTERM)
    print("sent SIGTERM to old sweep driver", driver, flush=True)
finally:
    try:
        os.kill(driver, signal.SIGCONT)
    except ProcessLookupError:
        pass
subprocess.run(["ps", "-o", "pid,ppid,stat,etimes,args", "-p", str(driver)], check=False)
