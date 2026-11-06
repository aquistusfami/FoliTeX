# TeXstudio native baseline

Ordinary subprocess and ELF observations. These are **not REA Evidence records**. No REA engine, decompiler, GPU or host desktop was used by this worker.

## Target identity

- Installed package: `texstudio 4.9.9-1` on Arch Linux.
- Binary: `/usr/bin/texstudio`, stripped ELF x86-64, 24,551,488 bytes (23.41 MiB).
- SHA-256: `605eb300dc23f2df2a588de08dbd19d5acd29eac6e31cb9f90ba4c0361ed9b3e`.
- ELF Build ID: `81b14f4853c6c658cb3d3b615ddfa3f43ad7d15c`.
- Installed package size reported by pacman: 97.59 MiB.
- 20 direct `DT_NEEDED` dependencies; 101 resolved dependency files total 125,422,880 disk bytes (~119.61 MiB). Disk file size is not resident memory.

## Measurement method

Each scenario launches a new owned subprocess with a fresh settings directory. All generated files, settings and XDG caches/data stay under `/tmp/texstudio-baseline`. Existing user settings and documents are never opened or changed.

```text
QT_QPA_PLATFORM=offscreen texstudio --config <fresh-dir> --start-always --no-session [fixture]
```

RSS/PSS/private memory, threads and process CPU ticks were sampled at 0.10, 0.25, 0.50, 1, 2, 4 and 5 seconds. Each scenario has three sequential trials, with filesystem caches uncontrolled/warm. Metrics refer to the TeXstudio process, including its threads, not a sum of unrelated host processes or later LaTeX compiler children. Owned processes are terminated after sampling.

## Five-second medians

| Scenario | RSS MiB | PSS MiB | Private MiB | CPU seconds total | CPU seconds in final wall second | Threads |
|---|---:|---:|---:|---:|---:|---:|
| Empty editor | 123.6 | 85.2 | 72.6 | 0.72 | 0.00 | 14 |
| 103-line source (7,436 bytes) | 135.2 | 96.4 | 83.7 | 0.98 | 0.00 | 33 |
| 5,003-line source (387,836 bytes) | 183.6 | 144.6 | 131.9 | 1.50 | 0.00 | 31 |
| 50,003-line source (3,977,836 bytes) | 630.4 | 591.5 | 578.8 | 5.07 | 1.70 | 31 |
| PDF viewer only, generated 12-page PDF | 147.1 | 108.3 | 95.8 | 0.77 | 0.00 | 22 |

The 50,003-line scenario is still busy at five seconds; its five-second memory is a transient value, not an idle baseline. The other scenarios show no additional CPU ticks during the final sampled second, which establishes process CPU quiescence for that interval, not interactive readiness.

## Large-source follow-up

One additional 20-second trial reached about 685.5 MiB RSS and 645.8 MiB PSS by 8 seconds and remained near those values through 20 seconds. Process CPU total was 7.07 seconds. At 20 seconds, mapping-level PSS was approximately:

- Heap: 277.3 MiB.
- Other anonymous mappings: 320.6 MiB.
- TeXstudio binary mappings: 15.3 MiB.
- Qt6 library mappings: 19.9 MiB.
- Poppler library mappings: 1.3 MiB.
- Other file mappings: 11.3 MiB.

These observations locate most of the large-source memory in allocated state. They do not identify an allocating class, feature, call stack, leak or the exact cost of a linked library, because allocations performed by libraries also appear in heap/anonymous mappings.

The fresh config file acquired `startupCompletion=complete` around 1.695 seconds in this run, while CPU-intensive source processing continued after that. Therefore this marker must not be reported as complete document readiness or first-paint latency.

## Reconstruction implications

1. Benchmark source parsing/editor state separately from the empty shell and PDF renderer. A language change alone is not evidence of improved memory or speed.
2. Bound syntax/completion/outline data per document and update it incrementally. Avoid duplicating the complete source into independent feature caches. This is a design recommendation, not a causal diagnosis of TeXstudio internals.
3. Render only visible PDF tiles/pages, bound raster-cache bytes, and release old-resolution images on eviction. PDF-library disk size alone cannot predict document memory.
4. Compare a rewrite against the same fixtures and matched feature scope. Report RSS and PSS alongside startup-to-document-ready time; measure native Wayland scrolling and input latency separately.

## Limits and remaining unknowns

- This sandbox has no access to the host display, GPU, existing TeXstudio session or its real memory usage.
- Offscreen Qt rendering, font selection, backend allocation and timing can differ from native Wayland.
- No first-paint, input-to-render latency, smooth-scroll frame pacing or real mixed-DPI movement was measured.
- No matched-feature Rust/C++ prototype has been benchmarked here. No language-performance ranking follows from these results.
- No allocation stack profiling was performed; per-feature memory attribution remains unresolved.
- The large-source settled value is one follow-up trial, not a three-trial median.

## Files

- `artifact-inventory.json`: target identity, direct dependencies and resolved dependency disk sizes.
- `measurements.json`: all 15 short-trial observations and commands.
- `summary.json`: five-second medians.
- `large-source-followup.json`: the 20-second large-source trial.
- `large-source-mapping-summary.json`: mapping-level RSS/PSS/private totals.
- `measure.py`, `followup.py`: reproducible measurement scripts.
- `fixture.tex`, `fixture.pdf`, `source-*.tex`: generated, non-user-owned fixtures.

Generated: 2026-11-06T17:40:41.000747+07:00
