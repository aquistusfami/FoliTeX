# RAM after PDF scrolling and zoom — 2026-11-06

Ordinary subprocess measurements (not REA Evidence). `harness.cpp` links `folitex_lib`,
runs offscreen at 1280×820, scale 1, and prints `/proc/self/smaps_rollup` plus glibc
`mallinfo2` after each step: empty window, a 330-line report, its 15-page image-heavy
PDF (3.2 MB), scrolling every page, four zoom steps (×2.4), `\usepackage{` completion,
and `research/texstudio-baseline/source-5000.tex`. Two runs per setting; they agreed
within 0.2 MiB. The report and PDF are private user files and are not included.

Build: `cmake -S research/ram-2026-11-06 -B /tmp/ram && cmake --build /tmp/ram`
(after building `folitex_lib` in `build/`).

| Step (MiB) | RSS default | RSS fixed mmap threshold | PSS default | PSS fixed |
|---|---:|---:|---:|---:|
| Empty window | 63.1 | 63.2 | 25.8 | 25.7 |
| + report.tex | 63.7 | 63.9 | 26.2 | 26.2 |
| + PDF first view | 73.1 | 72.8 | 34.1 | 33.6 |
| After scrolling all pages | 97.2 | 96.1 | 58.2 | 56.9 |
| After zoom ×2.4 | 123.8 | 102.4 | 84.8 | 63.2 |
| Package completion | 124.9 | 103.5 | 85.7 | 64.1 |
| + 5,003-line source | 148.0 | 125.3 | 108.8 | 85.9 |

With glibc's default dynamic threshold, 24.7 MiB of freed heap stayed mapped after
zooming (`malloc_trim` recovered 18.5 MiB RSS). `mallopt(M_MMAP_THRESHOLD, 128 KiB)` in
`main.cpp` (measured here as `GLIBC_TUNABLES=glibc.malloc.mmap_threshold=131072`) keeps
free heap near 3 MiB. Wall time of the whole scripted run: 11.03–11.14 s default,
11.05–11.16 s fixed (three runs each), so no measurable rendering cost.
`glibc.malloc.arena_max=2` saved a further ~1 MiB and was not adopted.

Remaining large items: the bounded tile cache (25–32 MiB while a PDF is open) and the
`QTextEdit` layout of large sources (+22 MiB for 5,003 lines).
