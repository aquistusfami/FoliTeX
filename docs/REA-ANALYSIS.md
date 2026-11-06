# TeXstudio Analysis and Implementation Decisions

## Scope and outcome

The goal is a native LaTeX editor with fewer features, a smaller footprint, DPI-sharp
PDF display and pixel-precise scrolling. `/usr/bin/texstudio` was neither replaced nor
modified, and no user configuration or documents were changed.

The implemented prototype uses **C++20 + Qt 6 Widgets + Poppler**. This choice was
validated on the current machine; it does not prove C++ is faster than Rust. The
prototype does not rename TeXstudio source; its features are written from scratch
with a narrower scope.

## Actual REA evidence

REA CLI 6.5.0 ran two `inspect-with-objdump` operations on `/usr/bin/texstudio`.
This is Evidence returned by REA, preserved in `evidence/`:

- [ELF file headers](evidence/rea-file-headers.json):
  `ev_2723c0198010ed68121c6f1353049f7f3537ab22cd88f7fe611b2401c26b2797`.
- [ELF section headers](evidence/rea-sections.json):
  `ev_265d8d2cce337e60d295a396f91c84a1345c2f6f1e2eb4bab9556a063d9e66a7`.

Both record an unchanged before/after SHA-256:
`605eb300dc23f2df2a588de08dbd19d5acd29eac6e31cb9f90ba4c0361ed9b3e`.
Raw GNU objdump identifies an x86-64 ELF. REA leaves some normalized fields and the
provider version as unknown; they are not promoted to verified facts.

The `.text` section is `0x63594e` bytes and `.rodata` is `0xfa3d18`.
These describe the byte layout of the file, not allocated RAM, and do not identify
which resources or features occupy the heap.

REA doctor found the CLI/Node working but no MCP registration for the Codex session;
Ghidra, Hopper and pwntools were not configured either. Therefore there is no
pseudocode, whole-program call graph or native UI trace collected by REA.
No engine was installed, no MCP registration changed, and no disassembler session
was left open.

## Source cross-reference and additional observations

The following items are **source or shell/subprocess evidence**, not REA Evidence:

- The installed package is `texstudio 4.9.9-1`; the binary is about 23.41 MiB, the
  package about 97.59 MiB.
- Direct libraries include Qt Widgets/Gui/Core, QML, Network, Concurrent, XML,
  PrintSupport, Core5Compat, Hunspell, QuaZip and Poppler. On-disk library sizes are
  not used to infer resident RAM.
- [Release 4.9.9](https://github.com/texstudio-org/texstudio/releases/tag/4.9.9)
  points to commit `d285921900394dc8aa151a12f5a12022bddbb351`.
- [The 4.9.9 config](https://github.com/texstudio-org/texstudio/blob/4.9.9/src/configmanager.cpp)
  defaults to a 512 MB PDF cache, hybrid loading, up to eight render queues and
  smooth scrolling enabled. 512 MB is a **configured ceiling**, not RAM always in use.
- Source at the same tag selects three initial completion dictionaries and has
  settings for grammar, dictionaries, AI chat, collaboration, macros and Git/SVN.
  The cost of each subsystem was not profiled, and the Arch build was not verified
  to match upstream byte for byte.
- The current master renderer already has async rendering and resolution-based
  caching; master differs from the installed release, so it is not used as evidence
  of the exact 4.9.9 binary behavior. The new prototype must not advertise these
  techniques as missing from TeXstudio.

## Footprint and measurement-supported optimization directions

Offscreen processes with private config/XDG directories under `/tmp` were measured
three times at the 5 s mark. Median TeXstudio PSS: 85.2 MiB empty; 96.4 MiB with
103 lines; 144.6 MiB with 5,003 lines; 108.3 MiB in PDF-only mode with a generated
12-page PDF.

The 50,003-line case was still processing at 5 s. A longer trace stabilized around
8 s, reaching about 645.8 MiB PSS at 20 s. Most of it is heap/anonymous mappings;
this does not identify the specific parser or data structure responsible.

Supported directions: reduce per-document state, reduce automatic work and bound the
raster working set. There is no basis to conclude that switching languages alone
solves that cost. Final prototype measurements and comparison conditions are in
`BENCHMARK.md`.

## Why this platform

| Consideration | Decision |
|---|---|
| Rendering and fonts | Poppler is available; math/fonts render through a mature library |
| DPI | Qt 6 and tile buffers follow the device pixel ratio; no upscaled low-resolution PDF bitmap |
| Scrolling | Direct pixel deltas for touchpads; easing for mouse wheels; no animation added to touchpad inertia |
| RAM | Budgeted tile cache; one worker; no extra copy of PDF bytes kept after Poppler copies them |
| UI | Qt Widgets; no QtQml/WebEngine in the new code |
| Source | Only highlighting and small completion; no full semantic/background services |
| Rust | Rust/GTK4/GtkSourceView or Rust with a native renderer is possible, but needs its own measured prototype |

Rust has no GC and suits the core, but C++ with RAII needs no GC either. On this
machine the Rust toolchain/GtkSourceView5 were not installed, while C++/Qt/Poppler
built immediately. This is an implementation convenience, not evidence that Rust is
slower or uses more RAM.

Upstream API references: [Qt High DPI](https://doc.qt.io/qt-6/highdpi.html),
[QWheelEvent](https://doc.qt.io/qt-6/qwheelevent.html),
[Poppler Qt6](https://poppler.freedesktop.org/api/qt6/).

## Completion checklist and unverified items

- Binary identified; REA digests/Evidence preserved.
- Subsystems cross-referenced against versioned source; master-only items are marked.
- Process footprint measured; shell metrics are not labeled REA Evidence.
- Separate prototype implemented, with I/O, compile, SyncTeX and scroll/cache tests.
- No decompilation/function profile yet; heap cost not attributed per feature.
- No feature-equivalent Rust build compared; not proven to be the lightest possible platform.
- First paint, input latency, FPS, GPU and native on-screen interaction not measured.
- No feature parity with TeXstudio; large source documents still need optimization.
