# TeXLite final prototype reconciliation

Ordinary subprocess and ELF observations. These are **not REA Evidence records**. This final run preserves the earlier v0.1 benchmark unchanged and binds every trial to the patched binary digest below.

## Frozen final snapshot

- Original executable: `/tmp/texlite-native/build/texlite`.
- Retained measured copy: `/tmp/texlite-final-benchmark/texlite-final-measured`.
- SHA-256: `95f7aecaefc044cf138f0efe402e76bc91a990996336aea05475fd9ada45b328`.
- Binary size: **342696 bytes**, unstripped, dynamically linked x86-64 ELF.
- Release build: C++20, `-O3 -DNDEBUG`.
- Direct dependencies: 10; resolved dependency files: 83. Their aggregate disk size is 111108592 bytes; this is not RAM or a complete installed application size.
- Current app source snapshot: 1279 lines / 50235 bytes across seven app source/header files, with per-file digests and retained copies.
- Parent reports the final tests passed at scales 1 and 1.25; this worker measured resource usage, rather than independently rerunning the functional test suite.

## Measurement method

Same corpus, checkpoints and lifecycle as both prior measurements: three owned subprocesses per scenario, each sampled at 0.10, 0.25, 0.50, 1, 2, 4 and 5 seconds. Root process RSS/PSS/private memory, VmHWM, threads and CPU ticks were recorded. Every trial checked the frozen binary hash before launch and after termination. No user process was killed.

All settings/cache/data locations and output were isolated under `/tmp/texlite-final-benchmark`. The prototype has no session persistence and was launched without `--config` or `--quit-after`. Source paths are positional; PDF mode uses `--pdf-only /tmp/texstudio-baseline/fixture.pdf`.

```text
QT_QPA_PLATFORM=offscreen /tmp/texlite-native/build/texlite [fixture.tex]
QT_QPA_PLATFORM=offscreen /tmp/texlite-native/build/texlite --pdf-only /tmp/texstudio-baseline/fixture.pdf
```

No Qt scaling environment override was inherited. The parent coordinated a pause of its own Qt screenshot/test processes during this final run. Unrelated host workloads, filesystem caches and sharing with external processes remain uncontrolled.

## Final medians at five seconds

| Scenario | Final RSS MiB | Final PSS MiB | Private MiB | CPU seconds total | CPU seconds during final wall second | Threads |
|---|---:|---:|---:|---:|---:|---:|
| Empty window | 60.1 | 32.2 | 23.8 | 0.12 | 0.00 | 3 |
| 103 lines / 7,436 bytes | 60.4 | 32.4 | 24.1 | 0.13 | 0.00 | 3 |
| 5,003 lines / 387,836 bytes | 81.9 | 54.0 | 45.7 | 0.36 | 0.00 | 3 |
| 50,003 lines / 3,977,836 bytes | 284.4 | 256.3 | 247.9 | 1.81 | 0.00 | 3 |
| Generated 12-page PDF, PDF-only mode | 68.6 | 40.4 | 32.0 | 0.13 | 0.00 | 3 |

**CPU seconds are not startup wall latency.** CPU quiescence during the final sampled interval is not proof of input responsiveness, interactive readiness, first paint or scrolling FPS.

## Comparison to earlier snapshots

| Scenario | TeXstudio RSS / PSS MiB | v0.1 RSS / PSS MiB | Final RSS / PSS MiB |
|---|---:|---:|---:|
| Empty window | 123.6 / 85.2 | 60.1 / 31.6 | 60.1 / 32.2 |
| 103 lines / 7,436 bytes | 135.2 / 96.4 | 60.2 / 31.7 | 60.4 / 32.4 |
| 5,003 lines / 387,836 bytes | 183.6 / 144.6 | 81.9 / 53.4 | 81.9 / 54.0 |
| 50,003 lines / 3,977,836 bytes | 630.4 / 591.5 | 284.1 / 255.6 | 284.4 / 256.3 |
| Generated 12-page PDF, PDF-only mode | 147.1 / 108.3 | 68.8 / 40.0 | 68.6 / 40.4 |

The 50,003-line TeXstudio value at five seconds was still transient; its separate one-run 20-second follow-up stabilized near 685.5 MiB RSS / 645.8 MiB PSS by eight seconds. Do not relabel the five-second baseline as settled, or its follow-up as a three-trial median.

## Interpretation and limits

- The final normal-case footprint remains close to v0.1. Changes that fix encoding/IO safety, failed-render handling, process cancellation, and huge-viewport caching are not fully exercised by these normal startup/resource scenarios.
- TeXLite is a much smaller prototype. TeXstudio completion/checking/project/session/PDF features are not matched, so resource differences are not a feature-equivalent benchmark or language-performance comparison.
- Both measured applications are C++/Qt; these results cannot establish a Rust-versus-C++ ranking.
- The prototype window initially uses 1280×820; TeXstudio viewport/zoom/geometry were not standardized. PDF workloads therefore are illustrative rather than an equal-pixel rendering benchmark.
- The renderer has an adaptive configured cache allowance (normally 32 MiB, up to 128 MiB for oversized viewports). This setting is not measured allocated RAM, does not include every in-flight/library/UI allocation, and is not a process-memory ceiling.
- The large source still consumes substantial process memory. A later lower-memory buffer/editor design should be assessed separately on the same corpus.
- The sandbox cannot access the real Wayland display, GPU, input stream, host application processes or mixed-DPI movement. Native scroll frame pacing and display quality remain manual/host validation work.
- The small executable relies on installed Qt, Poppler and transitive native libraries. Its file size is not a self-contained installation size.

## Retained evidence files

- `artifact-inventory.json`: frozen identity, source digests and dependency inventory.
- `measurements.json`: all 15 trials, exact command lines and timestamped checkpoints.
- `summary.json`: final medians.
- `comparison.json`: final, v0.1 and TeXstudio statistics.
- `measure.py`: exact measurement script; `make_report.py`: report generator.
- `runs/*/{stdout,stderr}.log`: per-trial application output.
- `source-snapshot/`: measured app source plus build/test/launch files.
- `texlite-final-measured`: retained measured executable.

Earlier v0.1 files remain unchanged under `/tmp/texlite-benchmark`; TeXstudio baseline files remain under `/tmp/texstudio-baseline`.

Generated: 2026-11-06T18:03:42.530941+07:00
