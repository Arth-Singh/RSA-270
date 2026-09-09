# Scanner throughput on the Nebius VM, 2026-09-09

Machine: apricot-cougar-instance-3, AMD EPYC Genoa, 128 vCPU (64 cores, 2 threads each), 503 GiB RAM, Ubuntu 24.04, g++ 13.3. Repository at commit af0ab47, built with `make -j` (`-O3 -std=c++20`). Both runs are throughput measurements over ranges already known to contain no match; they are not new negative results for any hypothesis.

| Command | Wall | Program summary line | Exit |
| --- | ---: | --- | ---: |
| `./build/current/rsaref_md5_state_scan md5-word 0 268435456 128 4` | 3.49 s | `summary mode=md5-word candidates=268435456 tested_state_transition_variants=1073741824 result=none elapsed=3.497 rate_Mvariants_s=307.046` | 1 |
| `./build/current/seed_scan raw32 0 1073741824 128 0 be 1adcee7d7d9f02f0c4469c32a5307426` | 1.48 s | `result=none elapsed=1.479s nominal_rate=725.769 M/s` | 1 |

Implications: `rsaref_md5_state_scan` covers a full 32-bit word space (4 variants per seed) in about 56 s; `seed_scan raw32` covers it in about 6 s. Rates are for 128 threads on an otherwise idle machine.

CADO-NFS at the pinned commit built with `make -j128` after `apt-get install python3-flask` (the cmake configure step fails without it). The c59 sanity number from the CADO README factored in 61 s with `./cado-nfs.py 90377629292003121684002147101760858109247336549001090677693 -t 128` into 260938498861057, 588120598053661, 760926063870977, and 773951836515617.
