# Repository Guidelines

## Scope and Structure

Folitex (formerly TeXLite) is a standalone C++20/Qt 6 Widgets/Poppler LaTeX editor.
Read `docs/ARCHITECTURE.md` first, and the local, untracked `HANDOFF.md` if present.

`src/editor.*` handles source editing and scrolling; `src/pdfview.*` implements
asynchronous tiled PDF rendering; `src/window.*` manages documents, compilation,
and SyncTeX. `src/main.cpp` starts the desktop UI. Functional tests live in
`tests/`, sample documents in `examples/`, and reports in `docs/`. Historical
measurements and scripts are retained in `research/`.

## Build and Development

Run `./run.sh` to launch the bundled binary or rebuild changed sources.
For explicit builds, use `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
-DBUILD_TESTING=ON`, then `cmake --build build --parallel 2`.
Dependencies include Qt 6 Widgets/Test, Poppler Qt6, CMake, a C++20 compiler,
latexmk and SyncTeX. Treat `build/` as generated output.

## Style and Reliability

Match existing four-space indentation, separate-line braces and Qt naming.
Keep rendering off the UI thread, bound image caches, and ignore obsolete render
results. Preserve source bytes and atomic-save guarantees. Compiler cancellation
must clean up descendant processes without targeting unrelated processes.

## Validation and Changes

Run `ctest --test-dir build --output-on-failure`. For DPI-sensitive work, run
`QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1.25 ./build/folitex_tests` and repeat
at scale 2. Verify native display behavior manually when available.

Use concise imperative commit subjects and describe validation. Distinguish REA
Evidence, source inspection and ordinary subprocess measurements. Preserve raw
benchmark records; record new binary hashes and limitations for new runs.
