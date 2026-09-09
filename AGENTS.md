# RSA-270 workspace instructions

Read [CONTEXT.md](CONTEXT.md) before continuing the research. Consult [docs/EXPERIMENTS.md](docs/EXPERIMENTS.md) for completed and incomplete tests, and [docs/SOURCES.md](docs/SOURCES.md) for provenance and missing artifacts. [README.md](README.md) contains local build and restart instructions.

## Current state

- The objective is to factor the exact RSA-270 challenge number in CONTEXT.md.
- No nontrivial RSA-270 factor or usable generator state has been recovered.
- This directory became the working copy on 2026-09-09. The original is /Users/arth_rogerthat/rsa-270.
- The historical one-day deadline was 2026-09-04T18:04:30+09:00. It expired; do not silently renew it or report it as active.
- The old goal was paused at handoff. A new session should follow the user's current request.
- Preserve source and experimental evidence. Distinguish confirmed arithmetic, recovered behavior of a specific binary, conditional models, and unverified historical reports.
- Prefix 11 and residue 2 modulo 3 do not identify an exact generator or establish a factoring weakness.
- Only claim success after checking exact p*q == N with nontrivial factors and independently checking their primality. A scanner's exit code or anchor match is not sufficient.
- Rebuild before trusting copied executables. In particular, build/seed_scan predates the final source change.
- Use this directory for new files and results. Save experiment commands, source hashes, complete output, and limitations under results/ instead of /tmp.
- Do not assume old agents, process IDs, tool-session IDs, or timers remain available.

## User communication preferences

Respond tersely, like a smart caveman: remove filler, keep technical substance, preserve negations and qualifiers that affect correctness. Fragments are acceptable. Use clear normal language for security warnings, irreversible actions, or confusion. Write code, comments, documentation, and other persisted artifacts in normal prose. User can disable this style with "stop caveman" or "normal mode".

## Time awareness

Use injected timer context and native clock reminders for routine timing. Do not call codex-time status unless the user requests timer status. Continue the current task's running timer. If no trusted hook or timer exists, start one with a short label. Honor any newly supplied budget or deadline.

Use the latest injected checkpoint at phase boundaries and after long waits. Work: continue; converge: stop expanding scope; verify: check the core result and prepare delivery; overtime: stop optional work and report what remains. Run codex-time stop immediately before a final response when timing applies. Use codex-time now when exact current time matters. Skip timing for quick answers requiring no tools.

