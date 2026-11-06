from pathlib import Path
import json,datetime,hashlib
root=Path('/tmp/texlite-final-benchmark')
final=json.loads((root/'summary.json').read_text())
old=json.loads(Path('/tmp/texstudio-baseline/summary.json').read_text())
previous=json.loads(Path('/tmp/texlite-benchmark/summary.json').read_text())
meta=json.loads((root/'artifact-inventory.json').read_text())
comparison={}
for key,row in final.items():
    comparison[key]={'texstudio_4_9_9':old[key],'texlite_v0_1_measured':previous[key],'texlite_final':row,'rss_reduction_vs_texstudio_percent':100*(1-row['rss_mib_median_at_5s']/old[key]['rss_mib_median_at_5s']),'pss_reduction_vs_texstudio_percent':100*(1-row['pss_mib_median_at_5s']/old[key]['pss_mib_median_at_5s'])}
(root/'comparison.json').write_text(json.dumps(comparison,indent=2))
labels={'empty':'Empty window','source_100':'103 lines / 7,436 bytes','source_5000':'5,003 lines / 387,836 bytes','source_50000':'50,003 lines / 3,977,836 bytes','pdf_12pages':'Generated 12-page PDF, PDF-only mode'}
lines=[
'# TeXLite final prototype reconciliation',
'',
'Ordinary subprocess and ELF observations. These are **not REA Evidence records**. This final run preserves the earlier v0.1 benchmark unchanged and binds every trial to the patched binary digest below.',
'',
'## Frozen final snapshot',
'',
'- Original executable: `/tmp/texlite-native/build/texlite`.',
'- Retained measured copy: `/tmp/texlite-final-benchmark/texlite-final-measured`.',
'- SHA-256: `'+meta['binary_sha256']+'`.',
'- Binary size: **'+str(meta['binary_bytes'])+' bytes**, unstripped, dynamically linked x86-64 ELF.',
'- Release build: C++20, `-O3 -DNDEBUG`.',
'- Direct dependencies: '+str(meta['direct_needed_count'])+'; resolved dependency files: '+str(meta['resolved_dependency_count'])+'. Their aggregate disk size is '+str(meta['resolved_dependency_disk_bytes_total'])+' bytes; this is not RAM or a complete installed application size.',
'- Current app source snapshot: '+str(meta['source_lines_total'])+' lines / '+str(meta['source_bytes_total'])+' bytes across seven app source/header files, with per-file digests and retained copies.',
'- Parent reports the final tests passed at scales 1 and 1.25; this worker measured resource usage, rather than independently rerunning the functional test suite.',
'',
'## Measurement method',
'',
'Same corpus, checkpoints and lifecycle as both prior measurements: three owned subprocesses per scenario, each sampled at 0.10, 0.25, 0.50, 1, 2, 4 and 5 seconds. Root process RSS/PSS/private memory, VmHWM, threads and CPU ticks were recorded. Every trial checked the frozen binary hash before launch and after termination. No user process was killed.',
'',
'All settings/cache/data locations and output were isolated under `/tmp/texlite-final-benchmark`. The prototype has no session persistence and was launched without `--config` or `--quit-after`. Source paths are positional; PDF mode uses `--pdf-only /tmp/texstudio-baseline/fixture.pdf`.',
'',
'```text',
'QT_QPA_PLATFORM=offscreen /tmp/texlite-native/build/texlite [fixture.tex]',
'QT_QPA_PLATFORM=offscreen /tmp/texlite-native/build/texlite --pdf-only /tmp/texstudio-baseline/fixture.pdf',
'```',
'',
'No Qt scaling environment override was inherited. The parent coordinated a pause of its own Qt screenshot/test processes during this final run. Unrelated host workloads, filesystem caches and sharing with external processes remain uncontrolled.',
'',
'## Final medians at five seconds',
'',
'| Scenario | Final RSS MiB | Final PSS MiB | Private MiB | CPU seconds total | CPU seconds during final wall second | Threads |',
'|---|---:|---:|---:|---:|---:|---:|',
]
for key,row in final.items():
    lines.append(f"| {labels[key]} | {row['rss_mib_median_at_5s']:.1f} | {row['pss_mib_median_at_5s']:.1f} | {row['private_mib_median_at_5s']:.1f} | {row['cpu_seconds_median_at_5s']:.2f} | {row['cpu_seconds_last_second_median']:.2f} | {row['threads_median_at_5s']} |")
