# Changelog

## 0.6.0 — 2026-11-07

### Changed
- The app is now called **FoliTeX** (window title, launcher, docs). The command stays
  `folitex`; settings move once from `~/.config/Folitex/Folitex.conf` to `FoliTeX.conf`.
- Builds are up to 2.5× faster on documents with images: FoliTeX asks for light PDF
  compression (`-usepretex`, level 1 instead of 9). A report with ten RGBA screenshots:
  cold build 5.9 → 2.3 s, rebuild after an edit 3.0 → 1.2 s; the PDF is ~12 % larger
  (`research/build-speed-2026-11-07/`).

### Added
- Shift+Enter in a list starts the next `\item` (or `\task`) at the same indentation,
  also from a continuation line; elsewhere it is a plain newline instead of a U+2028 break.
- Ctrl+' puts LaTeX quotes `…' around the selection or cursor; pressed twice it gives ``…''.

### Fixed
- Pressing F5 again without changes cleared the Problems panel: latexmk does nothing and
  keeps its log, which was ignored as "old". The latest log is now always read.
- An error raised while a package loads (e.g. a bad `geometry` option) pointed at the system
  `.sty` and F8 opened it; it now lands on the project file that loaded the package.
- After a failed build the PDF view kept the previous PDF although LaTeX had written a new
  one; it now shows it.
- Shift+Enter counted `\begin`/`\end` inside comments, so `% \end{itemize}` stopped it.
- Stopping a build reported "Build failed" and opened the panel; it now says "Build stopped".
- Squiggles from the last build no longer appear on a file changed on disk since then.
- With word wrap on, Ctrl+Z could get stuck after Tab or Enter: it undid an invisible
  indentation-format step, which was immediately re-added and wiped the redo history.
  Undo and redo now skip those steps together with the text change they belong to.

## 0.5.0 — 2026-11-07

### Added
- Problems panel (next to Output, under the editor): errors from `-file-line-error`,
  LaTeX/package warnings and over/underfull boxes, grouped by file. Clicking one or F8 /
  Shift+F8 opens the file at the line; editors show squiggles with the message on hover;
  the status bar counts errors and warnings. Logs are no longer folded at 79 characters.
  The panel looks like VS Code's: pill tabs, a filter box, Collapse All, Maximize and Close.
- View → Menu Bar hides the File / Edit / … menu bar (remembered between sessions); a lone
  Alt then shows it until an item is used or Alt is pressed again, like VS Code's
  `menuBarVisibility: toggle`.

### Changed
- Five seconds after scrolling or zooming stops, the PDF view drops cached pages more than
  two pages away and tiles of old zoom levels. Scrolling a 15-page image-heavy PDF and
  pausing: cache 20.1 → 5.4 MiB, RSS 84.6 → 69.8 MiB; the visible page is not re-rendered.
- Editor tabs are pills, like the panel's: the active tab gets a rounded light background
  instead of an underline; × and the file icon sit closer together.
- The panel header and Problems list share the editor background.

### Fixed
- The first tab showed no × until hovered.

## 0.4.0 — 2026-11-07

### Added
- Package-aware completion: curated commands and environments for about 45 common
  packages (hyperref, siunitx, tikz, pgfplots, listings, mathtools, beamer, …), offered only
  when an open document loads them; other packages contribute the `\newcommand`-style
  definitions of their `.sty` files.
- File-type icons in the style of VS Code's Seti theme, in the explorer and on editor
  tabs; chevrons instead of triangles; a bold root folder row that collapses the tree.
- Tab indents and Shift+Tab outdents every selected line (one undo step each); Shift+Tab
  also outdents the current line. Tab without a selection pads to the next 4-column stop.
- Word wrap keeps the indentation: wrapped lines continue under the first non-blank
  character of their line, as with VS Code's `wrappingIndent: same`.
- GitHub Actions: build and tests on Debian 13 and Arch Linux for every push and pull
  request; pushing a `vX.Y.Z` tag publishes a release with its changelog section.

### Changed
- Side bar, editor, build log and PDF are separate rounded panels on a darker
  background, as in VS Code 1.145. The build log (Ctrl+J) opens under the editor only.
- Muted dark colour scheme (Dark Modern with custom workbench colours): darker side
  bar, flat tabs with an underline for the active one, thinner scroll bars.
- Editor tabs behave like VS Code with `tabActionLocation: left`: × left of the icon, only on
  the active or hovered tab, ● for unsaved changes (× on hover); the active tab has a light
  background and underline; faint separators. Crowded tabs keep their names and scroll sideways with the wheel or
  touchpad, without arrow buttons.
- Tabs with the same file name show their distinguishing folder, dimmed, after the name.

