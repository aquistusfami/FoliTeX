# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Folitex (formerly TeXLite; historical records under `research/`, `artifacts/` and `docs/BENCHMARK.md` keep that name) is a standalone C++20 / Qt 6 Widgets / Poppler-Qt6 LaTeX editor + PDF viewer prototype, built after analysing TeXstudio 4.9.9. Read the local, untracked `HANDOFF.md` (if present) for status and open priorities; `docs/ARCHITECTURE.md` is the public overview.
## Commands

```bash
./run.sh                                         # launch; rebuilds into build/ only if src/*.cpp|h or CMakeLists.txt are newer than the binary
./run.sh ./examples/demo.tex ./examples/demo.pdf # open the sample (F5 builds it, needed before SyncTeX works)

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure       # sets QT_QPA_PLATFORM=offscreen

# single test (Qt Test function name, see tests/tests.cpp private slots)
QT_QPA_PLATFORM=offscreen ./build/folitex_tests tiledPdf

# DPI-sensitive changes: also run at scale 1.25 and 2
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1.25 ./build/folitex_tests
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 ./build/folitex_tests
```

Tests need `latexmk`, a TeX engine and `synctex` on PATH (compilation/SyncTeX tests run real builds). No linter beyond `-Wall -Wextra -Wpedantic`.

`bin/texlite` (untracked) is the Release binary measured under the former name TeXLite; `run.sh` runs `build/folitex`. `bin/`, `artifacts/` and `HANDOFF.md` are git-ignored. `build/` is generated. Install the app with `cmake --install build --prefix ~/.local`.

## Architecture

All app code is in `folitex_lib` (`src/editor.*`, `src/pdfview.*`, `src/window.*`), linked by both `folitex` (`src/main.cpp`) and `folitex_tests` (`tests/tests.cpp`, one Qt Test class).

- **Editor** (`editor.*`): `QTextEdit` subclass with `LatexHighlighter` and a small `QCompleter`. `SmoothScroll` is an event filter installed on scroll areas: touchpad pixel deltas scroll directly, mouse wheel animates (140 ms `QPropertyAnimation`). Shared by editor and PDF view.
- **PdfView** (`pdfview.*`): `QAbstractScrollArea` that lays out pages continuously and requests 512×512 px `Tile`s. `PdfRenderer` lives on a dedicated `QThread` and owns the Poppler document (opened from an in-memory snapshot so latexmk can rewrite the file mid-render). Communication is signals only (`loadRequested`/`tileRequested` → `loaded`/`rendered`). Every load bumps `m_generation`; results whose `generation` or `dpi1000` no longer match are dropped. Rendered tiles go into a cost-bounded `QCache` (32 MiB, grows with the visible area up to 128 MiB); `renderDensity()` caps raster density for huge viewports to stay in budget. Failed pages are recorded in `m_failedPages` so they aren't re-requested forever.
- **Window / BuildRunner** (`window.*`): tabs, save/load, compilation, SyncTeX.
  - `BuildRunner::rootFile()` follows `% !TeX root = ...` magic comments; output goes to `.folitex-build/` next to the root (`pdfPath()`). Build runs `latexmk -<engine> -synctex=1 -outdir=.folitex-build`.
  - The compiler is started in its own session (`setsid` via `setChildProcessModifier`). Cancel = SIGTERM to the process group, then SIGKILL after 1.5 s — but only if `/proc/<pid>/stat` start time still matches, so a reused PID is never targeted.
  - Saves use `QSaveFile`; loading rejects invalid UTF-8 and U+2028/U+2029, and preserves BOM, NBSP and the file's newline style.
  - SyncTeX runs the `synctex` CLI asynchronously (`syncProcess`); PDF reload is driven by `QFileSystemWatcher` + a debounce `QTimer`.

## Rules (from AGENTS.md)

- Style: 4-space indent, braces on their own line, Qt naming (`m_` members). UI strings and docs are English.
- Keep rendering off the UI thread, keep image caches bounded, and ignore stale render results (generation/DPI check).
- Preserve source bytes and atomic-save guarantees.
- Compiler cancellation must kill descendant processes but never unrelated ones.
- Docs: distinguish REA Evidence vs. source inspection vs. ordinary subprocess measurements; don't call shell measurements "REA Evidence". Keep raw benchmark records in `research/`/`docs/`; record new binary hashes and limitations for new runs. Paths under `/tmp/...` in research scripts are historical.
- Commits: concise imperative subject, describe validation performed.
