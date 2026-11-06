# Changelog

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
