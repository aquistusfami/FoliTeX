# Build speed: PDF compression level — 2026-11-07

Ordinary subprocess measurements (not REA Evidence), wall time on the author's machine
(16 threads), TeX Live 2026, latexmk 4.87. The document is a private 15-page report
(not included): pdfLaTeX, `T5` fonts, one 4975×5056 RGB PNG and ten 1704×1403 **RGBA**
screenshots. Commands as Folitex runs them (`-interaction=nonstopmode -file-line-error
-synctex=1 -outdir=.folitex-build`, `max_print_line=10000`).

## Where the time goes

| Run | Time |
|---|---:|
| one `pdflatex` pass | 2828 ms |
| same pass with `-draftmode` (no PDF written) | 432 ms |
| the 4975×5056 RGB PNG alone, in a one-page document | 191 ms |

RGB PNGs without alpha are copied into the PDF as they are. PNGs with an alpha channel are
decoded, split into image and soft mask and deflated again on every pass, at
`\pdfcompresslevel=9` by default.

## Compression level, one pass (best of two)

| `\pdfcompresslevel` | Time | PDF size |
|---|---:|---:|
| 9 (default) | 2831 ms | — |
| 6 | 1348 ms | 3.2 MB |
| 3 | 1066 ms | 3.5 MB |
| 1 | 1051 ms | 3.6 MB |
| 0 | 895 ms | 93 MB |

SyncTeX on or off made no measurable difference (2831 vs 2817 ms).

## Whole latexmk builds

| | Default | `-usepretex='\pdfcompresslevel=1'` |
|---|---:|---:|
| cold build | 5872 ms | 2293 ms |
| rebuild after a one-word edit | 2971 ms | 1199 ms |

Small check per engine (one RGBA screenshot, cold latexmk, exit 0, no errors in the log):
pdfLaTeX 546 → 347 ms, LuaLaTeX (`\pdfvariable compresslevel=1`) 841 → 645 ms,
XeLaTeX (`\special{dvipdfmx:config z 1}`) 761 → 523 ms; PDFs 13–28 % larger.

Adopted in `BuildRunner::start` as `-usepretex=…` per engine; a document that sets its own
compression level after that still wins.
