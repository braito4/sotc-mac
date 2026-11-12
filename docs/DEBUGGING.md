# Contributor handoff

Checkpoint: 2026-11-12. Native ARM64 and Metal reach gameplay on an Apple M1 with
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

## Fragmented-output propagation: current checkpoint

The initializer and both append-producer variants now have native validation on the
owner's M1. In two consecutive sessions the owner passed the recurring failure point
and closed the window normally. Eighteen captured producer dispatches preserve the
whole output between successive appends, retain counter continuity and yield valid
active indices. This does not prove all routes, textures or crashes are fixed.
Colored pixel noise remains visible on some surfaces. A screenshot distinguishes
that symptom from completely absent textures but cannot establish its cause.

`gpu_fragment_writeback_metal.h` is an opt-in ordered propagation scope. Each exact
private selection refreshes a fragmented output after earlier GPU work completes,
executes with a fresh temporary, copies completed data to the original staging
range and publishes it to physical backing. This retains previous append results
even when following destinations were prepared with stale data. The original atomic
counter binding is preserved; an observed fallback counter is rejected rather than
silently replaced. Producer-input and downstream-consumer guards remain enabled.

The latest paired-output trial supports two output slots per entry, for the next
attribute-update stage. It plans both before rebinding, and the host batch-v2 API
validates the entire group before any copy under the actual memory-table read lock.
The original even generation must match every output. Missing fragments, physical
bounds, overlapping physical aliases, ambiguous native bindings or mapping changes
reject the group. There is no interval where an allocator can remap one destination
between validation and publication. This is atomic against mapping changes, not
against unsynchronized guest CPU stores. GPU commands are complete before host
publication; the next queued dispatch is encoded afterwards. Additional synchronous
waits make this a correctness trial, not a performance optimization.

The shared ABI is in `fragment_writeback_api.h`; the host implementation lives in
`fragment_writeback_runtime.h`, included by the real memory backend. The single-output
v1 export remains for older renderer controls. The new renderer requires batch v2.
Each output is capped at 8 MiB, each group at two outputs, and mapping plans at
256 physical segments. Selection allows up to five distinct entries. A synthetic
example is `fixture_init,3,64;fixture_append,5,64,4;fixture_pair,10+13,64` through
`SOTC_FRAGMENT_WRITEBACK`; real entries/layouts must be reviewed privately.

Validation: sanitized CPU protocol tests exercise nine appends into preprepared
stale buffers, live counters, paired and mixed direct/fragmented outputs, restoration,
preserved tails, second-output failure, mapping ABA and physical-alias rejection.
Both outputs remain unpublished when the second fails validation. Full host contracts
run in JIT and interpreter modes; native library compilation/load are separate
checks. Paired-output gameplay and any effect on the colored noise remain pending.

Local incremental builders: `build_fragment_writeback_trial.py`,
`build_producer_writeback_trial.py` and `build_group_writeback_trial.py`. They depend
on verified predecessor manifests and control objects in the ignored work directory;
they are not standalone release builders. The cumulative overlays are public research
sources. Neither the default renderer manifest nor dependency pins are promoted by
these experiments. Preserve the known working producer trial for rollback.

To validate the original synthetic propagation fixtures on an ARM64 Mac:

```sh
mkdir -p work
python3 tools/check_fragment_writeback_trial.py
bash packaging/test-arm64.sh --contracts-test
```

The optional `--native` fixture attempts real Metal work and reports unavailability
when no device is exposed. CPU protocol success alone is not native GPU validation.

## Comparing two producer variants

The optional producer observer accepts one exact entry or two entries separated
by `|` in the first field of `SOTC_PRODUCER_BUNDLE`. The remaining fields are the
shared output slot, counter slot and minimum output byte size. Supply reviewed
private profiles; entry names and game layouts are not provided by this repository.
For example, the synthetic form is `fixture_a|fixture_b,5,4,64`.

One entry retains the 32-capture budget. Two entries reserve 16 captures each;
arming is independent, and an early entry cannot consume the other's allowance.
The saved metadata identifies the actual selected entry and retains the observed
binding descriptor association for every capture. Each variant still requires its
own verified offline interpretation, particularly for element stride and push data.
Unselected entries, duplicate entries, empty entries and more than two selections
are covered by the observer regression (`tools/check_producer_bundle_trial.py`).

`tools/build_dual_producer_trial.py` builds an isolated diagnostic library from
the verified region trial. This does not enable writeback, expand capture memory
limits, relax index validation or change the default renderer. The checks use
ordered CPU protocol doubles with sanitizers; a native game comparison is separate.
Equality of producer/consumer bytes does not establish allocation identity, while
a difference does not by itself identify the writer or the intervening copy.

## Protecting producer input reads