lines += [
'',
'**CPU seconds are not startup wall latency.** CPU quiescence during the final sampled interval is not proof of input responsiveness, interactive readiness, first paint or scrolling FPS.',
'',
'## Comparison to earlier snapshots',
'',
'| Scenario | TeXstudio RSS / PSS MiB | v0.1 RSS / PSS MiB | Final RSS / PSS MiB |',
'|---|---:|---:|---:|',
]
for key,row in final.items():
    a,b=old[key],previous[key]
    lines.append(f"| {labels[key]} | {a['rss_mib_median_at_5s']:.1f} / {a['pss_mib_median_at_5s']:.1f} | {b['rss_mib_median_at_5s']:.1f} / {b['pss_mib_median_at_5s']:.1f} | {row['rss_mib_median_at_5s']:.1f} / {row['pss_mib_median_at_5s']:.1f} |")
lines += [
'',
'The 50,003-line TeXstudio value at five seconds was still transient; its separate one-run 20-second follow-up stabilized near 685.5 MiB RSS / 645.8 MiB PSS by eight seconds. Do not relabel the five-second baseline as settled, or its follow-up as a three-trial median.',
'',
'## Interpretation and limits',
'',
'- The final normal-case footprint remains close to v0.1. Changes that fix encoding/IO safety, failed-render handling, process cancellation, and huge-viewport caching are not fully exercised by these normal startup/resource scenarios.',
'- TeXLite is a much smaller prototype. TeXstudio completion/checking/project/session/PDF features are not matched, so resource differences are not a feature-equivalent benchmark or language-performance comparison.',
'- Both measured applications are C++/Qt; these results cannot establish a Rust-versus-C++ ranking.',
'- The prototype window initially uses 1280×820; TeXstudio viewport/zoom/geometry were not standardized. PDF workloads therefore are illustrative rather than an equal-pixel rendering benchmark.',
'- The renderer has an adaptive configured cache allowance (normally 32 MiB, up to 128 MiB for oversized viewports). This setting is not measured allocated RAM, does not include every in-flight/library/UI allocation, and is not a process-memory ceiling.',
'- The large source still consumes substantial process memory. A later lower-memory buffer/editor design should be assessed separately on the same corpus.',
'- The sandbox cannot access the real Wayland display, GPU, input stream, host application processes or mixed-DPI movement. Native scroll frame pacing and display quality remain manual/host validation work.',
'- The small executable relies on installed Qt, Poppler and transitive native libraries. Its file size is not a self-contained installation size.',
'',
'## Retained evidence files',
'',
'- `artifact-inventory.json`: frozen identity, source digests and dependency inventory.',
'- `measurements.json`: all 15 trials, exact command lines and timestamped checkpoints.',
'- `summary.json`: final medians.',
'- `comparison.json`: final, v0.1 and TeXstudio statistics.',
'- `measure.py`: exact measurement script; `make_report.py`: report generator.',
'- `runs/*/{stdout,stderr}.log`: per-trial application output.',
'- `source-snapshot/`: measured app source plus build/test/launch files.',
'- `texlite-final-measured`: retained measured executable.',
'',
'Earlier v0.1 files remain unchanged under `/tmp/texlite-benchmark`; TeXstudio baseline files remain under `/tmp/texstudio-baseline`.',
'',
'Generated: '+datetime.datetime.now().astimezone().isoformat(),
]
(root/'REPORT.md').write_text('\n'.join(lines)+'\n')
assert hashlib.sha256((root/'texlite-final-measured').read_bytes()).hexdigest()==meta['binary_sha256']
print(root/'REPORT.md')
for key,row in final.items():
    print(key,round(row['rss_mib_median_at_5s'],2),round(row['pss_mib_median_at_5s'],2),row['cpu_seconds_median_at_5s'],row['cpu_seconds_last_second_median'])
