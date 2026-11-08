# Architecture

FoliTeX is a single Qt Widgets application. All logic lives in a static library,
`folitex_lib` (`src/editor.*`, `src/pdfview.*`, `src/window.*`), linked by the app
(`src/main.cpp`) and by the Qt Test suite (`tests/tests.cpp`).

## Editor (`src/editor.*`)

- `Editor` is a `QTextEdit` (not `QPlainTextEdit`) so scrolling stays pixel-smooth;
  the cost is that Qt lays out the whole document, roughly 3 KB per line.
- `SmoothScroll` is an event filter shared with the PDF view: touchpad pixel deltas
  scroll directly, mouse wheels animate for 140 ms.
- `LatexHighlighter` scans each line once, keeping brace depth and the mode (text, inline
  or display maths, a math environment, verbatim) in the block state so maths and listings
  continue across lines. Command arguments (headings, keys, environment names, `\text{}`
  in maths) are coloured after the scan, only where nothing else set a colour.
- Completion picks a context from the text before the cursor (command, `\begin{`,
  `\ref{`, `\cite{`, `\usepackage{` / `\documentclass{`) and fills a `QCompleter`.
  Templates mark placeholders with U+2022; the mark is never inserted into the source,
  placeholder positions are tracked with `QTextCursor`s instead. User macros, labels and
  environments are rescanned per document revision; `.bib` keys are cached by file
  modification time; package and class names come from TeX Live's `ls-R` files, read on a
  background thread when the first editor opens.
- Commands of loaded packages: `src/completiondata.cpp` holds curated entries per package;
  other packages are scanned once for `\newcommand`-style definitions (plus the packages
  they require, one level). Documents over 5,000 lines are rescanned only when typing pauses.
- `Editor::outline()` and `Editor::closingFor()` are pure functions over text, which
  keeps them easy to test.
- `\ref`/`\cite` completion walks the project from the root (`BuildRunner::rootFile`)
  through `\input`/`\include`/`\subfile`; open tabs supply their unsaved text, other files
  are read from disk and cached by modification time.
- Hover previews (`Editor::preview`): `mathAt()` finds the formula around the pointer,
  `imageAt()` the figure. Formulas compile with `pdflatex` in a temporary directory and
  `pdftocairo` turns the page into a PNG; both run as `QProcess`es, results are cached
  per editor by the generated source, and `previewReady` shows the tooltip when done.
- Folding hides `QTextBlock`s (`setVisible(false)` + `markContentsDirty`), so it touches
  neither the text nor the undo history. A line is folded when the block after it is
  hidden; `foldEnd()` finds the range (matching `\end`, or the next heading of the same
  level). The gutter only runs a cheap per-line test (`looksFoldable`).
- Theme: colours are written for the dark theme; `themeColor()` / `themed()` map them to
  light ones when `setLightTheme(true)` was called at start-up.
- Inverse search (`Editor::goToWord`): SyncTeX puts every word of a beamer frame on
  `\end{frame}`, so the PDF view sends the clicked word with its neighbours and the
  editor finds it inside the enclosing frame.

## PDF view (`src/pdfview.*`)

- Presentation mode (`setPresentation`) lays out one page per screen height, fitted and
  centred; `Window::present()` opens it in a second full-screen `PdfView` with its own
  renderer, so the main view keeps its position and cache.

- `PdfRenderer` lives on its own `QThread` and owns the Poppler document, loaded from an
  in-memory snapshot so `latexmk` can rewrite the file during a render. The UI talks to
  it only through queued signals: load, render a tile, inspect a page's text and links.
- Poppler decodes embedded images on every render call, so a page is rendered whole
  when it fits 8 megapixels, otherwise in 1024 px tiles.
- Every load increments a generation number; results whose generation or resolution no
  longer match are dropped. Tiles go into a cost-bounded `QCache` (32–128 MiB).
- Zoom and resize change the resolution only 150 ms after the last step; meanwhile
  existing tiles are drawn scaled, so the page never flashes white.
- Five seconds after the last scroll, zoom or resize, tiles more than two pages from the
  view and tiles of other resolutions are dropped (`trimCache`).
- Text boxes and links for the 8 most recently used pages are cached for selection,
  copy, link following and cursor shapes.

## Window (`src/window.*`)

- `BuildRunner` runs `latexmk` in its own session (`setsid`). Cancelling sends SIGTERM to
  the process group, then SIGKILL after 1.5 s, but only if `/proc/<pid>/stat` shows the
  same start time — a reused PID is never targeted. `% !TeX root` is followed with
  cycle detection; output goes to `.folitex-build/` next to the root file.
  Builds pass `-usepretex` with a PDF compression level of 1 (per engine): images that
  pdfTeX cannot copy as-is are otherwise re-deflated at level 9 on every run.
- Sources are decoded strictly (UTF-8 only, no U+2028/U+2029), saved with `QSaveFile`,
  and keep their BOM and line-ending style.
- A `QFileSystemWatcher` follows open sources. External changes are coalesced (200 ms)
  and applied as a minimal middle replacement, which keeps the cursor and undo history;
  tabs with unsaved edits are left alone. The watch is re-added after each change
  because atomic writers replace the file's inode.
- SyncTeX runs the `synctex` command-line tool asynchronously in both directions.
- After a build, `BuildRunner::parseLog` reads the `.log` (run with `max_print_line=10000`, so
  lines are not folded). Errors carry `file:line:`; warnings are assigned to the project file
  the log has open, tracked through its `(` / `)` file markers. Editors keep problem lines as
  `QTextCursor`s, so squiggles follow edits.
- The side bar (Files, Outline), activity bar, menus and the PDF toolbar are plain
  widgets; colours and metrics come from the application style sheet in `main.cpp`.

## Settings

`QSettings` under organisation `Folitex`, application `FoliTeX` (file
`~/.config/Folitex/FoliTeX.conf`) stores the workspace folder
and word wrap. They are read and written only in `main.cpp`, so tests never touch a
user's configuration. Settings from the former name TeXLite are copied once.