The optional `SOTC_PRODUCER_INPUT_GUARD` selects up to two exact reviewed entries.
Each semicolon-separated record contains the entry, parameter slot, parameter push
adjustment byte, first-index field, triangle-count field, index slot and adjustment
byte, normal slot and adjustment byte, normal stride and X threads per group.
Layouts must come from the owner's verified shader sources. Supported strides are
four and eight bytes; unsupported or ambiguous layouts fail explicitly.

The guard snapshots parameters, indices and indirect dimensions in GPU order before
the producer dispatch. It validates the active triangle range, complete normal
elements and 32-bit shader address arithmetic. On success, it binds the immutable
validated input and retains it through the command; original bindings are restored
after encoding. Snapshot truncation or invalid indices stop before the producer
with status23 and private `producer-input` evidence, binding associations and buffer
history. The guard has no capture-count cutoff, so exhaustion of the separate
producer observer budget does not disable containment.

This can stop earlier than the consumer guard. It does not repair bad source data,
pin guest allocation lifetimes or propagate fragmented writable output. Adding
writeback requires destination lifetime and ordering guarantees beyond a sampled
mapping generation. Both strict guards remain active in the private comparison.
`tools/check_producer_input_trial.py` checks the actual guard with sanitized ordered
CPU protocol doubles; `tools/build_producer_input_trial.py` builds its isolated
region renderer. Native gameplay and performance validation remain separate.

## Earlier comparison: direct-output control

Two distinct problems appeared in earlier failing captures: producer input already
contained indices outside its normal-buffer extent, and a fragmented writable
fallback's new output did not reach the later consumer snapshot. The observed
consumer retained the producer's pre-dispatch contents. Fixing output propagation
alone cannot repair the already-invalid input.

A November 6 instrumented run closed normally after about 446 seconds. Nine complete
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

A subsequent repeat stopped on the strict guard after about 292 seconds, with
6,540 invalid active indices. Its selected producer captures belong to a different
output/counter range from the failing consumer, so the earlier stale-output
conclusion cannot be applied to this run. The consumer's first 64 sampled bytes
were zero at the mapping observation and invalid at consumption. This narrows the
observation interval but does not identify a writer or establish alias ownership.

## Optional small compute transfers

The separate `tools/build_small_copy_trial.py` renderer includes the queued
association trial below and an adaptation of bloodborne-runtime `a03929b5`.
`SOTC_SMALL_COPIES=1` allows bounded small copies, fills and updates inside an
already open serial compute encoder. Default is off. The original queued trial
and standard build remain available as controls.

Read [the adaptation and its limits](REFERENCE_REVIEW_20261109.md#optional-small-transfer-adaptation-prepared).
Cross-object copies with unknown backing retain blits. No buffer history or strict
guard is removed. `SOTC_SMALL_COPIES` log lines count encoded operations and
fallbacks; `compute_copy_buffer`, `compute_fill_buffer`, `compute_update_buffer`
trace events identify the actual compute encoder. These are encoding observations,
not completion or FPS evidence. Full diagnostic runs can change timing.

CPU-only validation: `python3 tools/check_small_copy_trial.py` after preparing the
isolated trial sources. Portable planner tests also run in the public tools suite.
The protocol doubles and translated kernel model do not establish native Metal
shader compilation, synchronization or gameplay correctness.

## Queued association trial prepared

`tools/build_binding_route_trial.py` builds an isolated renderer with an opt-in
`SOTC_BINDING_ROUTE` path. The private launcher supplies up to eight reviewed shader
request identifiers through `SOTC_BINDING_ROUTE_SHADERS`; none are hardcoded in
the public adapter. At most 128 selected large-binding dispatch packets are retained,
with up to 32 binding records each. Budget exhaustion is reported.

The preparation thread copies raw descriptor observations, validates the descriptor
write submitted for recording, and carries an immutable packet in the queued dispatch.
The encoder compares native object, epoch and extent before allowing CPU/backing
sampling. Unobserved special paths, changed descriptors, unavailable identities and
reuse are explicit rejections; there is no newest-record fallback across threads.

`route-*-before.json` records pre-guard bindings. `route-*-encoded.json` records the
dispatch bindings, including any immutable input substitutions made by the guard.
`route-*-completed.json` records completion of that actual Metal command buffer.
The existing guard can commit a chunk between the first two phases, so their
command-buffer pointers may differ legitimately. Completion must match the encoded
phase. No new GPU waits, guest writes or permissions changes are added by this route
observer; metadata and callback/file overhead can still change timing.

`tools/analyze_binding_route.py` reports association reasons and prior overlapping
writable bindings as **candidates**, not proven byte writers. Native epochs do not
identify guest allocation lifetimes. Missing completion files remain unknown.
The native build, cross-thread/identity sanitizer fixtures, protocol-double callback
tests and 50 tooling tests pass. Owner gameplay coverage is still pending.

Reviewed performance backports remain separate in the
[November 9 reference review](REFERENCE_REVIEW_20261109.md).

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