### Fixed
- Completion stays fast in large documents: sources over 5,000 lines are rescanned when
  typing pauses instead of on every key (50,000 lines: ~10 ms → under 1 ms per key).
- The first `\usepackage{` no longer stalls: TeX Live's package list loads in the background
  at start-up (+2.4 MiB RSS), and opening the full list takes 2 ms instead of ~50 ms.
- The compiler selector and build button sat on a black block; the tab area now paints
  its own background.
- Collapsing the root folder row moved it to the middle of the side bar; it stays put.
- Scroll bar colours were read as `#AARRGGBB` and looked olive; they use `rgba()` now.
- The editor find widget overflowed a half-width editor with wider fonts; it now shrinks
  to fit, up to 480 px.

## 0.3.0 — 2026-11-07

### Added
- Search in the PDF: Ctrl+F while the PDF has focus opens a find bar above the pages
  (Case sensitive, Whole words, Enter / Shift+Enter, "3 of 12", all matches highlighted).
- VS Code-style find widget over the editor (Ctrl+F, Ctrl+H): match case, whole word,
  match counter, Replace and Replace All (one undo step); Escape closes it.
- TeXstudio's default insert shortcuts: `\emph` Ctrl+Shift+E, `\texttt` Ctrl+Shift+T,
  `\textsc` Ctrl+Shift+C, `\textsf` Ctrl+Shift+A, `\\` Ctrl+Enter, `equation` Ctrl+Shift+N,
  `_{}` Ctrl+Shift+D, `^{}` Ctrl+Shift+U, `\frac` Alt+Shift+F, `\dfrac` Ctrl+Shift+F,
  `\sqrt` Ctrl+Shift+Q, `\left` / `\right` Ctrl+Shift+L / R; plus `\underline` Ctrl+U.

### Changed
- Open folder moves to Ctrl+K Ctrl+O and Ctrl+Enter now inserts `\\`, as in TeXstudio;
  Source → PDF stays on F7.
- Touchpad scrolling keeps gliding after a flick (kinetic scrolling) on Wayland and
  other systems without built-in inertia, in both the editor and the PDF view.
### Fixed
- After zooming or scrolling an image-heavy PDF, about 20 MiB of freed memory was not
  returned to the system (glibc mmap threshold); now fixed (`research/ram-2026-11-06/`).
- On Wayland, mouse wheel notches scrolled only 15 px: Qt sends a small pixel delta
  with each notch, which was mistaken for touchpad input. Notches now scroll 72 px in
  the editor and 120 px in the PDF view.

### Changed (scrolling)
- Touchpad movement in the PDF view is doubled.
- The PDF text test is skipped on systems without any installed font.
- README: verified Linux install steps, supported distributions and troubleshooting.

## 0.2.0 — 2026-11-06

First public release, renamed from TeXLite to Folitex.

### Added
- Context-aware completion: ~300 commands with snippet placeholders, environments with
  automatic `\end`, `\ref` labels, `\cite` keys from nearby `.bib` files, user
  `\newcommand`s, package and class names from TeX Live.
- TeXstudio-style shortcuts (F5/F6 build, F7 view, Ctrl+B/I, Ctrl+E, Alt+Return …).
- Editor aids: line numbers, current-line highlight, bracket matching, auto-closing
  brackets, auto-indent, word wrap (Alt+Z).
- Outline panel and workspace folder with a file tree.
- Silent reload of files changed by other programs (including AI agents) when the tab
  has no unsaved edits; the change can be undone.
- PDF interaction: text selection and copy, clickable links, Ctrl+click inverse search,
  right-click menu, zoom shortcuts and a PDF toolbar.
- Desktop entry and icon; `cmake --install` puts the app in the launcher.

### Changed
- VS Code "Dark Modern"-inspired interface: activity bar, menu bar, status bar, run button.
- PDF rendering uses whole pages or 1024 px tiles instead of 512 px tiles, and zoom
  re-renders once after the gesture ends. On an image-heavy page: 5.0 s → 0.69 s;
  six zoom steps: 11.0 s → 0.83 s (`research/pdf-render-2026-11-06/`).
- User interface and documentation are in English.
- Build output moved from `.texlite-build/` to `.folitex-build/`; settings move from
  `~/.config/TeXLite/` to `~/.config/Folitex/` automatically on first start.

## 0.1.0 — prototype (as TeXLite)

Native editor and tiled PDF viewer built after analysing TeXstudio 4.9.9: tabs,
`latexmk` builds with process-group cancellation, two-way SyncTeX, atomic UTF-8 saving,
DPI-aware background rendering with a bounded cache, pixel-smooth scrolling.
Measurements are in `docs/BENCHMARK.md`.
