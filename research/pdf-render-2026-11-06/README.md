# PDF render benchmark, 2026-11-06

Ordinary subprocess measurements (not REA Evidence), offscreen Qt, QT_SCALE_FACTOR=1.25,
Arch Linux, Poppler 26.10.0-2, Qt 6.12.0-3, ThinkPad P14s Gen5 AMD.

Input: a 15-page user report PDF with screenshots (not included; private document),
SHA-256 5c8993ad5dac726ade84b13fcf50e3f35c070ef504609294468420bc33f99e73.

- `poppler-calls/`: one page rendered at 180 dpi as 1 call, 15 tiles of 512 px, or 3 strips.
  Poppler decodes embedded images on every call, so call count dominates image-heavy pages.
- `viewer/`: PdfView harness, `old` = research/texlite-final-benchmark/source-snapshot
  (512 px tiles, immediate DPI change), `new` = whole-page/1024 px tiles + 150 ms zoom debounce
  with scaled fallback. Times include a 200 ms idle wait subtracted from the total.

Binary measured: build/texlite SHA-256 9b04e240068f2e57a144c8a65cf5a762de78fdc21fb0af469a46b50b8ba8d25d.
Limitations: single run each, offscreen (no GPU/compositor), one document; RSS rises
at high zoom because the cache must hold up to two whole pages (≤ 32 MiB each).
