# Folitex

A lean, native LaTeX editor with a built-in PDF viewer, written in C++20 with
Qt 6 Widgets and Poppler. It aims to start fast, stay light on memory, render PDFs
sharply at any display scale and scroll smoothly — while covering the everyday
LaTeX workflow: edit, build, preview, jump between source and PDF.

![Folitex editing a LaTeX file next to its PDF](docs/screenshot.png)

> Folitex was previously developed as **TeXLite**. Its behaviour was informed by
> studying TeXstudio and VS Code; it contains no code from either project and is
> not affiliated with them.

## Features

**Editor**
- Tabs, UTF-8 sources, LaTeX syntax highlighting, line numbers, current-line
  highlight, bracket matching, auto-closing `{}` `[]` `()`, indentation kept on Enter.
- Context-aware completion (Ctrl+Space, or automatically while typing):
  ~300 LaTeX/amsmath commands with snippet placeholders (Tab jumps to the next one),
  environments with automatic `\end{…}`, `\ref` labels and `\cite` keys from your
  document, your own `\newcommand`s, and package/class names from the installed TeX Live.
- Word wrap (Alt+Z or right-click → Word Wrap).
- Files edited by another program — another editor or an AI coding agent — reload
  silently while the tab has no unsaved edits; the cursor stays put and Ctrl+Z undoes
  the external change. Unsaved edits are never overwritten.
- Safe saving: atomic writes, BOM / NBSP / CRLF preserved, invalid UTF-8 refused.

**Build and preview**
- One-key asynchronous builds through `latexmk` with pdfLaTeX, LuaLaTeX or XeLaTeX;
  `% !TeX root = …` for multi-file projects; output goes to `.folitex-build/`.
- Two-way SyncTeX: F7 jumps from the source to the PDF, Ctrl+click in the PDF jumps back.
- PDF viewer: continuous pages, select and copy text, clickable links, zoom, page
  jump, automatic reload after each build, right-click menu with Jump to Source.
- Background rendering at the display's native pixel density with a bounded image
  cache; image-heavy pages render in a single Poppler call (≈7× faster than small tiles).

**Workspace**
- Open a folder as a workspace (Ctrl+Shift+E) to browse TeX/BibTeX/PDF files.
- Outline of sections, labels, included files and TODOs; click to jump.
- Minimal dark interface with File / Edit / Insert / Build / View menus.

## Installing

Folitex currently builds from source on Linux. You need a C++20 compiler, CMake ≥ 3.24,
Qt ≥ 6.5 (Widgets, plus the SVG image plugin), Poppler's Qt 6 bindings, and a TeX
distribution providing `latexmk` and `synctex`.

On Arch Linux:

```bash
sudo pacman -S --needed gcc cmake qt6-base qt6-svg poppler-qt6 texlive-binextra texlive-bin
# Vietnamese documents (vntex / T5 encoding) also need:
sudo pacman -S --needed texlive-langother
```

On Debian/Ubuntu the equivalents should be `build-essential cmake qt6-base-dev
qt6-svg-plugins libpoppler-qt6-dev latexmk` (untested; reports welcome).

Build, test and install for the current user (no root needed):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix ~/.local
```

This installs `~/.local/bin/folitex`, a desktop entry and an icon, so Folitex appears
in your application launcher and `folitex paper.tex` works from a terminal
(make sure `~/.local/bin` is on your `PATH`). For development, `./run.sh [files…]`
rebuilds when sources changed and starts the app.

## Using Folitex

Open a `.tex` file, press **F5** to build, and the PDF appears on the right. Settings
(workspace folder, word wrap) are stored in `~/.config/Folitex/`.

Keyboard shortcuts follow TeXstudio where TeXstudio documents them.

| Key | Action |
|---|---|
| F5 / F6 | Build, or stop a running build |
| F7 or Ctrl+Enter | Show the cursor position in the PDF |
| Ctrl+click in the PDF | Jump to the matching source line |
| Ctrl+Space | Complete command, environment, package, `\ref` or `\cite` |
| Tab / Ctrl+Right / Ctrl+Left | Next / next / previous snippet placeholder |
| Ctrl+B / Ctrl+I | `\textbf{}` / `\textit{}` (wraps the selection) |
| Ctrl+Shift+M / Alt+Shift+M | Inline math `$…$` / display math `\[…\]` |
| Ctrl+Shift+I / Ctrl+E | `\item` / insert an environment |
| Alt+Return | Close the last open environment or brace |
| Ctrl+/ | Toggle comment |
| Alt+Z | Toggle word wrap |
| Ctrl+F / F3 / Shift+F3 / Ctrl+G | Find / next / previous / go to line |
| Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S / Ctrl+W | New / open / save / save as / close |
| Ctrl+Shift+E | Open a workspace folder |
| Ctrl+PgDown / Ctrl+PgUp | Next / previous tab |
| Ctrl+= / Ctrl+- / Ctrl+0 | Zoom the PDF in / out / fit width |
| Ctrl+\ / Ctrl+J / Ctrl+Shift+O | Toggle PDF / build log / side bar |

In the PDF, drag to select text, double-click to select a word, Ctrl+C to copy and
click links to follow them. Right-click for Jump to Source, Copy, zoom and Open in
External Viewer.

## Status and limitations

Folitex is young (0.2) and deliberately smaller than TeXstudio. Known gaps:

- No spell or grammar checking, no full LaTeX parser, no session restore.
- The engine comes from the combo box; `% !TeX program` is not read yet.
- Build errors are shown as the raw log, not as a clickable list.
- The outline covers the current file only; `\cite` completion reads `.bib` files
  next to open sources.
- PDF text selection stays within one page; there is no PDF text search yet.
- Very large sources (tens of thousands of lines) use noticeably more memory, because
  the editor keeps pixel-smooth scrolling (Qt `QTextEdit`).
- Developed on Linux (Arch, Wayland); the automated tests run offscreen at 100 %,
  125 % and 200 % scale. Other platforms are untested.

## Documentation

- [Architecture](docs/ARCHITECTURE.md) — how the editor, renderer and build runner fit together.
- [Changelog](CHANGELOG.md).
- [Research and measurements](docs/REA-ANALYSIS.md), [benchmark report](docs/BENCHMARK.md)
  and [validation log](docs/VALIDATION.md) from the prototype phase, plus later
  measurements under [`research/`](research/). These records keep the former name TeXLite.

## Contributing

Issues and pull requests are welcome. Please run the test suite before submitting —
`ctest --test-dir build --output-on-failure`, and for rendering or layout changes also
`QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1.25 ./build/folitex_tests` and scale `2`.
Code style: four-space indentation, braces on their own line, Qt naming. Rendering
stays off the UI thread and image caches stay bounded. See [AGENTS.md](AGENTS.md) for
the full guidelines.

## License

GPL-3.0-only. See [LICENSE](LICENSE).
