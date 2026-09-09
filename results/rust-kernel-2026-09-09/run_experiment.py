#!/usr/bin/env python3
"""Build, verify and compare two general MD1 implementations on this machine.

One parent process measures every child with its own perf_counter_ns clock.
No timestamp or monotonic origin is compared across different processes.
Benchmarks run sequentially, one active child at a time.
"""
import csv
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import statistics
import subprocess
import time

DIRECTORY = Path(__file__).resolve().parent
ROOT = DIRECTORY.parents[1]
RUN = DIRECTORY / "run"
BIN = ROOT / "build/rust-kernel"
SEED = 20260909
PAIRS = 6


def postincrement(data):
    state = bytearray(data)
    for i in range(15, -1, -1):
        old = state[i]
        state[i] = (old + 1) % 256
        if old:
            break
    return bytes(state)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    RUN.mkdir(exist_ok=True)
    BIN.mkdir(parents=True, exist_ok=True)
    if (RUN / "summary.json").exists():
        raise RuntimeError("completed run exists; preserve it before rerunning")
    with (RUN / "subprocesses.jsonl").open("w") as log:
        def invoke(args, phase, input_text=None, **extra):
            started = time.perf_counter_ns()
            result = subprocess.run([str(x) for x in args], input=input_text,
                                    text=True, capture_output=True, cwd=ROOT)
            elapsed = (time.perf_counter_ns() - started) / 1e9
            record = dict(phase=phase, argv=[str(x) for x in args],
                          cwd=str(ROOT), parent_elapsed_seconds=elapsed,
                          returncode=result.returncode, stdout=result.stdout,
                          stderr=result.stderr, **extra)
            if input_text is not None:
                record["stdin"] = input_text
            log.write(json.dumps(record, sort_keys=True) + "\n")
            log.flush()
            if result.returncode:
                raise RuntimeError("child failed: " + repr(record))
            return record

        rust_version = invoke(["rustc", "--version", "--verbose"], "metadata")
        cpp_version = invoke(["c++", "--version"], "metadata")
        hardware = invoke(["sysctl", "-n", "hw.model", "hw.machine", "hw.ncpu",
                           "hw.physicalcpu", "hw.logicalcpu", "hw.memsize",
                           "machdep.cpu.brand_string"], "metadata")
        binaries = {"cpp": BIN / "md1_cpp", "rust": BIN / "md1_rust"}
        cpp_flags = ["-O3", "-DNDEBUG", "-std=c++20", "-Wall", "-Wextra",
                     "-Wpedantic", "-mcpu=native", "-flto=full", "-Isrc"]
        rust_flags = ["--edition=2021", "-C", "opt-level=3", "-C",
                      "target-cpu=native", "-C", "lto=fat", "-C",
                      "codegen-units=1", "-C", "panic=unwind", "-D", "warnings"]
        invoke(["c++", *cpp_flags, "src/md1.cpp",
                DIRECTORY / "md1_cpp_bench.cpp", "-o", binaries["cpp"]], "build")
        invoke(["rustc", *rust_flags, DIRECTORY / "md1_rust.rs", "-o",
                binaries["rust"]], "build")

        kat = [
            ("kat-empty", b"", "7ae2a21d8793650044ae40277eafaa2e"),
            ("kat-abc", b"abc", "74594a0a73b03503efe62e574dbaf816"),
            ("kat-a", b"a", "420965f2da150b44cbfadd695ab5bbe9"),
            ("kat-digits", b"1234567890", "34dbffc3e4a9c0ad2d5559c99649c69e"),
        ]
        independent = [("binary16-{}".format(i), hashlib.sha256(
            b"RSA270 Rust performance correctness 20260909\0" + i.to_bytes(4, "big")
        ).digest()[:16], None) for i in range(128)]
        boundary = [("length-{}".format(n), bytes((13*i+197) % 256 for i in range(n)),
                     None) for n in (15, 17, 31, 32, 33, 48, 65)]
        carry = [bytes(16), bytes([255])*16, bytes(range(16)), bytes(range(255,239,-1))]
        for trailing_zeros in range(1,16):
            base = bytearray(hashlib.sha256(b"carry" + bytes([trailing_zeros])).digest()[:16])
            base[16-trailing_zeros:] = bytes(trailing_zeros)
            carry.append(bytes(base))
        carry.extend([bytes(14)+b"\xff\x00", bytes(14)+b"\x00\xff"])
        vectors = kat + independent + boundary
        vectors += [("carry-before-{}".format(i), x, None) for i,x in enumerate(carry)]
        vectors += [("carry-after-{}".format(i), postincrement(x), None)
                    for i,x in enumerate(carry)]
        input_text = "".join(data.hex()+"\n" for _,data,_ in vectors)
        digests = {language: invoke([binary,"vectors"], "digest-validation", input_text)
                   ["stdout"].splitlines() for language,binary in binaries.items()}
        assert len(digests["cpp"]) == len(vectors)
        assert digests["cpp"] == digests["rust"]
        with (RUN / "digest-vectors.tsv").open("w", newline="") as handle:
            writer = csv.writer(handle, delimiter="\t")
            writer.writerow(["label", "input_hex", "cpp_md1", "rust_md1", "known_answer"])
            for i,(label,data,expected) in enumerate(vectors):
                if expected is not None:
                    assert digests["cpp"][i] == expected, label
                writer.writerow([label,data.hex(),digests["cpp"][i],digests["rust"][i],expected or ""])
        carry_input = "".join(x.hex()+"\n" for x in carry)
        posts = {language: invoke([binary,"post-vectors"], "carry-validation", carry_input)
                 ["stdout"].splitlines() for language,binary in binaries.items()}
        expected_posts = [postincrement(x).hex() for x in carry]
        assert posts["cpp"] == posts["rust"] == expected_posts
        with (RUN / "carry-vectors.tsv").open("w", newline="") as handle:
            writer = csv.writer(handle, delimiter="\t")
            writer.writerow(["input_hex", "python_expected", "cpp_next", "rust_next"])
            writer.writerows(zip([x.hex() for x in carry],expected_posts,posts["cpp"],posts["rust"]))
        print("validation=PASSED kats=4 independent_binary16=128 digest_cases={} carry_cases={}"
              .format(len(vectors),len(carry)), flush=True)

        samples = []
        summaries = {}
        for workload_index,workload in enumerate(("full-state", "postincrement")):
            def bench(language, count, phase, **extra):
                record = invoke([binaries[language],"bench",workload,count,SEED],
                                phase, language=language, workload=workload, **extra)
                record["result"] = json.loads(record["stdout"])
                assert record["result"]["iterations"] == count
                return record
            warm = [bench(x, 1024, "warmup") for x in ("cpp", "rust")]
            assert warm[0]["result"] == warm[1]["result"]
            count = 4096
            while True:
                trial = [bench(x,count,"calibration") for x in ("cpp","rust")]
                assert trial[0]["result"] == trial[1]["result"]
                if min(x["parent_elapsed_seconds"] for x in trial) >= 0.35:
                    break
                count *= 2
                if count > 2**22:
                    raise RuntimeError("calibration unexpectedly large")
            retained = []
            for pair in range(PAIRS):
                order = ("cpp", "rust") if (pair+workload_index)%2 == 0 else ("rust","cpp")
                pair_records = [bench(x,count,"timing",pair=pair,order=position)
                                for position,x in enumerate(order)]
                assert pair_records[0]["result"] == pair_records[1]["result"]
                assert min(x["parent_elapsed_seconds"] for x in pair_records) >= 0.2
                retained.extend(pair_records)
                samples.extend(pair_records)
            per_language = {}
            for language in ("cpp", "rust"):
                values = [x["parent_elapsed_seconds"] for x in retained
                          if x["language"] == language]
                median = statistics.median(values)
                per_language[language] = dict(seconds=values, median_seconds=median,
                    min_seconds=min(values), max_seconds=max(values),
                    median_absolute_deviation_seconds=statistics.median(abs(x-median) for x in values),
                    digests_per_second=count/median, nanoseconds_per_digest=median*1e9/count)
            ratios = []
            for pair in range(PAIRS):
                values = {x["language"]:x["parent_elapsed_seconds"]
                          for x in retained if x["pair"] == pair}
                ratios.append(values["cpp"] / values["rust"])
            summary = dict(iterations_per_process=count, timing_pairs=PAIRS,
                           implementations=per_language, paired_cpp_over_rust_ratios=ratios,
                           median_paired_cpp_over_rust_ratio=statistics.median(ratios),
                           ratio_of_median_cpp_over_rust=per_language["cpp"]["median_seconds"]/
                           per_language["rust"]["median_seconds"], output=retained[0]["result"])
            summaries[workload] = summary
            print(json.dumps({workload:summary},sort_keys=True),flush=True)

        report = dict(created_utc=datetime.now(timezone.utc).isoformat(),
            root=str(ROOT), base_commit="52f5860da06aa740d7fd24200d49e0a46a9051a0",
            platform=platform.platform(), python=platform.python_version(),
            hardware_fields=["model","machine","cpus","physical_cpus","logical_cpus",
                             "memory_bytes","brand"], hardware=hardware["stdout"].splitlines(),
            rust_compiler=rust_version["stdout"], cpp_compiler=cpp_version["stdout"],
            rust_flags=rust_flags, cpp_flags=cpp_flags,
            correctness=dict(kats=4, independent_binary16=128, digest_cases=len(vectors),
                             carry_cases=len(carry), all_benchmark_outputs_agree=True),
            timing_method="Each interval measured in one Python parent with its own perf_counter_ns; sequential children; no comparison of process clock origins.",
            workloads=summaries,
            limitations=["Specific sources, compilers, flags, CPU, and short local sample; not universal language speed.",
                         "Apple clang17 and rustc1.94 LLVM21 are different compiler toolchains.",
                         "Whole-process timing includes startup, deterministic input generation, and output folding.",
                         "No scanner, solver, seed search, or new factoring algorithm is implemented.",
                         "No CPU affinity or frequency pinning was applied; order alternates to reduce temporal bias."])
        files = [ROOT/"src/md1.cpp",ROOT/"src/md1.hpp",ROOT/"references/rsaref/raw_r_random.c",
                 *binaries.values(), *DIRECTORY.glob("*.rs"), *DIRECTORY.glob("*.cpp"),
                 Path(__file__).resolve()]
        report["sha256"] = {str(p.relative_to(ROOT)):sha256(p) for p in files}
        (RUN/"summary.json").write_text(json.dumps(report,indent=2,sort_keys=True)+"\n")
        with (RUN/"timings.tsv").open("w",newline="") as handle:
            writer=csv.writer(handle,delimiter="\t")
            writer.writerow(["workload","pair","order","language","iterations","seconds","fold"])
            for x in samples:
                writer.writerow([x["workload"],x["pair"],x["order"],x["language"],
                                 x["result"]["iterations"],x["parent_elapsed_seconds"],x["result"]["fold"]])
        print("experiment=COMPLETE",flush=True)


if __name__ == "__main__":
    main()
