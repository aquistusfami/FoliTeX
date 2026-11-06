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

Folitex builds from source. It is developed and tested on Linux; Windows is untested
(see below). You need:

| Needed for | Arch Linux | Debian / Ubuntu (untested) |
|---|---|---|
| Compiler and build tool (C++20, CMake ≥ 3.24) | `gcc` `cmake` | `build-essential` `cmake` |
| Interface (Qt ≥ 6.5) | `qt6-base` | `qt6-base-dev` |
| SVG icons (tab close, side bar) | `qt6-svg` | `qt6-svg-plugins` |
| PDF viewer | `poppler-qt6` | `libpoppler-qt6-dev` |
| Building documents (`latexmk`) | `texlive-binextra` | `latexmk` |
| TeX engines and `synctex` | `texlive-bin` `texlive-latexextra` | `texlive-latex-extra` |
| Vietnamese documents (vntex, T5 encoding) | `texlive-langother` | `texlive-lang-other` |

Common problems:
- Blank icons: the Qt SVG image plugin (`qt6-svg`) is missing.
- `Encoding scheme 'T5' unknown` or `Unicode character … not set up` when building:
  the language package for your document (e.g. `texlive-langother`) is missing.
- No package-name completion: `kpsewhich` (TeX Live) is not on `PATH`.

On Arch Linux:

```bash
sudo pacman -S --needed gcc cmake qt6-base qt6-svg poppler-qt6 \
    texlive-binextra texlive-bin texlive-latexextra texlive-langother
```

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

### Windows (experimental, untested)

Folitex has not been built on Windows yet. The code avoids Linux-only calls outside
`#ifdef Q_OS_UNIX`, so a build is expected to work with [MSYS2](https://www.msys2.org/)
(UCRT64 shell):

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,qt6-base,qt6-svg,poppler-qt6}
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build
```

Install [TeX Live for Windows](https://tug.org/texlive/windows.html), which includes
Perl, `latexmk`, `synctex` and the `ls-R` database. MiKTeX also works for building, but
needs a separate Perl (e.g. Strawberry Perl) for `latexmk` and gives no package-name
completion. Known differences on Windows:

- Stopping a build ends `latexmk` but may leave its `pdflatex` child running.
- There is no installer yet; run `build/folitex.exe` from the MSYS2 shell, or copy the
  Qt and Poppler DLLs next to it (`windeployqt`).

Reports of Windows builds — working or not — are welcome as issues.

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
  125 % and 200 % scale. Windows and macOS are untested (see Installing).

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
