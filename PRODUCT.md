# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

FoliTeX is a native Linux desktop application (C++20, Qt 6 Widgets, Poppler-Qt6), not a website.
`web` is recorded only because the schema has no desktop value; no iOS/Android design language applies,
and the web-only tools (`live`, `generate`, `detect`) do not apply to its Qt code.

## Users

LaTeX users in general: anyone who writes LaTeX documents (papers, theses, lecture notes, exam
papers, beamer decks) and wants a light native editor instead of TeXstudio, VS Code with LaTeX
Workshop, or a cloud editor. The developer is also a daily user, writing Vietnamese mathematics
documents (exam papers, course material) on Arch Linux.

## Product Purpose

A lean LaTeX editor with a built-in PDF viewer that covers the everyday loop: edit, build, preview,
jump between source and PDF, fix problems. It exists because the established editors are heavy
(TeXstudio's memory and UI weight, Electron for VS Code) or need the cloud (Overleaf). Success is an
editor that starts fast, stays responsive on long documents, renders the PDF sharply at fractional
display scales, and never puts the user's source at risk.

## Positioning

A native Qt editor built after analysing TeXstudio 4.9.9: the everyday LaTeX workflow at a fraction
of the start-up time and memory, with a tiled background PDF renderer that stays sharp at any
display scale, TeXstudio-compatible insert shortcuts, VS Code-style navigation (command palette,
quick open, back/forward), and nothing that requires an account or network.

## Operating Context

- Linux only. Primary environment: Arch Linux on Wayland (niri compositor), HiDPI at fractional
  scale 1.25; tests also run at scales 1, 1.25 and 2.
- Builds run `latexmk` (pdfLaTeX, LuaLaTeX or XeLaTeX) from the installed TeX Live; output goes to
  `.folitex-build/` next to the root file; `% !TeX root = …` selects the root of multi-file projects.
  SyncTeX links source and PDF in both directions.
- Sources are often edited at the same time by other programs, including other editors and AI coding
  agents; FoliTeX reloads them when the tab has no unsaved edits and never overwrites unsaved edits.
- Documents range from short notes to long papers and theses, often with non-ASCII text
  (Vietnamese).

## Capabilities and Constraints

- Editor: tabs, LaTeX highlighting, completion (commands, environments, labels, citations, package
  commands), folding, find/replace, formula and figure previews on hover, word wrap, templates,
  command palette, quick open, back/forward history, distraction-free mode, a formatting bar on
  mouse selections.
- Build and preview: asynchronous builds, Problems panel with jump-to-line and squiggles, two-way
  SyncTeX, continuous tiled PDF viewer with search, selection, links, zoom, dark pages and
  full-screen presentation.
- Workspace: explorer with file operations, outline, session restore, recent files.
- Technical constraints: rendering stays off the UI thread; image caches are bounded; stale render
  results are dropped (generation/DPI check); saves are atomic and preserve BOM, NBSP and line
  endings; invalid UTF-8 is refused; cancelling a build kills the compiler's whole process group and
  never an unrelated process.
- Terminology used in the UI: Build, Problems, Output, Explorer, Outline, Focus (distraction free),
  Command palette, Quick open, Source → PDF / Jump to Source, Fit width, Dark (PDF pages).
- UI strings and documentation are English.
- Undecided: no Windows or macOS support is planned; no spellcheck or full LaTeX parser yet.

## Brand Commitments

- Name: FoliTeX (formerly TeXLite; historical records under `research/`, `artifacts/` and
  `docs/BENCHMARK.md` keep the old name).
- Informed by studying TeXstudio and VS Code; contains no code from either and is not affiliated
  with them. Never imply otherwise.
- Voice: plain and factual; claims about speed or memory cite a measurement.

## Evidence on Hand

- `docs/BENCHMARK.md`, `research/texlite-final-benchmark/`: memory measurements (PSS) of the
  measured `bin/texlite` build against a TeXstudio baseline. They apply to that binary only; later UI
  changes are not re-measured.
- `docs/VALIDATION.md`, `docs/validation/`: test logs; `docs/REA-ANALYSIS.md`: the TeXstudio analysis.
- `docs/screenshot.png`: README screenshot (0.5 era UI, outdated).
- Interface reference: the approved interactive mock at https://claude.ai/artifact/W5CdsVe27dkxzyMvf929xg.
- Absent, and not to be invented: user counts, testimonials, adoption figures, Windows/macOS
  builds, and performance numbers for builds that were not measured.

## Product Principles

1. Speed and memory are features. A change must not make start-up, typing, scrolling or PDF
   rendering measurably worse; claims about performance come from measurements.
2. The user's source is sacred. Never lose, reformat or silently overwrite bytes the user did not
   change.
3. Keyboard first. Every action has a shortcut and is reachable from the command palette; the mouse
   is a convenience, not a requirement.
4. Local and offline. Depend only on the installed TeX distribution; no accounts, telemetry or
   network services.
5. Familiar over novel. Follow TeXstudio and VS Code conventions where LaTeX users already have
   muscle memory.
