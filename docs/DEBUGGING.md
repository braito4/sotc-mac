# Contributor handoff

Checkpoint: 2026-11-06. Native ARM64 and Metal reach gameplay on an Apple M1 with
16 GB. Low frame rates, intermittent failures and route-dependent visual artifacts
remain. There is no confirmed general crash fix. No game files, generated game
code, shader dumps, private profiles, raw captures or saves are distributed here.

## Start here

1. Follow [BUILD.md](BUILD.md) for a standard source build using your own supported
   game input. `tools/dependencies.json` and `tools/host_sources.json` record pins;
   `tools/gpu_patches/manifest.json` identifies the default renderer overlays.
2. Preserve a control host, renderer and their build manifests. Keep CPU, renderer,
   shader-cache identity and optional diagnostic settings explicit in comparisons.
3. Read [REPOSITORY_WORKFLOW.md](REPOSITORY_WORKFLOW.md) for component ownership,
   upstream review and the staged CPU/recompiler separation. Trial patches are
   optional; their presence in Git does not enable them in the default build.

## What the latest comparison establishes

Two distinct problems appeared in earlier failing captures: producer input already
contained indices outside its normal-buffer extent, and a fragmented writable
fallback's new output did not reach the later consumer snapshot. The observed
consumer retained the producer's pre-dispatch contents. Fixing output propagation
alone cannot repair the already-invalid input.

The latest instrumented run closed normally after about 446 seconds. Nine complete
producer captures used direct imported pool bindings. Every captured input index
was within the bound normal-buffer extent; inputs contained varied data. Each
complete output-after matched the next output-before, and all appended indices
were valid. This supplies a useful successful-path comparison, not proof that
direct mapping alone fixes the intermittent failure. There was no full downstream
consumer capture in this run, so final consumption and rendering remain unproven.

Reported FPS windows had median 2.1, versus 2.4 in the preceding failing run; both
had a median of 2.6 in their last 20 windows. Duration, scene and instrumentation
coverage differ. These are observations, not a measured performance improvement.

The new descriptor/CPU-view audit **did not associate any of the first bundle's
bindings** in actual gameplay. Its synthetic fixtures pass, but gameplay coverage
does not. No CPU alias/backing equivalence can be inferred from missing reports.
Sparse page-content samples have their own retention and sampling budgets; absence
of an event or sample does not prove that no write occurred.

## Next investigation

1. Establish why the descriptor audit misses actual bindings. Instrument bounded
   association outcomes: hook coverage, thread, logical/unified slot, native span
   and allocation epoch. Do not relax identity checks to accept stale records.
2. Trace the operation that initializes the producer's input. Successful-path
   history retains a recent full-range writable binding intent from an earlier
   shader. Correlate it with dispatch and completion before calling it the writer.
   A binding intent, physical overlap or matching bytes is insufficient attribution.
3. Compare direct and fragmented mappings for the same logical resource and
   allocation generation. Follow initialization, reuse and GPU completion order.
4. Design writable-fallback propagation only after ownership is established:
   preserve read/write access, dirty ranges, aliases, generation checks and command
   completion. Never copy an entire stale temporary buffer over reused guest pages.

Keep strict index guards. Do not hide the failure by zeroing pages, clamping indices,
skipping work, or continuing after invalid coverage. Once correctness is established,
measure lower-overhead runs separately from the capture-heavy diagnostics.

## Source navigation

| Area | Sources | Scope / limitation |
| --- | --- | --- |
| CPU physical-page history | `runtime/src/buffer_page_history_{api,core,runtime}.h` | Bounded mapping/copy events and sparse contents; not an exhaustive write monitor |
| CPU import origin | `runtime/src/cpu_import_origin*.h` | Captures validated import-boundary context; missing origin stays unknown |
| GPU buffer history | `runtime/src/gpu_buffer_history.h` | Native identities, copies, write intents and bounded physical history |
| Fragmented mappings | `runtime/src/gpu_fragment_{provenance,gather}.h` | Provenance and optional ordered read-only gather; no general writable copyback |
| Producer snapshots | `runtime/src/gpu_producer_bundle_metal.h` | Completed before/after snapshots for privately selected dispatches |
| Guarded inputs | `runtime/src/gpu_scatter*_metal.h`, `gpu_scatter_cached_guard.h` | Validates captured inputs; invalid coverage stops instead of masking indices |
| Descriptor/CPU audit | `runtime/src/gpu_binding_audit.h`, `buffer_binding_audit_runtime.h` | Strict association; current gameplay associations missing |
| Offline analysis | `tools/analyze_page_content.py`, `analyze_binding_audit.py` | Explicit missing/unequal observations; private input and output |
| Optional builds | `tools/build_*_trial.py`, `tools/gpu_patches/*-trial.patch.json` | Isolated checked overlays, not default dependency updates |
| Low-load / RAM policy | `tools/graphics-low-load.sh`, `hardware-memory.sh` | Engine options and adaptive retention; RAM tiers need on-device validation |

## Reproducing checks

Public checks require no game data or existing native build. On macOS, install
Xcode Command Line Tools first for the synthetic C fixtures:

```sh
python3 tools/check_public_source.py
python3 -m compileall -q tools
python3 -m unittest discover -s tools/tests -p 'test_*.py'
for script in build.sh run.sh tools/build_host.sh tools/build_cpu.sh tools/runtime-settings.sh; do
    bash -n "$script"
done
```

The native diagnostic fixture scripts are separate from that portable suite.
For example, `check_binding_audit_trial.py` requires macOS/Metal framework headers,
the prepared pinned sources and an existing isolated host. It exercises real
observer code with synthetic CPU/Metal protocol doubles, not a retail game scene.
Read each script's prerequisites and paths before invoking it.

Native host/renderer builds, load/export checks, JIT and interpreter CPU contracts,
and diagnostic ASan/UBSan fixtures passed during local preparation. These are local
validation results; the public CI checks source boundaries and portable tooling.
A clean full native rebuild of this publication is not a CI guarantee.

### Incremental trial limitations

Trial wrappers reuse `build_texture_alias_trial.py`, verified baseline source files
and native objects under `work/`. That builder deliberately requires the exact
recorded control-library SHA-256. A new compiler/SDK/build can produce a different
binary and will be rejected even at the same source revision. Do not remove that
check merely to run a trial. Qualify a new baseline, review overlay source hashes,
rebuild and validate affected units, then record the new control identity as an
explicit reviewed change. Preserve the original control for comparisons.

Root `prueba_*.command` launchers mentioned in historical notes are private and are
not included. Shader entry selections, descriptor layouts and instruction profiles
must be independently derived and verified from your own local build. Without
those selections, targeted diagnostics may record nothing; that is not a pass.
The standard `build.sh` / `run.sh` route does not depend on those private launchers.

## Useful contributions

Prefer a small change with its synthetic regression, source pins, build provenance
and a clear statement of what was actually observed. Separate correctness,
performance and repository-layout changes. Start with bounded association coverage
and the initializer trace above before changing memory synchronization. See the
[reference strategies](MAC_PORT_STRATEGIES.md) for upstream context.

Share aggregate measurements and minimal original synthetic reproductions. Keep
game addresses, extracted code/shaders, logs containing game data and binary
captures private; do not attach them to issues, PRs or releases.
