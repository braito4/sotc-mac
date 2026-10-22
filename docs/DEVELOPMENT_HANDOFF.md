# Development and debugging handoff

Snapshot: 2026-10-22. This is the starting point for the next developer. Read
[AGENTS.md](../AGENTS.md), this page and [Build instructions](guide/building.md)
before changing code. [Status](STATUS.md) and [Graphics bring-up](GRAPHICS_BRINGUP.md)
contain chronological investigations; their earlier “latest run” sections describe
older boundaries, not the present state.

## Continuation checkpoint

### Black output, repeated OOM and withdrawn buffer-bound experiment

The newest owner run starts at **08:36Z on 2026-10-22** and ends after
**160.610 seconds** on `MTLCommandBufferErrorDomain` code 8, `Insufficient Memory`.
Child and supervisor both exit 23. The owner reports no logos or visible scene,
while input still works. Presentation API success does not prove visible pixels;
pixel capture was off. Earlier visible logos/menu and defective 3D remain
historical evidence, not the state of this run. Scene stays **640x360**, DMA and
asynchronous submission off.

Native allocation at failure is **12,468,715,520 bytes** against the unchanged
12,713,115,648-byte recommendation. The last journal tracks 5,002,137,756 bytes of
Vulkan allocation, including 1,242,582,400 bytes of images. Dedicated upload
inventory is 2,977,955,840 bytes across six buffers; its snapshot is 1.154 seconds
old. These overlapping counters do not identify the resource that failed or
justify releasing retained resources. Final asynchronous counters include
185,119 missing-pipeline draws out of 470,597 requests and 194 failed pipeline
creations. There are 40,867 reduced decisions out of 41,175 scene decisions.

The local guarded read-only buffer-bound experiment in `9eca092` was intended to
avoid a multi-GiB fallback copy when a shader's proven access range is small.
**No applied buffer reduction is logged in this native run.** Its synthetic CPU
fixtures and build passed, but native matching and memory savings are unverified.
The experiment is withdrawn from the active renderer, including its persistent
metadata additions and cache-version changes. The private source matches the
previous checked overlay manifest again, with shader binary/meta versions
**23/9**. The working audio-quantum and staging best-fit corrections remain.
The local historical commit preserves the experiment for isolated research.

Zero reductions do not explain the black output or establish that the experiment
caused it. The previous ARM64 renderer is rebuilt incrementally using the existing
dependencies/generated shaders. The five actual-source renderer regression tools,
25 builder tests and full JIT/interpreter CPU contracts pass. A completely fresh
dependency build still needs the unavailable external glslang compiler. No
tool-driven GPU run validates this rollback.
Do not claim that the rollback restores the logos until the owner sees them.
Keep the same launch settings and change only the renderer revision for that
comparison. Do not start a game run automatically.

The large-copy investigation remains open: a raw descriptor is clamped to the
end of its guest mapping, and a request crossing the selected imported window
falls back to a full staging copy. Allocation rounding can turn an approximately
3 GiB request into a retained 3 GiB upload buffer. A synthetic guard-bound proof
does not demonstrate matching against the native final IR. Do not substitute an
arbitrary buffer cap, disable split imports, add overlapping aliases or weaken
resource/tick ownership checks. Later image growth also remains to investigate.

### Native audio improvement and exercised staging reuse

The earlier owner run starts at **07:23Z on 2026-10-22**, using the corrections in
`a49852f`. The owner reports that audio is much better. The native prerequisite
draw/readback passes on Apple M1; the game starts its main loop in 1.49 seconds,
detects a DualSense and runs for **137.642 seconds**, versus 59.026 seconds in the
preceding comparison. It still stops on actual Metal out-of-memory failure,
`MTLCommandBufferErrorDomain` code 8, with confirmed child/supervisor exit 23.
There is no duration deadline, GPU-watchdog or import stop; a targeted lookup
finds the process absent. Scene stays **640x360**, DMA/async off and cache 23.
The longer run is not a gameplay stability or performance result.

There are 52 complete five-second audio windows, 26 per active port, spanning
about 130 seconds per port. They confirm an actual **1024-frame, 48 kHz device**.
The input limit is 32,768 bytes before accepting the next 8,192-byte guest block,
allowing five blocks instead of the former two. Mean delivery is **47,684 input
frames/s per port**, versus 23,912 previously; median window rates are about
47,988–47,995 frames/s. This is native evidence for the queue correction and
agrees with the owner's audible improvement. Maximum submission gaps still reach
121.85 ms, output-lock waits peak at 54 microseconds and no queue/output failure
is recorded. Zero input-queue observations do not establish hardware underruns;
uninterrupted playback and audiovisual alignment remain unverified. See
[Audio diagnostics](AUDIO_DIAGNOSTICS.md).

The new staging path is exercised: four new dedicated buffers, nineteen reuses
and **four reuses that the old size restriction rejected**. Dedicated upload
inventory is **2,560 MiB versus 3,072 MiB**, one retained buffer. Dedicated
device-local inventory is 140 MiB; device-local/upload rings are 144/128 MiB.
Total staging is **2,972 MiB versus 3,500 MiB**, a 528 MiB reduction in this
comparison, including 512 MiB less upload inventory. These counters overlap other
allocation classes; do not add them to native allocation or physical RAM.
No retained buffer is freed. The final staging snapshot is **31.8 seconds old**,
so its reusable-byte counts do not establish safety to release resources at the
failure. Preserve actual tick, held, memory-type and import lifetime guards.

Native allocation at failure is 12,562,169,856 bytes (**11.70 GiB**) against a
12,713,115,648-byte recommendation, a 143.95 MiB margin. The recommendation is not
a hard allocation or physical-memory limit. Tracked Vulkan allocation peaks at
4,851 MiB, including 1,562 MiB of images, versus 4,771/953 MiB previously.
Image inventory grows later while staging remains stable. The two runs reach
different work, so staging savings do not establish lower total pressure or
identify the exact resource that exhausts memory. The failed chunk retains its
complete twelve-note CPU tail, with the same two compute candidates seen in an
earlier failure; this is not a precise failing instruction or proof of causation.

The visual journal has 3,769 events, including 1,256 presentations with API
success and one native error. Pixel capture stays off; correct pixels remain
unverified. The CSV has 1,242 complete rows and a different clock origin. Final
asynchronous counters include 715,689 missing-pipeline draws out of 1,571,829
requests, 412 pipeline-creation failures and 141,161 reduced decisions out of
141,347 scene decisions. There are 354 PerVertexKHR messages, 271 SRT-walk errors
and 22,799 clamps; totals from this longer run are not comparable error rates.

Next investigate the request and size rounding that produce the single 2.5 GiB
upload buffer, and the later image working set/lifetimes. Keep the audio correction
and existing settings. Do not trim retained buffers or disable split imports:
upstream documents late resource/argument-buffer lifetime failures. No runtime
change or tool-driven GPU run accompanies this evidence update; the prior ARM64
build, source regressions and full JIT/interpreter contracts remain the validation
baseline. Pixel readback and a tracing-overhead comparison still need native tests.

### Earlier half-rate audio and retained staging memory

The earlier owner run starts at 07:00Z on 2026-10-22, after the previous 06:39Z
comparison. Both stop on the actual Metal out-of-memory error (code 8) with
confirmed child/supervisor exit 23, after 59.026 and 64.496 seconds respectively.
The newest starts the main loop in 1.39 seconds, detects a DualSense, and has no
duration deadline, GPU-watchdog stop or import-stop report. A targeted lookup
afterwards finds its PID absent. Keep scene 640x360, DMA/async off and cache 23.

The latest failure records 12,465,848,320 native allocated bytes against a
12,713,115,648-byte recommendation, a 235.81 MiB margin. The failed native chunk
retains 96 of 160 CPU notes, including a compute dispatch and indirect/final draw
candidates. This does not identify the exact GPU instruction/resource responsible.
The visual journal retains 2,119 records, including 706 presentations with API
success and the native error. Pixel capture is off, so correct pixels remain
unverified. The line-buffered CSV has 693 complete rows; its clock origin differs
from the supervisor and visual journal.

Two active audio ports now have eleven five-second windows each. They deliver
about **23,912 input frames/s**, 49.82% of the requested 48 kHz; submission gaps
reach 74.33 ms. Output-lock waits peak at 8 microseconds, and no queue/provider
failure is recorded. This identifies a delivery problem rather than proving a
hardware underrun from zero input-queue samples. The old two-block queue cap can
feed only 512 input frames before waiting. An actual-source fake-device test with
a 1024-frame quantum reproduces half-rate delivery: **471 blocks in five seconds**.
The corrected adapter queries actual device frames/frequency, sizes a bounded
queue for a complete quantum plus a guest block, and produces **939 blocks** in
the same deterministic test without fabricated PCM. The native device's quantum
was not recorded in the old run; new windows include it. At this checkpoint,
native improvement was still unverified; the 07:23Z comparison above supersedes
that boundary. See [Audio diagnostics](AUDIO_DIAGNOSTICS.md).

Staging inventory is identical in both native comparisons: 3,072 MiB dedicated
upload, 140 MiB dedicated device-local and rings of 128/160 MiB, **3,500 MiB total**.
This is a subset of tracked/native allocations, not memory to add to them.
Zero held bytes do not prove buffers are reusable: outstanding ticks were not
counted. The old inventory was published only on frame ticks, so early all-zero
arrays mean unknown accounting. New snapshots include age, counts and completed,
unheld reusable bytes, with publishing after prewarm/new allocations too.

Source review finds a retained-cache inefficiency: completed sufficiently large
buffers were rejected when more than roughly 25% above a new request. The new
checked staging overlay selects the smallest sufficient buffer across size
classes, preserving memory type, deferred ownership and actual GPU-tick guards.
It frees no retained buffer and changes no import or fence behavior. Actual-source
CPU fixtures reproduce 288 MiB/five buffers for a sequential synthetic workload;
the adapted path retains 128 MiB/one buffer and preserves pending/held/type/best-fit
checks. At this checkpoint, these were CPU results rather than measured game
savings. The native comparison above now exercises reuse; do not enable trimming.

The newest counters also show 69,033 missing-pipeline draw requests out of 198,029
requests, with 195 failed pipeline creations, 180 PerVertexKHR log messages, 271
SRT-walk errors and 10,865 clamps. Asynchronous totals are not per-frame results
or a partition of every draw outcome. Reduced scene decisions are recorded for
21,722 of 21,756 scene decisions. General shader support remains unresolved.

The ARM64 library and full JIT/interpreter contracts pass with the new corrections.
No tool-driven GPU run was launched. The later owner-supervised comparison above
establishes the real quantum, delivery improvement and staging reuse while still
ending at the native memory boundary.

### Earlier visual evidence, copy recording and upstream watch

The owner now additionally reports choppy audio. [Audio delivery tracing](AUDIO_DIAGNOSTICS.md)
is enabled by default in the combined launcher and records private periodic
metadata in stderr/`audio.jsonl`, summarized by the visual report. It measures
actual submission gaps, input queue observations, deadline and queue/mutex waits;
it does not label input-queue zero as a hardware underrun. PCM handling and queue
policy remain. The old game log records only initial accepted output batches;
that initial run did not yet identify the delivery problem described above.

The latest code adds [bounded visual diagnostics](VISUAL_DIAGNOSTICS.md), enabled
as CPU metadata by the combined launcher. Optional `SOTC_VISUAL_CAPTURE=1` adds
completed game-frame readbacks, at most one outstanding, every five seconds and
24 attempts, with size/headroom/staleness guards. The offline report distinguishes
API results from pixels, dark/unchanged preview measurements and draw-skip/memory
totals. Complete timeline lines and the optional flip CSV are flushed; completed
capture records survive timeline rotation. Rendering stays 640x360. The native
ARM64 build, CPU evidence fixtures and full contracts pass. Later owner runs above
exercise metadata and reuse accounting; optional pixel capture and an overhead
comparison remain untested. Native memory exhaustion remains the current boundary.

[bmy/bbport-mac](DEPENDENCY_WATCH.md) was reviewed at its actual current default
branch `macos-native-0.6`. Its whole renderer/driver architecture is not imported.
Its specific 4096-region copy batching correction is adapted to our two production
copy paths, with capacity/alignment checks and actual-source CPU regressions. The
pinned methods reproduce the recording-size failure; the adapted ones preserve
copy data/order and barriers through larger cases. This prevents a separate CPU
assertion, not the native out-of-memory failure below. Pins/cache version remain.

The owner also requested ongoing dependency review; a daily public-source watch
is configured, and its source list/baseline is in [Dependency watch](DEPENDENCY_WATCH.md).
[MoltenVK #2858 and SPIRV-Cross #2699](PERVERTEX_REVIEW.md) are open drafts, not
production replacements. The compiler proposal builds separately and converts
four original synthetic per-vertex cases using an actual exported producer layout;
default and incomplete-layout paths correctly reject them. Native GPU linking,
topology/bindings, SotC shaders, pixels and runtime memory are not validated. Keep
the current renderer intact while designing any future isolated integration.

### Earlier confirmed native memory failure and reduced scene draws

The newest owner-run preset-4 capture starts the main loop at about 1.46 seconds,
detects a DualSense, and self-exits after **83.45 seconds** with confirmed child
and supervisor exit 23. The native prerequisite synthetic draw/readback passes.
There is no duration deadline, GPU-watchdog stop, import stop or shader-metadata
failure. The new Metal completion-error path is now exercised on the real device:

- The NSError domain is `MTLCommandBufferErrorDomain`, code 8, with description
  `Insufficient Memory` and an explicit IOGPU out-of-memory callback reason.
- At failure `currentAllocatedSize` is 12,508,184,576 bytes (**11.65 GiB**), versus
  `recommendedMaxWorkingSetSize` 12,713,115,648 bytes (**11.84 GiB**). The counters
  are about 195.44 MiB apart (98.39% of the recommendation). The recommendation is
  not a hard allocation limit; these are device/process counters, not total macOS
  RAM pressure or a complete resource inventory.
- The failing native chunk is retained and matched to a submitted command-buffer
  generation. Its complete twelve-note CPU tail contains two guest compute dispatch
  candidates, with group counts 1x1x1 and 4x1x1, plus bindings/barriers. One shader
  identity also appeared in the preceding run's erroneous native chunk; it is a
  repeated lead, not proof of the shader or exact GPU instruction that exhausted
  memory. A small dispatch does not establish a small total resource working set.
- CPU-only replay of both recorded compute modules converts successfully through
  the installed SPIRV-Cross MSL path. This checks conversion, not execution, GPU
  memory accesses, native pipeline acceptance or which resource caused the error.
- The error path terminates before the later watchdog/cancellation sequence. The
  supervisor confirms child exit, and a later targeted PID check finds it absent.
  This validates failure handling, not a repair for memory exhaustion. Its shorter
  elapsed time is not directly comparable to older runs that waited for a watchdog.

The scene configuration remains **640x360**, half targets 320x180 and UI 1920x1080;
DMA and async submission remain off. Unlike the preceding 107.54-second capture,
three later statistics windows now record reduced scene draws: 106/106, 406/407
and 499/501 per frame. This is evidence of the reduced scene path being used for
those draws, not complete image correctness. Other windows still record no scene
draws. There is no new owner visual report to validate scene contents.

Sampled tracked Vulkan allocations peak at 4,793 MiB and images at 992 MiB. The
final sample therefore has roughly 3,801 MiB beyond the counted images. The guest
backing pool is 6,592 MiB, imported through two overlapping windows whose declared
sizes sum to 7,104 MiB (512 MiB overlap). These figures include aliases and distinct
sampling times; do not add them as a physical-memory/RSS measurement. Reducing
scene pixels cannot shrink all these pools, temporary buffers, native resources
or compiled pipelines. Allocation-class accounting and safe reuse/lifetime are
now the main memory investigation; the exact allocation causing exhaustion remains
unknown. Keep the split imports: upstream documents incorrect MoltenVK offsets
past 4 GiB. Large staging buffers are intentionally retained upstream because
premature destruction caused later argument-buffer references/device loss; do not
enable `BB_STAGING_TRIM_LARGE` or disable residency as an untested remedy.

Rendering failures remain: 230 PerVertexKHR messages, 262 failed graphics pipelines,
271 SRT-walk failures and 19,605 buffer-size clamps. These are message counts across
different progress windows, not comparable unique-shader or error-rate reductions.
Unsupported-Phi messages and undeclared shared-U32 errors are absent. The frame CSV
retains 1,035 complete rows through about 66.30 seconds relative to its first row;
a partial final row and buffered loss prevent treating that timestamp as exit time.
No new GPU test was launched by the restricted tool process for this analysis.

### Earlier native stalls after the lookup/SRT update

Two subsequent owner-run tests stop on GPU progress, earlier than the preceding
156.90-second shader-metadata boundary. Both use preset 4, DMA off, async submission
off, JIT/statistics/journaling on and no duration deadline. The owner's report is
less visual progress. Do not describe these runs as rendering validation.

| Capture order | Elapsed | Observed stop | Child/supervisor status |
|---|---:|---|---|
| Previous metadata boundary | 156.90 s | Missing shader metadata | 23 / 23 |
| First new attempt | 16.61 s | GPU-progress watchdog | unknown / 1 |
| Latest attempt | 107.54 s | GPU-progress watchdog, with native Metal error evidence | unknown / 1 |

The prerequisite synthetic Metal draw/readback passes in the owner's Terminal.
Both game attempts detect a DualSense and enter the main loop (about 1.41/1.55 s).
The first attempt retains a pending native chunk with commit returned but neither
scheduled nor completion callback; the preceding chunk completed successfully.
The latest attempt instead records the preceding chunk's completion callback with
**status 5, error 8**. The installed Apple SDK defines these as
`MTLCommandBufferStatusError` and `MTLCommandBufferErrorOutOfMemory`: insufficient
memory to execute that command buffer. The old trace did not retain the NSError
domain/description. This is concrete native error evidence, not a measurement of
total macOS memory pressure or proof of which resource exhausted it.

The latest watchdog's awaited submission is absent from the retained journal (or
had not reached submit); it cannot be reconstructed from the preceding shader's
identity. The latest log still has 281 PerVertexKHR messages, 323 failed graphics
pipelines, 271 SRT-walk failures and 12,110 buffer clamps. Unsupported-Phi diagnostic
messages and missing shared-U32 errors are absent, but the run stops at a different
boundary. There is no new required metadata failure or snapshot. Do not claim the
previous missing shader is repaired based on this earlier exit.

The scene setting remains **640x360** (half targets 320x180, UI 1920x1080), but all
sixteen latest statistics samples have zero reduced-size scene draws. The earlier
metadata run has nonzero reduced scene draws. A preset selection therefore does
not prove those frames used the reduced scene path. Sampled Vulkan allocation
maxima are 5,167 MiB in the latest run versus 3,393 MiB in the previous metadata
run, while image maxima are 709 versus 1,988 MiB. These are different progress
windows and exclude other native/system allocations; neither establishes a total
RAM trend. CSV `alloc_mb` is **new device allocation per frame**, not live/resident
memory. Do not interpret its maximum as a process memory peak.

Both new supervisors observe cooperative cancellation quickly but cannot collect
child exit within the existing allowance. A later targeted existence check finds
both child PIDs absent; exit codes/times remain unconfirmed. No further GPU test
was launched from the restricted tool process.

A new checked `sotc-metal-command-error` overlay fixes independent error handling:

- The native completion handler always checks `MTLCommandBufferStatusError`, even
  with the optional submission journal disabled. Previously error status was only
  stored in the journal; the application continued until a later stall report.
- `runtime/src/gpu_command_failure.h` prints the real error domain, code,
  description, label, command-buffer identity, recording generation, and the
  device's actual allocation/recommended-working-set counters, then self-exits 23.
  It does not retry GPU work, fabricate successful progress or signal fake fences.
- With tracing enabled, the fatal report includes the bounded originating
  submission/open-recording candidate tail. Lookup matches both handle and
  generation; missing/evicted records remain explicit. This is not a per-command
  GPU execution marker.

Native ARM64 rebuilding, 23 digest/migration builder tests, full JIT/interpreter
contracts and CPU-only completion/journal fixtures pass. The latter check failure
with tracing on/off, unknown error details, success continuation, pre-submit
failure, recorded submissions and reused handles. No native GPU run validates this
new stop behavior or fixes the memory exhaustion. Pins/cache version 23 and the
640x360 configuration are unchanged. Fresh builds still need the missing external
glslang prerequisite.

CPU replay of the latest capture's 1,059 modules converts 778 (including four exact
vertex-zero rewrites); all 281 failures remain unsupported PerVertexKHR fragment
inputs. Private reports stay under `work/`. This confirms a persistent compiler
limitation, not driver or image correctness. There is no controlled bisect tying
the new stalls to one lookup/Phi change. Continue with memory/resource residency,
the exact per-vertex input path and actual scene eligibility before another native
comparison; preserve the earlier metadata evidence separately.

### Previous shader-metadata boundary and CPU-only continuation

The newest owner-run preset-4 capture reaches the main loop at about 1.47 seconds,
detects a DualSense, and stops after about 156.90 seconds with confirmed child and
supervisor exit 23: `SearchBinaryInfo` cannot find shader metadata. No duration
deadline, unresolved-import stop, missing shared-U32 declaration or GPU-watchdog
stop appears. This validates continuation beyond `sched_yield`, not stable gameplay
or correct rendering. The failing shader's role/address/backtrace is absent from
the old assertion; do not infer that it ran on a preparation worker.

The log still has 474 PerVertexKHR messages, 561 graphics-pipeline failures, 805
SRT-walk failures and 797 unsupported-Phi messages. Counts describe messages,
not unique modules or frames. Compressed-format storage-image failures and many
buffer-size clamps remain. The owner's visual report remains defective 3D animation.

Implemented after that capture, without GPU execution:

- `runtime/src/gpu_shader_metadata.h` reads actual `BinaryInfo` through bounded
  copies. It promotes prologue arithmetic before addition/multiplication, preserves
  the original 64 KiB candidate scan, supports unaligned trailers, validates full
  code ranges and never invents a length/hash. `gpu_shader_lookup.h` uses the pinned
  runtime's checked backing-memory hooks, so metadata reads do not themselves fault
  through GPU page tracking. The rest of shader compilation still uses the real code.
- Missing speculative graphics, geometry-copy or prepared-pipeline metadata marks
  preparation unavailable and retries on the owning GPU thread. A required missing
  program still stops with exit 23. It now prints role, thread, header, mapped length,
  hint and a native backtrace, and writes at most 64 KiB plus context under the private
  capture's `shader-metadata-failure/`. Existing evidence is preserved. This is
  diagnosis/retry behavior, not proof that the recorded required shader is valid.
- The portable SRT walker accepts a Phi only when every incoming edge emits exactly
  the same supported offset program. Constants, uniform user data, matching
  arithmetic and nested equal values retain their actual offsets. Divergent edges,
  lane reads, cycles, excessive depth and exponential expansion stay dynamic.
  Gameplay DMA remains disabled; synthetic DMA=1 checks initialize no GPU.
- Shader binary version 23 invalidates stale SPIR-V and corresponding metadata.
  Digest-checked migration includes the previous compatibility overlay (version 22).
- `tools/replay_shader_cache.py` compiles a standalone CPU converter using the actual
  exact-only compatibility policy and installed SPIRV-Cross. Each private module has
  an isolated ten-second conversion budget and input-size/header/instruction checks.
  It writes only a private JSON report, not converted game shaders. It does not link
  the renderer or Metal/Vulkan frameworks. Synthetic tests check exact-only policy,
  malformed files and dependency absence.

CPU replay of all 1,609 modules in the latest capture converts 1,134, including six
exact vertex-zero rewrites. The other 475 are fragment modules, all rejected with
`PerVertexKHR decoration is not supported in MSL.` The converted set has 591 vertex,
351 compute and 192 fragment modules. This standalone MSL 3.1 conversion omits
pipeline resource bindings, specialization and tessellation variants; it does not
prove MoltenVK pipeline creation, Metal compilation or pixels. Do not enable
approximate derivative reconstruction to hide the remaining per-vertex limitation.

The incremental ARM64 library rebuild, twelve actual SRT/shader-info IR cases in
both synthetic DMA modes, two metadata/lookup tests, wave64/exact-input regression,
15 auxiliary interface modules, pipeline-validity guards, 22 builder tests and all
46 host imports pass. Full native JIT/interpreter contracts also pass. The fresh
build's missing external glslang prerequisite is still unresolved; incremental
builds reuse existing generated assets. No new native game run validates these
latest changes. The tool's supervised prerequisite check still enumerates zero
Metal devices and exits 24 before a draw; do not bypass that restriction.

The owner first allowed a 320-pixel comparison, then explicitly restored the
**640x360 scene configuration**. No 320-wide scene change was applied. Preset 4,
DMA off and async submission off remain the native configuration; the existing
320x180 half-resolution targets are intermediate buffers, not a 320-wide scene.
Output/UI remain 1920x1080. Actual reduction eligibility still depends on each draw.
CPU-only conversion needs no pixel-resolution change.

For further autonomous analysis, use an existing private capture:

```sh
python3 tools/replay_shader_cache.py work/your-capture/cache/your-profile \
  --first-vertex-provoking --output work/shader-replay.json
python3 tools/check_shader_memory.py
python3 tools/check_shader_compat.py
```

Use `--first-vertex-provoking` only when the target device fixes the provoking
vertex to first (the reviewed native device lacks `VK_EXT_provoking_vertex`).
Reports and any failure snapshots remain private. Priorities: inspect a real
metadata failure snapshot if required lookup still fails; resolve general per-vertex
inputs exactly; then investigate compressed storage images, unresolved dynamic SRT
reads, clamps and the visible scene. A new GPU run is needed to measure pixels.

### Earlier yield and wave64 correction

**Newest native boundary and implemented corrections:** the owner-run preset-4
capture now confirms no duration deadline in both supervision layers. It detects a
DualSense, starts the main loop at about 1.37 seconds, and exits after about 160.93
seconds at the unresolved POSIX `sched_yield` import. Child and supervisor both
report exit 20; no GPU-progress watchdog stop is recorded. This establishes a new
import boundary, not stable gameplay. Rendering errors persist: 310 PerVertexKHR
messages, 354 graphics-pipeline creation failures and four undeclared shared-U32
compute failures. These are message counts, not unique shaders or frames.

The scoped POSIX `sched_yield` exports now call the native scheduler, preserve
guest errno on success and translate host failure to POSIX -1/guest errno. Guest
ABI checks in both JIT and interpreter cover both observed library scopes, thread
identity and rejection of data/unrelated/disabled bindings. Full native contracts
pass. Game continuation beyond this newly implemented import remains untested.

The checked local shader-compatibility overlay makes two renderer corrections:

- Wave64 ballot lowering runs after shared-memory simplification and previously
  introduced a U64 scratch view alongside its U32 stores on a device without
  explicit workgroup aliasing. It now loads the aligned pair as two U32 values and
  packs the same low/high halves, keeping the scratch reservation and barriers.
  The original pass reproduces missing shared-U32 MSL declarations in all nine
  synthetic compute modules; the adapted pass produces none. Checks exercise
  64/128/256-thread groups and three initial LDS sizes, including alignment padding.
- Before MoltenVK module creation, raw per-vertex inputs used exclusively at
  vertex zero use the existing upstream rewriter's exact Flat case only when the
  device fixes the provoking vertex to the first vertex. Configurable provoking
  vertex support disables this rewrite. Both new and cached module paths apply
  the policy. Ordinary modules and inputs reading
  vertices one/two or mixed vertices remain unchanged and explicitly unsupported;
  derivative reconstruction is not admitted into this compatibility policy.

Shader binary cache version 22 invalidates the old compute output. The original
three-backport, submission-journal and pipeline-validity states migrate through
digest-checked patches; partial and foreign states remain rejected. The new policy
lives in `runtime/src/gpu_msl_compat.h`; regression driver:
`python3 tools/check_shader_compat.py`. It uses synthetic IR/SPIR-V and no GPU or
game data. Conversion tests do not establish Metal driver acceptance or pixels.

The incremental ARM64 library rebuild, auxiliary emitter regression, actual
pipeline-validity guards, 21 offline builder tests and 46-symbol host-link check
pass. The full builder's prerequisite check currently cannot find the external
glslang executable referenced by the older configuration; the incremental build
reused the existing generated shader assets and succeeded. Restore a real glslang
compiler (or set `SOTC_GLSLANG`) before a fresh dependency/configuration build.
No real GPU experiment was launched for these corrections. SRT/Phi, general
per-vertex inputs, typed copies and correct scene rendering remain open.

### Earlier duration implementation and native capture

This checkpoint supersedes the older black-screen and stopping-boundary descriptions
below. The owner now **sees the 3D animation, with defects**. Correct scene rendering,
introduction synchronization, playable gameplay and stability remain unverified.

The newest reviewed native capture selects preset 4 (640x360 scene configuration,
320x180 half targets, 1920x1080 output), DMA off, async submission off, JIT and
statistics/journaling on. Initialization completes at about 1.10 seconds and the
main loop starts at about 1.62 seconds. A DualSense connects during the run, followed
by sixteen further statistics windows. There is no GPU-watchdog message, import
stop or CPU-fault report before the 120-second wall deadline. The supervisor then
fails to confirm cooperative child exit and ends after about 122.02 seconds with
unknown child status and exit 1, despite a cancellation notice after about 1.81 ms.
A later targeted PID lookup finds the process absent; its exit code/time remain
unknown. A preceding preset-4 run logs window closure and a confirmed natural
child/supervisor exit 0 after about 101.27 seconds.

The newest capture records 199 PerVertexKHR messages, 222 graphics-pipeline creation
failures, four undeclared `shared_mem_u32` compute failures, 271 SRT-walk failures,
263 Phi offset failures and 25044 buffer-size clamps. The final Metal sample still
reports 1696 draws skipped for missing pipelines in its interval. These establish
omitted rendering work, without isolating the owner's visible defects to one cause.
All twenty-two statistics windows print zero scene/reduced draws per frame; these
are rounded averages. `BeginRenderingFull` recognizes scene start using at least
five color targets and an eligible depth target. Preset selection alone does not
prove SotC's actual 3D draws were downscaled. Do not remove this condition blindly
or claim total GPU/memory pressure was reduced. Sampled Vulkan allocations/images
peak at 3795/586 MiB; imported guest memory remains unchanged and macOS pressure
is not measured. The CSV retains 1237 complete rows through about 113.53 seconds
relative to its first row, plus a partial row. Buffered loss prevents identifying
the final presentation time. No pending-submission dump appears because the
renderer watchdog did not trigger; no exact unfinished GPU command is identified.

**No-deadline game duration implemented at the owner's request:**
`SOTC_DIAGNOSTIC_SECONDS=0` disables the supervisor wall deadline and the native
guest alarm. The combined launcher now defaults to zero and explicitly passes the
same setting to both layers. The GPU-progress watchdog, DMA guard, cooperative
cancellation and manual window-close/Terminal interruption remain in place. Explicit
1..1800-second runs remain available; headless probes still default to 30 seconds.
The independent ten-second Metal precheck and 60-second renderer initialization
budget remain unchanged. No duration setting protects against a kernel/driver hang.

Relevant files are `tools/run_gpu_diagnostic.py`, `runtime/src/probe_entry_arm64.c`
and `packaging/test-arm64.sh`. Synthetic tests cover no-deadline natural exit,
GPU-stall cancellation, manual cancellation, launcher propagation and the native
guest alarm in JIT/interpreter. No real GPU experiment was launched for this change.
Validation: `bash packaging/test-arm64.sh --contracts-test` completed natively in
ARM64 with JIT/interpreter contracts passing, including all six duration/launcher
tests and twenty-two supervisor tests. One optional real Vulkan-loader test was
skipped because the isolated GPU dependencies were not present in the validation
checkout. Shell syntax, staged-source audit and patch application checks also pass.
The historical native capture above still used the old 120-second limit; it does
not validate the new mode or resolve the unconfirmed child exit.

After applying and validating these sources in the owner's working copy, the
no-deadline command preserves the preceding renderer settings:

```sh
SOTC_MEMORY_BACKING=file BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 \
  SOTC_DIAGNOSTIC_SECONDS=0 BB_JIT=1 BB_FRAME_STATS=1 BB_UPSCALE_PRESET=4 \
  bash packaging/test-arm64.sh --graphics-game-test
```

Close the window or press Ctrl+C to stop; inspect the log before another run.
Confirm the previous instance has closed first. Do not launch a real GPU experiment
from the restricted tool process. Preset 4 remains a requested scene configuration,
not proof that SotC's actual 3D draws are reduced.

## Goal and current evidence

The goal is native Apple Silicon execution of the PS4 game through bbcpu and the
pinned renderer. On Apple M1, the owner has visually confirmed game logos/menu and
heard the opening introduction audio. This confirms audible game output, but not
full audio coverage, audiovisual synchronization or correct introduction video.
Entering a playable 3D scene, gameplay stability, save persistence and gameplay FPS
remain unverified; there is no packaged playable application.

Supported input is the owner's readable CUSA08809 update 1.01 dump. ARM64 macOS on
Apple M1 is the observed host; the isolated GPU build targets macOS 15 or later.
Other game editions and hardware are unverified.

An earlier native run completes initialization at about 0.94 seconds and enters
the main loop at about 1.38 seconds. It continues past the save/dialog import stops,
then ends at the 60-second diagnostic budget. There is no unresolved-import stop or
CPU execution fault recorded in that interval. GPU activity continues after the
profile-save request. A timeout does not establish stable gameplay or rule out a
stall. The owner's observation confirms logos/menu and opening audio, not correct
rendering of an in-game scene. Draw/dispatch counters are not gameplay FPS.

The mount/cleanup/metadata error batch now handles eight related SaveData calls with
NOT_INITIALIZED and untouched outputs. Full contracts and comparisons against the
supplied original module pass in both CPU modes. The native game handles this mount
failure and attempts a save dialog. No save filesystem was added.

The two remaining save-dialog startup exports are now scoped: Initialize returns
NOT_SYSTEM_INITIALIZED because CommonDialog is unavailable, and Open validates its
base header/address cookie/reserved bytes and outer size before reporting no instance.
The initializer policy and its injected dependency-error comparison are distinct from
Open's original-module validation comparison. The subsequent native run continues
until timeout. After the host incident below, the owner authorized gradual GPU
testing. This tool process still exposes no Metal device, so real comparisons need
the owner's native Terminal. CPU/synthetic contracts do not load the real renderer.

The renderer still skips pipelines because of PerVertexKHR-to-MSL conversion (78
reported errors in an earlier run), and has SRT Phi and compressed-texture usage errors.
The isolated fragment diagnostic reproduces the compiler limitation without game data
or a GPU. IT_GET_LOD_STATS also remains unimplemented.

Physical SDL3 gamepad input now complements the keyboard fallback. See
[DualShock input](guide/building.md#dualshock-input) for connection and logging.
Guest ABI tests cover the entire stick/trigger domain, buttons, two touch identities,
hotplug, output errors and synthetic callback dispatch in both CPU modes. The latest
native 120-second-budget low-resolution run detects a DualSense, without a recorded
disconnection. The preceding short repeats have no physical connection logged.
An earlier complete summary records 778 physical samples,
387 accepted LED requests and 388 accepted rumble requests without disconnection;
the timeout run has no final input summary. These counters do not identify the
connection transport or prove the physical output effect.

## Host watchdog incident

The owner reported a macOS kernel panic with no watchdogd check-ins for 90 seconds.
The incident-associated private graphics log ends during renderer initialization, after
importing approximately 6.4 GiB of guest memory and preparing rendering workers.
It does not reach the renderer-ready line, linked-module initialization or guest
entry. Its selected dynamic-memory setting is 1. The preceding complete graphics
log uses the former configuration, reaches the main loop, reports a GPU submission
unfinished for more than ten seconds and then exits at the 60-second diagnostic
timeout. Neither log identifies the cause of the kernel panic. The earlier GPU
stall means the new dynamic-read setting cannot be the only suspected path.

The unvalidated default-on dynamic-read experiment is withdrawn. The loader now
sets `BB_DIRECT_MEMORY_ACCESS=0` and rejects every other explicitly supplied value
before `dlopen`, including load-only and renderer-only modes. The launch script
rejects those values before Vulkan/Metal prechecks. The unvalidated 300-second
default is withdrawn; the owner requested a 120-second combined budget after the
incident. Explicit 1..1800-second budgets remain supported, but a
user-space alarm cannot guarantee recovery from a kernel/driver hang. Do not
increase timeouts as a remedy or treat DMA off as evidence of safe GPU execution.

The initial pause is superseded by the owner's explicit request to resume gradually.
Do not repeat the DMA-enabled incident configuration or run an automatic sequence.
The normalized evidence above is sufficient for the public handoff; panic reports,
device identifiers, addresses and execution logs remain private.

## Gradual GPU testing

### Earlier compute stall in the newest capture

The newest owner-run capture has a 30-second wall budget, DMA off, async submission
off and JIT/statistics/journaling on, but **no `BB_UPSCALE_PRESET=4` override**.
Output is 1920x1080 and no reduced-scene transition is logged. Do not treat this
as the preceding 640x360 comparison. Initialization finishes at about 1.13 seconds
and the main loop starts at about 1.70 seconds. Only keyboard fallback is recorded;
there is no physical-controller connection in this capture.

The GPU watchdog reports unfinished submitted work after ten seconds. The supervisor
finishes after about 19.49 seconds with **unknown child status and exit 1**: the
cancellation notice arrives about 16.68 ms after the request, but child exit is not
observed within the cooperative allowance. A later targeted PID lookup returns
ESRCH. This establishes later absence, not a confirmed exit code or exit time.
The intermediate preceding log ends during GPU initialization without a supervisor
footer and cannot establish a completed comparison.

The awaited submission's queue call returned success. Its single Metal chunk has
commit return observed but neither scheduled nor completion callback observed;
the preceding chunk completed with status 4, error 0. There are no explicit
wait-semaphore entries, missing buffers, missing notes or missing Metal events.
The pending batch has 164 metadata notes with a retained 96-note tail: eight guest
compute dispatch candidates, one three-vertex draw candidate, transfers and helper
passes. Six guest dispatch notes use a 120x68x1 group grid. The final retained
compute candidate also appears in 31 buffer-size clamp warnings. This narrows
resource-provenance and compute investigation, but identifies neither the command
that executed nor a proven bad descriptor, deadlock or driver failure. The zero
status/error fields remain unobserved-callback defaults.

There are 50 SRT-walk failures, 42 Phi offset failures and 153 buffer-size clamps.
No PerVertexKHR, undeclared shared-memory or integer-blit failure appears before
this earlier stop; their absence does not validate the later 3D paths. No unresolved
import or CPU-fault report appears. The CSV is empty, so it supplies no presentation
count or timing. This changed configuration and shorter run cannot establish the
visual benefit or regression of the pipeline-validity correction below. Keep the
known low-resolution command in the tracing section for the next single supervised
comparison; the 120-second default and protective watchdog remain unchanged.

### Black scene and native submission evidence

**Current result:** the owner reports a black screen where the 3D animation should
appear. The preceding complete low-resolution native run uses the same DMA-off, async-off,
JIT/statistics-on settings with the submission journal enabled and a 120-second
budget. It detects a DualSense, finishes initialization at about 0.94 seconds and
starts the main loop at about 1.38 seconds. Scene raster remains 640x360, half-size
targets 320x180 and UI/output 1920x1080. The GPU watchdog stops it after about
114.91 seconds; child exit 21 is confirmed and supervisor exit 31 preserves the
stall reason. No import stop, CPU-fault report or cleanup error is recorded.

The native journal now identifies the awaited batch's recorded work. Its submit
call returned success, with one command buffer and no explicit wait-semaphore
entries. Its sole Metal chunk has commit return observed but no scheduled or
completion callback observed. The immediately preceding empty batch's Metal chunk
has both callbacks and completion status 4, error 0. The awaited chunk's zero
status/error fields are defaults because no completion callback arrived, not a
polled Metal status or evidence that no driver error occurred. These observations
do not prove which command started or whether the stall is within that batch,
earlier queue dependencies or the driver.

The awaited batch has 28632 metadata notes, not that many draws. Only the 96-note
tail is retained. It includes sixteen indexed indirect draw candidates from two
vertex/fragment shader pairs, with maximum draw count 1 and stride 20. The actual
indirect index/instance counts were not read. Both pairs also appear in failed
graphics-pipeline messages immediately following PerVertexKHR conversion errors.
This links candidate shader pairs to a concrete compiler limitation, but pairs
can share several pipeline variants: it does not prove which variant was bound
or which GPU instruction blocked. The awaited record has no missing buffers;
missing-note and missing-Metal-event counts are zero. Cumulative ring evictions
do not reconstruct the omitted prefix or constitute per-command GPU breadcrumbs.

The log records 118 PerVertexKHR messages, 133 graphics-pipeline creation failures,
four undeclared `shared_mem_u32` compute failures, two integer-format Metal blit
failures, 271 SRT-walk failures, 8094 clamps and 267 incomplete GPU-mapping warnings.
The fragment-interface CPU fixture still reproduces the installed SPIRV-Cross
limitation (expected exit 2); it is not a passing shader-support test. Missing
pipelines and omitted copies are rendering defects consistent with missing scene
content, but the owner's black image has not been isolated to one of them.
Sampled Vulkan allocations/images peak at 5304/1318 MiB, not measured macOS RAM.
The CSV retains 438 complete rows through about 60.53 seconds relative to its
first row, plus a partial row. Buffering prevents dating the last presentation.

Source review exposes a separate validity defect worth correcting before another
capture. `GetGraphicsPipeline` checked the handle after first creation and in the
prepared path, but its ordinary cache-hit path returned the object unconditionally.
A rejected pipeline could therefore be offered again with an empty handle.
`GraphicsFromVulkan` also accepted a nonnull object with an empty handle, allowing
different native Metal entries to share the null Vulkan lookup key. The local
validity overlay checks both paths. This consistently rejects unsupported state;
it does not fabricate a successful pipeline or bypass shader semantics.

The regression compiles the actual source guard blocks with synthetic handles:
the original guards fail both checks, the adapted guards reject null/empty entries
and preserve valid ones. The ARM64 library rebuild, fifteen auxiliary SPIR-V cases,
the new CPU guard test, twenty offline builder tests and full JIT/interpreter
contracts pass. Pristine sources,
the exact original three-backport state and the preceding journal overlay can
migrate; mixed/unknown sources remain rejected. No GPU run of this correction was
launched. Native image verification, PerVertexKHR support, compute shared-memory
emission, typed blits and the precise stall cause remain open.

**Earlier uninstrumented low-resolution stall:** the owner's run uses a 120-second budget
with DMA off, async submission off, JIT/statistics on and scene preset 4. Initialization
completes at about 0.93 seconds; a DualSense is detected before the main loop starts
at about 1.37 seconds. There is no recorded physical disconnection or input-backend
failure. This establishes detection in this run, not transport, physical output
effects, completed hotplug in the preceding run or absence of every input issue.
Scene raster remains 640x360, half-resolution targets 320x180 and UI/output 1920x1080;
later statistics windows record reduced scene draws.

The renderer reports submitted GPU work unfinished for ten seconds, with the last
completed timeline value immediately before the awaited value and later recording
work already present. The supervisor stops on that message before the 120-second
wall deadline: confirmed child exit 21, supervisor exit 31, elapsed about 112.05
seconds. The cancellation notice is observed about 5.24 ms after the request and
there is no cleanup error, import stop or CPU-fault report. There is no recorded
device-lost error. The pinned `Rasterizer::GpuSignalLoop` waits on the work semaphore;
`GpuWatchdog` distinguishes this branch from a tick not yet submitted. This does
not identify the command, shader, dependency or driver behavior responsible, prove
a hardware hang, or report a new host kernel panic. Do not fake label completion,
disable the watchdog or extend the budget as a remedy.

The last statistics window reports a worst flip interval of about 22.17 seconds
and 355 shader/pipeline compiles totaling about 12.75 seconds. These are aggregate
observations, not proof that compilation caused the stalled submission. Sampled
Vulkan allocations rise from 4307 to 5740 MiB, and images from 490 to 1418 MiB;
imported guest memory remains 6592 MiB. The Linux-style zero RSS/VRAM/GTT fields do
not measure macOS pressure. Low pixel count has not eliminated the progress failure.

Both typed Metal blit failures persist, along with 161 PerVertexKHR messages,
263 Phi offset messages, SRT-walk failures, 9567 buffer clamps and 399 incomplete
GPU-mapping warnings. Message counts are not distinct shader/resource counts or
causal evidence. The last Metal sample also reports draws skipped for missing
pipelines. The CSV retains 519 complete rows through about 38.03 seconds relative
to its first row, plus a partial row; cancellation buffering prevents treating
this as the final presentation time or explaining the remainder of the run.

The source/log review did not change shader semantics, dependencies or GPU settings.
Its follow-up submission instrumentation is described below. The typed-copy fixes
remain pending. Keep the protective stop and DMA-off setting. Earlier confirmed
and unknown stop results remain separate historical observations.

### Pending submission tracing

The historical log identifies the awaited timeline value and its predecessor, but
not the draw, dispatch or transfer inside that batch. It explicitly reports that
`VK_AMD_buffer_marker` is unavailable, so the existing GPU breadcrumbs are disabled.
The private cache contains compiled shaders and pipelines, not their submission
membership or execution order. A recently created cache file is not proof that
its shader ran, and no command list can be recovered retrospectively from it.
Four compute compilation failures also reference undeclared `shared_mem_u32`;
they establish a compiler problem, not the cause of the awaited submission.

A local checked overlay now connects the scheduler's logical submission to the
Metal command buffers it commits. `runtime/src/gpu_submission_trace.h` owns the
bounded, thread-safe CPU journal. The combined launcher enables
`SOTC_GPU_SUBMIT_TRACE=1` by default; 0 disables it. Direct library users must opt in.
The supervisor logs the setting. The journal retains:

- Logical scheduler/tick, wait and signal semaphore values, and whether the queue
  submission call returned, with its result. A prepared entry is not a submitted
  or completed GPU operation.
- Vulkan command API names and guest draw/dispatch shader identities and counts
  from `RecordCrumb`. These are CPU-recorded candidates. Host helper passes without
  a guest crumb have API names but may lack shader identities. Indirect argument
  contents and resource memory are never read by this journal.
- Actual Metal chunk commit-return, scheduled and completion-callback observations,
  including partial `CommitChunk` buffers, status and error codes. Scheduled is not
  proof that an individual shader started. A completion callback is per Metal
  command buffer, not per draw. Recording generations prevent handle-reuse confusion.

There are at most 256 open buffers, 512 submission records, 1024 Metal chunk records
and a 96-note tail per recorded buffer/batch. `notes` includes shader metadata and
API calls; it is not a GPU instruction/draw count. Truncation, missing buffers and
evictions remain visible. On the ten-second watchdog callback, the awaited and
last-completed batch snapshots are printed and flushed **before** the existing
watchdog prefix; otherwise the supervisor could exit before the details arrive.
No new GPU marker writes, guest-memory access, fence completion or render commands
are introduced. Callback/journal overhead is unmeasured on a native GPU run.

The ARM64 library compiles and links with the local overlay, and all 46 host imports
are present. The actual auxiliary SPIR-V regression and full JIT/interpreter
contracts pass. Native CPU fixtures check dependencies, prepared/returned states,
recording reuse, bounded tails, concurrency and callbacks. A CPU-only supervised
fixture proves the dump precedes the watchdog prefix and survives cooperative exit.
Twenty offline builder tests cover pristine application, exact previous-set
migration, unknown/partial rejection and idempotence; twenty supervisor tests pass.
The owner's first native capture is analyzed above. No GPU capture was run in the
tool environment.

For the next single owner-run capture, keep the previous low-resolution renderer
settings and 120-second observation budget; the protective GPU-watchdog stop remains
active. From the repository root:

```sh
SOTC_MEMORY_BACKING=file BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 \
  BB_JIT=1 BB_FRAME_STATS=1 BB_UPSCALE_PRESET=4 \
  SOTC_GPU_SUBMIT_TRACE=1 SOTC_DIAGNOSTIC_SECONDS=120 \
  bash packaging/test-arm64.sh --graphics-game-test
```

The local library is already rebuilt. On another checkout, first run
`python3 tools/build_gpu.py`; an old library cannot implement this journal simply
because its environment contains the new flag. Inspect the resulting private log
before another experiment. Submission IDs can change between runs. Even a complete
CPU list may leave several candidates without per-command GPU execution markers.

### Earlier low-resolution comparisons

**Preceding 30-second repeat:** the owner repeats the low-resolution configuration and reports
that the window closes when turning on the controller. The only input line records
the logical controller opening with keyboard fallback. There is no physical
connection/disconnection message, input-backend failure, unresolved-import stop,
CPU-fault report or GPU-watchdog warning. The recorded stop is the existing
30-second wall deadline, followed by the native cancellation notice observed about
1.90 ms later. This evidence does not establish whether hotplug completed or caused
any separate problem; do not diagnose a controller crash from timing coincidence.

The supervisor again fails to collect the child status within its unchanged
two-second allowance: unknown child status, supervisor exit 1, elapsed about
32.02 seconds. A later lookup of that specific PID returns ESRCH. This confirms
later process absence, not its exit code, exact exit time, a successful supervised
stop or a permanently live process. Preserve the owner's window observation and
this failed collection separately. Do not increase the grace period as a presumed
fix or enable external-signal fallback.

Initialization completes at about 1.07 seconds and the main loop starts at about
1.53 seconds. Scene raster remains 640x360 with 320x180 half-resolution targets
and 1920x1080 UI/output. Five statistics windows record nonzero reduced scene draws
after the first window; the last is 27 reduced draws/frame out of 33 scene draws.
The CSV retains 399 complete rows through about 23.70 seconds relative to its first
row, plus a partial row. Buffered-row loss still prevents a final frame count.
Both integer-format Metal blit failures recur, alongside the earlier shader/resource
errors. Sampled Vulkan allocations peak at 4772 MiB and images at 931 MiB; these
are renderer counters, not measured total macOS RAM pressure or proof of lower load.

The owner also requests direct GPU use now that the scene resolution is reduced.
GPU rendering is already active in the owner's native runs; DMA-off is a separate
dynamic shader-read setting. One supervised synthetic Metal prerequisite recheck in
the tool context still exposes zero devices and confirms child/supervisor exit 24
after about 0.17 seconds, before any draw or game renderer launch. This is a host
execution-access limitation, not a repository GPU-disable switch. No escalation,
alternate Terminal-control route or additional native game test was attempted.

**Preceding confirmed-exit result:** the owner's single low-resolution comparison confirms the
preset was applied: scene raster targets are 640x360 with 320x180 half-resolution
targets and 1920x1080 UI/output. The last two statistics windows record 26 reduced
draws/frame out of 26 scene draws, then 24 out of 24. This verifies application to
some SotC scene passes, not all draws, correct displayed images or gameplay.
Initialization finishes at about 1.90 seconds and the main loop starts at about
2.81 seconds. The 30-second stop request is followed by a native cancellation
notice and confirmed `child_returncode=21 exit=21` after about 30.37 seconds. The
notice is observed about 0.91 ms after the request. There is no cleanup error,
import stop, CPU-fault report or GPU-watchdog warning. This confirms the stop in
this run, without explaining earlier unknown exits or proving system stability.

The CSV retains 240 complete rows spanning about 18.37 seconds from its first row.
The explicit 120-flip flush and cancellation `_Exit` can discard later buffered
rows, so this is not a final frame count or the time presentation stopped. Four
statistics windows report flip rates about 8.0 to 14.9/s; these do not establish
gameplay FPS or a causal performance change. The initial idle figure exceeds
100%, so it is not a usable instantaneous GPU-utilization/pressure measurement.
No physical gamepad detection is recorded in this log; image progression is still
unreported. Shader/resource failures remain: 54 PerVertexKHR errors, 259 Phi offset
errors, 267 SRT-walk failures, six compressed-format/usage errors and 3,637 clamps.
Lower message counts are not evidence of corrected shaders or better output.

Imported guest memory remains 6592 MiB. Sampled Vulkan allocation maxima are
4867 MiB versus 4593 MiB in the preceding baseline, and Vulkan image maxima are
797 versus 751 MiB. These different snapshots/progress intervals do not establish
total macOS memory pressure or isolate a causal preset effect, but they provide
no evidence that this test reduced total memory. Full-size originals and UI remain
allocated while reduced proxies add storage. See the source-verified limitations
in the comparison below.

Two new Metal pipeline errors identify a concrete next renderer target: R32Uint
and RG32Uint blit destinations reject the generic float4 fragment output. In the
pinned `renderer_metal/mtl_frame.mm`, `BlitPipeline` chooses only float-color or
depth shaders, and `BlitImage` returns without copying when the pipeline is nil.
The failure is cached by destination format, so two messages do not mean only two
copy operations were affected. A correction needs typed integer reads/outputs and
nearest-copy semantics that preserve integer values, rather than a float output
cast or skipped-copy success. Validate pixel formats, rectangles, mip selection
and representative integer values with synthetic data before another native
comparison. The new sampler-LOD-bias warning is a separate remaining limitation.
No renderer patch or further native game comparison was made for this log analysis.

**Earlier timed-notice checkpoint:** that instrumented run records its cancellation notice
about 1.47 ms after the parent stop request, but still ends with unknown child status
and supervisor exit 1 after about 32.02 seconds. The owner confirms that the window
closed itself, without manual termination. A subsequent targeted `proc_pidpath`
lookup returns ESRCH for that run's PID. This confirms process absence at the later
check, not its exit code, exact exit time or the cause of the supervisor's failed
collection. No process sample was taken because that PID was no longer present.
Do not describe this execution as a permanently live process or a proven driver
hang; do not retroactively replace the unknown return code with success.

Initialization finishes at about 0.92 seconds and the main loop starts at about
1.35 seconds. The same DMA-off, async-off, JIT/statistics-on 30-second settings are
recorded, with five statistics windows, no import stop, CPU-fault report or GPU
watchdog warning. Its CSV retains 438 complete rows through about 25.37 seconds
relative to the first row, plus a partial row. Errors persist: 52 PerVertexKHR,
259 Phi offset, 267 SRT-walk, six compressed-format/usage errors and 7,687 buffer
clamps. The DualSense is detected. This run's image progression remains unreported.

The owner now explicitly authorizes low-resolution GPU tests to reduce load.
The next single scene-preset comparison is prepared below. A supervised Metal
prerequisite attempted from this tool context returns zero devices and status 24
after about 0.35 seconds, with a confirmed child exit. No real game/GPU test was
launched here, and no permission escalation or alternate Terminal-control path
was attempted.

**Earlier acknowledgement checkpoint:** that run includes the output-backpressure correction and
now emits `STOP_DIAGNOSTIC_CONTROL: parent requested termination`. The supervisor
still fails to observe child exit within its two-second allowance. Its footer
reports `child_returncode=None exit=1` after about 32.02 seconds. This is still a
failed stop. The notice comes immediately before `_Exit` in our control thread;
it proves that the reporting point was reached, not that the exit call completed
or that the OS reaped the child. Neither a driver/kernel teardown cause nor an
insufficient grace period is established. Do not increase timeouts as a remedy.

The header retains DMA off, asynchronous submission off, JIT on, frame statistics
on and the same 30-second budget. Initialization finishes at about 1.03 seconds
and the main loop starts at about 1.65 seconds. There is no unresolved-import stop,
CPU-fault report or GPU-watchdog warning. Five statistics windows and 438 complete
CSV rows are retained, through about 25.23 seconds relative to the first row,
followed by a partial row. Pixel correctness and image progression remain
unreported. Errors persist: 48 PerVertexKHR errors, 259 Phi offset errors, 267
SRT-walk failures, six compressed-format/usage errors and 7,423 buffer clamps.
The DualSense is detected; no final input/audio summary is available.

The supervisor now records `child_pid` before reading child output and appends
`control_notice_seen` and `control_notice_after_request_ms` to its completion
footer. The latter is a parent-side observation delay, including output delivery;
it is not the child's exact cancellation latency, kernel teardown time or an
exit acknowledgement. The existing child return code remains the exit evidence.
A split-notice/delayed-self-exit synthetic fixture verifies that observing the
notice still produces a failed stop while exit remains unknown. GPU settings,
the wall budget, two-second allowance and no-signal-fallback policy are unchanged.
No GPU run was launched for these logging changes.

All 20 supervisor tests and four native diagnostic-budget tests pass after the
logging change, using only synthetic processes/guests. The native control header
and renderer are unchanged from the preceding backpressure correction.

Before another GPU instance, confirm that the prior `probe_entry_arm64` process
has ended. The latest recorded PID is absent at the targeted check above. Window
closure alone is insufficient when the supervisor cannot reap a future instance.
An exit-phase process state/stack could distinguish the remaining collection
failure; this is separate from the authorized scene-preset test. The private log's PID
identifies the owned diagnostic in Activity Monitor. If it remains present after
the failure, use Sample Process for that specific diagnostic and keep the report
under ignored `work`. Record a sampling failure or an already-exited process as
such; do not request elevated access or substitute external termination signals.

### Low-resolution scene comparison

The owner has run the single comparison adding only `BB_UPSCALE_PRESET=4` to the
preceding 30-second settings; its measured result is summarized above. The command
below records that configuration, not an instruction to rerun automatically.
The pinned `BbSettings::Load` accepts this preset;
`SceneResolution::ForPreset` requests 640x360 from the default 1920x1080 output,
with 320x180 half-resolution effects where eligible. This uses the existing
host-side raster proxy path. The window, UI/output images and imported guest pools
retain their preceding sizes. Do not set `BB_RENDER_RES`: its compatibility path
assumes a game-specific startup resolution patch that is not applied to SotC.

```sh
SOTC_MEMORY_BACKING=file BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 \
  SOTC_DIAGNOSTIC_SECONDS=30 BB_JIT=1 BB_FRAME_STATS=1 BB_UPSCALE_PRESET=4 \
  bash packaging/test-arm64.sh --graphics-game-test
```

Keep the default FSR 3.1 provider and output resolution; do not add frame-limit,
worker, DMA or memory-budget changes to this comparison. The supervisor header
now also records `BB_UPSCALE_PRESET`, `BB_RENDER_RES` and `BB_OUTPUT_RES`, so inherited
overrides can be identified. A CPU-only check compiles the actual pinned settings
implementation and scene-size header, then verifies preset 4 selects 640x360 without
a fixed guest-resolution session. It does not exercise rasterization or a GPU.

Look for `Scene resolution: raster 640x360` and nonzero `reduced-size draws` in the
five-second statistics. The setting/size message alone does not prove that SotC's
draws used the proxies: the renderer checks scene dimensions, formats, attachments
and pass eligibility. Native-size guest images remain allocated and reduced proxy
images can be additional allocations. Pixel work may fall for eligible passes;
lower total memory pressure, fewer shader failures, image correctness and host
stability are not established. Guest pool imports remain approximately 6.4 GiB.
Read the new log and memory counters before another comparison. This tool context
has no Metal device; actual validation needs the owner's native Terminal.
The upstream VRAM/GTT/RSS fields read Linux `/proc` files and report zero on this
Mac; those zeros are not evidence of low memory pressure. Vulkan allocation/image
counters describe only their own allocations, not total macOS unified-memory use.

**Earlier unacknowledged-stop checkpoint:** after the POSIX clock-alias correction, that instrumented
run reaches its 30-second deadline without an import stop, CPU-fault report or GPU
watchdog warning. Initialization finishes at about 0.91 seconds and the main loop
starts at about 1.34 seconds. DMA and asynchronous submission remain off, with JIT
and frame statistics on. The log records five flip-statistics windows. Its CSV
retains 438 complete rows through about 24.80 seconds relative to the first record,
followed by a partial row. Flip records do not establish pixel correctness, game
progression or gameplay FPS; the latest image progression is still unreported.

The supervisor records its deadline request, followed by a cooperative-stop error:
child exit is not observed in the two-second allowance. Its footer reports
`child_returncode=None exit=1` after about 32.04 seconds. There is no native
`STOP_DIAGNOSTIC_CONTROL` acknowledgement. Classify this as a failed stop and ensure
the previous instance has closed before another GPU comparison. Do not infer a
kernel/driver hang, clean exit or continued process lifetime solely from this file.
Shader/resource failures persist: 64 PerVertexKHR errors, 259 Phi offset errors,
267 SRT-walk failures, six compressed-format/usage errors and 7,942 buffer clamps.
The DualSense is detected again; no final input/audio summary is available.

CPU-only fixtures reproduce two output-backpressure failure mechanisms in the old
cancellation code: a full inherited output pipe blocks the native stop notice
before `_Exit`, and a child writing queued output cannot exit while the supervisor
waits without draining it. Both fail before the fix. The native control thread now
makes its notice nonblocking and best effort, then exits regardless of delivery.
The supervisor consumes bounded output batches while awaiting exit, retaining the
same two-second allowance and never using external-signal fallback. All 19 targeted
supervisor tests pass, including the two regressions. This explains a reproducible
mechanism consistent with the native log, but does not establish that it caused
that native failure. No new GPU run was launched for this correction, and native
stop confirmation remains pending.

The full synthetic `--contracts-test` passes in JIT and interpreter with this
change. The Metal prerequisite also compiles with the updated control header;
it was not executed. Neither check loads a real GPU or validates recovery from a
kernel/driver hang.

**Earlier import-stop checkpoint:** the preceding flip-statistics run ends at an unresolved POSIX `usleep`
import in the original NGS2 audio module, on its sound-system thread. Initialization
finishes at about 1.17 seconds and the main loop starts at about 1.63 seconds. The
supervisor confirms `child_returncode=20 exit=20` after about 19.67 seconds, before
the 30-second deadline. There is no CPU-fault report, GPU-watchdog warning or cleanup
error. DMA and asynchronous submission remain off, with JIT and frame statistics on.
The run's visual progression has not been reported; do not attach the prior
first-logo observation to it automatically.

The private CSV retains 134 complete flip rows spanning about 9.03 seconds relative
to its first row, with a longest recorded interval of about 1.12 seconds. One
five-second summary reports a 16.7/s flip rate. These are presentation-path records,
not proof of distinct displayed images, correct pixels, gameplay FPS or absence of
a later stall. This import-stop path explicitly flushes streams before `_Exit`;
concurrent work can still end between observations. Do not use the CSV's relative
origin as process startup time or its last row as the process stop time. The file
still has 49 PerVertexKHR errors, 220 Phi offset errors, 228 SRT-walk failures, six
compressed-format/usage errors and 1,153 buffer clamps. Input uses keyboard fallback
with no physical gamepad detected; the earlier comparison detected a DualSense.

The missing symbol's NID matches POSIX `usleep`, and the supplied module's import
identity is `libkernel` library version 1 / module version 257. The linker correctly
normalizes that identity into the executable's kernel namespace. Our clock resolver
only bound the POSIX-library spelling. It now binds the exact kernel-library alias
to the same real POSIX sleeper, preserving interruption and errno semantics. The
same review found the already-present POSIX `gettimeofday` import in the kernel
namespace, so its checked adapter receives that exact alias too. No NID-only
fallback, new timing policy or fabricated success is introduced. The next native
comparison uses the same instrumented 30-second configuration below.

The full synthetic `--contracts-test` passes in JIT and interpreter after this
change, without loading the real GPU or game. New guest-call checks cover both
scopes, rejected data/foreign scopes, zero and real microsecond waits, 32-bit
argument truncation, signal interruption with POSIX EINTR, success errno
preservation and guarded realtime outputs. The subsequent native run has no import
stop during its recorded interval, as summarized above.

**Earlier cooperative-stop checkpoint:** an earlier native GPU run confirms cooperative cancellation.
Its header records DMA off, asynchronous submission off and JIT on. The control
channel is ready before renderer initialization, which finishes at about 1.10
seconds; the main loop starts at about 1.63 seconds. At the 30-second deadline,
the log records the stop request, `STOP_DIAGNOSTIC_CONTROL` and a completion footer
with `child_returncode=21 exit=21` after about 30.52 seconds. There is no cleanup
error, unresolved import, CPU-fault report or GPU-watchdog message. This verifies
the stop for that run; it is not a renderer-stability or gameplay result.

**Earlier visual boundary:** the owner reports only the first logo, without reaching the
second. The earlier menu and introduction-audio observations remain historical;
this run does not reproduce them. The duration and submission configuration differ
from earlier runs, so neither is an established cause of the missing progression.
The last sampled Metal activity drops to one draw and nine dispatches, but these
are command counters, not a record of displayed images or a diagnosis of a stall.

That file still has 21 PerVertexKHR/skipped-pipeline messages, 220 Phi offset
errors, 228 SRT-walk failures, six compressed-texture usage errors and 910 clamps.
The DualSense and Metal command activity are recorded. Message/command counts are
not FPS or evidence of better rendering. The subsequent investigation records flip
progress with the existing host-side statistics described below. Do not change
renderer settings based solely on error counts. The following paragraphs retain
earlier checkpoints.

The earlier completed native log uses DMA off and a 120-second budget. Initialization completes
at about 1.69 seconds and the main loop starts at about 2.33 seconds. The run ends
at STOP_TIMEOUT without an unresolved import, CPU fault or GPU-watchdog message.
It records Metal draw/dispatch activity throughout its sampled intervals and detects
the DualSense. This does not verify scene entry, introduction-video correctness or
system stability. Five PerVertexKHR conversion errors, 220 Phi offset errors, 228
SRT-walk failures and 996 buffer clamps remain; message counts are not distinct
shaders and cannot be compared as performance or correctness improvements.

`tools/run_gpu_diagnostic.py` now supervises the combined process in a separate
session. Its wall budget includes renderer startup. Owned diagnostics now receive
cancellation through an inherited pipe and exit themselves from a native monitor
thread. The channel is validated/started before renderer initialization. A GPU
watchdog stop returns 31; deadline expiration returns 21 after confirming child exit.
Parent cancellation or parent-pipe closure also requests self-exit. The launcher
uses `--cooperative-stop` and never escalates to external signals in this mode.
Generic process-group signaling remains available for synthetic process fixtures.
The child exit status is otherwise preserved, output is mirrored into a new private
log, and there is no retry. This is an additional user-space stop, not protection
against a frozen kernel/driver. The standalone Metal prerequisite uses a separate
ten-second supervisor. The combined default remains 120 seconds.

The owner has now run the 30-second comparison with asynchronous submission off
(`BB_ASYNC_SUBMIT=0`), DMA off, JIT on and the same renderer pins. The header confirms
the settings and the two recording workers remain. Initialization finishes at
about 0.924 seconds and the main loop starts at about 1.36 seconds. Five Metal
samples record draw/dispatch activity, with no import stop, CPU-fault report or
GPU-watchdog warning in the file. Its last write is near the 30-second boundary,
but there is no child timeout, supervisor stop or completion footer. Therefore the
exit cause is unknown; do not classify it as a confirmed timeout or clean exit.

This run records 48 PerVertexKHR errors/skipped-pipeline messages, 259 Phi offset
errors, 267 SRT-walk failures, six compressed-texture usage errors and 7,788 buffer
clamps. Its larger command counts and error counts do not establish better FPS,
worse rendering, scene entry or a causal benefit from the submission setting.

The supervisor persists a stop decision before cleanup and records
the observed child status, supervisor exit code and elapsed time on its normal
completion paths. Natural exits, busy output, child signal statuses and timeout
cleanup output are covered with synthetic processes. The earlier missing footer
cannot be reconstructed retrospectively. No additional GPU test was launched for
this change.

The next native log now confirms the deadline decision at 30 seconds, followed by
`STOP_GPU_SUPERVISOR_CLEANUP: Operation not permitted`. Its footer reports child
status unknown and supervisor exit 1 after about 30.56 seconds. This establishes a
cleanup failure, not confirmed child termination. It again reaches the main loop
(about 1.38 seconds), with no import stop, CPU-fault report or GPU-watchdog message.
The GPU shader/resource failures remain. The earlier generic error did not identify
the exact cleanup syscall; stage-specific errors are now recorded as well.

The owned probe and Metal check use the cooperative channel to avoid depending on
external signal permission. This does not elevate privileges, retry denied signals
through another API or guarantee recovery from a kernel/driver hang. CPU-only
native fixtures check deadlines despite ignored TERM, parent cancellation, parent
channel closure and invalid descriptors. Synthetic infinite guests verify the
actual probe's channel in JIT and interpreter before its longer own alarm.
No GPU run was used to implement that change. The subsequent native run confirms
child exit under GPU load, as summarized above. This remains the reference
configuration; it is not an instruction to rerun automatically:

```sh
SOTC_MEMORY_BACKING=file BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 \
  SOTC_DIAGNOSTIC_SECONDS=30 BB_JIT=1 \
  bash packaging/test-arm64.sh --graphics-game-test
```

Inspect the newest game log before another run. Record what was actually visible,
whether progression stopped, the selected setting, terminal stop and any GPU-stall
message. Keep other renderer settings unchanged and do not turn DMA back on.

### First-logo presentation trace

The owner has run the pinned renderer's `BB_FRAME_STATS=1` instrumentation. The
following command records its preceding baseline for comparison; the current
prepared next test is the [low-resolution scene comparison](#low-resolution-scene-comparison)
above. Do not automatically repeat this baseline:

```sh
SOTC_MEMORY_BACKING=file BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 \
  SOTC_DIAGNOSTIC_SECONDS=30 BB_JIT=1 BB_FRAME_STATS=1 \
  bash packaging/test-arm64.sh --graphics-game-test
```

No additional GPU run was launched for the new observation fields. Use a native Terminal only
after the prior diagnostic process has ended; inspect the resulting log before
another comparison. The launcher chooses a private `frames.csv` inside this run's
isolated GPU-state directory unless `BB_FRAME_LOG` is already specified. The
supervisor records both settings and the CSV path in its header. Remove
`BB_FRAME_STATS` from the environment to return to the preceding baseline.

In the pinned `VideoOutDriver::Flip`, this instrumentation reads host clocks and
existing counters after the presenter call. It reports flip-rate/wait summaries
every five seconds when flips continue and records per-flip CSV rows. It is not a
pixel readback, a new shader, or evidence of correct game frames. Profiling adds
CPU work and can affect timing. The CSV is buffered, with an explicit flush every
120 flips; cooperative `_Exit` can discard its final rows. An empty, absent or
short file does **not** prove that no flips occurred. Keep CSVs and logs private.

Look for continuing flip records versus continuing command activity, long frame
intervals, host/GPU wait counters and compilation work. If flips continue while
the first logo remains, investigate image/resource contents; if the trace stops,
investigate the submission/presentation waits. No result alone justifies skipping
a fence, replacing a shader result with zero, increasing the budget or enabling
DMA. The retained records establish some flip progress; they do not yet identify
the cause of the earlier first-logo observation. Establish the exit-phase failure
and confirm actual process termination before another renderer change.

## First session: inspect without GPU execution

Run commands from the repository root in a native macOS Terminal. Quote absolute
paths containing spaces. Keep stdout/stderr and generated reports private.

```sh
uname -m
# Must print arm64.
git status --short
git submodule status --recursive
bash packaging/test-arm64.sh --contracts-test
```

Read the newest `logs/graphics-game-*.log` by modification time, not an old filename
from chat. Compare its final lines with the preceding completed run. No new GPU
run is needed for this inspection. The combined game now defaults to no duration
limit; headless remains 30 seconds. `SOTC_DIAGNOSTIC_SECONDS` accepts 0..1800, with
zero disabling only the game-duration timers. Renderer-only initialization retains
its separate 60-second limit. A timeout is an observation boundary,
not successful gameplay or evidence that all imports are implemented.

The combined command rebuilds the host diagnostic, prepares a separate linked image,
checks Metal access and host symbol coverage, and launches the renderer. SDL/window
initialization stays on the host main thread; guest execution runs on a worker.
Each run has an isolated GPU configuration/cache directory under `work`.

## Reconstructing the environment

The full setup lives in [Building](guide/building.md) and [Game files](guide/game-files.md).
The main entry points are:

| Command | Purpose and limit |
|---|---|
| `bash packaging/test-arm64.sh --fetch` | Fetch exact pinned sources; do not run over non-Git snapshot directories |
| `bash packaging/test-arm64.sh --test` | Build CPU dependencies/archive and run native emitter, JIT and vector checks |
| `bash packaging/test-arm64.sh --contracts-test` | Synthetic runtime/ABI tests in both CPU modes; no game or real GPU required |
| `bash packaging/test-arm64.sh --prepare` | Prepare the owner's original executable privately |
| `bash packaging/test-arm64.sh --content-check` | Audit supplied archive manifests/extents; does not establish asset decoding |
| `bash packaging/test-arm64.sh --linked-test` | Paired headless initialization comparison; intentionally stops at unsupported imports |
| `bash packaging/build-gpu-arm64.sh` | Build/resume the pinned isolated renderer and dependencies |
| `bash packaging/test-arm64.sh --graphics-check` | Synthetic offscreen Metal draw/readback; no game frame |
| `bash packaging/test-arm64.sh --vulkan-check` | Check Vulkan surface extension discovery |
| `python3 tools/check_gpu_link.py` | Inspect actual host/library symbol coverage; does not validate ABI or load the library |
| `python3 tools/check_pipeline_validity.py` | CPU-only source-guard regression: original invalid acceptance versus adapted rejection |
| `bash packaging/test-arm64.sh --gpu-load-check` | Load the library without renderer initialization |
| `bash packaging/test-arm64.sh --renderer-check` | Initialize the renderer and diagnostic window without guest commands |
| `bash packaging/test-arm64.sh --graphics-game-test` | One supervised combined experiment; see the pending-submission capture above |

Important environment variables:

| Variable | Meaning |
|---|---|
| `BB_DEPS` | Optional compatible ARM64 static Zydis/Zycore prefix; default `work/deps-arm64` |
| `SOTC_GAME_ROOT` | Owner's readable game directory containing `eboot.bin` and `sce_module` |
| `SOTC_SYSTEM_MODULE_ROOT` | Original gesture/NGS2 modules; default sibling `sys_modules` directory |
| `SOTC_MEMORY_BACKING=file` | Unlinked temporary-file backing with shared aliases when POSIX shm is unavailable |
| `BB_JIT=1` / `BB_JIT=0` | ARM64 translation / interpreter |
| `SOTC_DIAGNOSTIC_SECONDS` | Accepts 0..1800; 0 means no game-duration limit and is the combined default; headless default 30; prerequisite budgets and GPU-progress cancellation stay active |
| `SOTC_GPU_SUBMIT_TRACE` | Combined diagnostic defaults to 1; 0 disables the bounded CPU submission journal and Metal callbacks; dumps on GPU watchdog |
| `BB_DIRECT_MEMORY_ACCESS` | Unset or literal 0 only; all other values rejected before GPU loading/prechecks |
| `BB_ASYNC_SUBMIT=0` | Current single comparison: submit on the calling thread; preserve recording workers |
| `BB_FRAME_STATS=1` | Opt-in host-side flip/wait statistics for the first-logo investigation; remove the variable for baseline timing |
| `BB_FRAME_LOG` | Optional CSV destination; with statistics on, the combined launcher defaults to private per-run `frames.csv`; final rows may be buffered |
| `BB_UPSCALE_PRESET=4` | Prepared low-resolution comparison: requests 640x360 host scene targets at default output; confirm actual reduced draws, native/guest memory does not automatically shrink |
| `SOTC_GPU_JOBS` | Isolated build parallelism, 1..8 |

The launch script explicitly selects its bundled Vulkan loader, MoltenVK ICD and
SDL Vulkan library. Preserve this: the Metal path still depends on Vulkan WSI.
`VK_KHR_surface` failures can come from selecting a different system loader/ICD.
The builder repairs generated FFmpeg/pkg-config metadata for prefixes with spaces.
Do not move the repository merely to conceal a quoting regression.

A restricted automation process can compile and run CPU contracts while exposing
no Metal device. Run only the authorized single comparison in native Terminal. A failed device
precheck in that process is not evidence that the user's GPU is unsupported.

## Code ownership and execution path

| Area | Files and responsibility |
|---|---|
| Build/test orchestration | `packaging/test-arm64.sh`; compiler flags, source lists, isolated modes, log capture |
| Input preparation | `tools/prepare_game.py`, `tools/prepare_linked_arm64.py`; private BBPROBE images and original module/TLS metadata |
| Host entry and stops | `runtime/src/probe_entry_arm64.c`; image mapping, relocations, initializer order, watchdog, import/fault reporting |
| Cooperative cancellation | `tools/run_gpu_diagnostic.py`, `runtime/src/diagnostic_control.h`; owned pipe, native child monitor, self-exit and parent status confirmation |
| Import policy | `runtime/src/bootstrap_contracts.c`; scoped namespace mappings and capability gates |
| CPU context and vector support | `bootstrap_cpu_context.c`, `bootstrap_cpu_vector.c`; pinned bbcpu adapters and explicit extensions |
| Threads/TLS/fibers | `bootstrap_threads.c`, `bootstrap_tls.c`, `bootstrap_keys.c`, `bootstrap_fibers.c` |
| Shared memory and address policy | `platform_arm64.c`, `runtime_memory_arm64.c`; real physical backing/aliases and low-address constraints |
| GPU notification storage | `tools/prepare_memory_support.py`, `runtime/src/memory_hook_queue.h`; checked private source adaptation with growable storage |
| Library/device binding | `runtime/src/bootstrap_gpu.c`; checked dlopen/dlsym, main-thread initialization and actual backend publication |
| Graphics import adapters | `runtime/src/bootstrap_graphics_routes.c`; reviewed encoders, live display validation, submit ABI and provider errors |
| Local services | Other `bootstrap_*.c` files; explicit narrow clocks/input/audio/service contracts |
| Renderer build | `tools/build_gpu.py`; pinned private source, checked upstream backports, dependencies and `work/gpu-build/libbbgpu.dylib` |
| Native GPU internals | Pinned `runtime/upstream/gpu` and its private build checkout; shaders, pipelines, buffers, textures, Metal/Vulkan |

Input preparation preserves original FS/TLS loads; do not reintroduce native x86 TLS
rewrites. The CPU executes supplied original libc/Fios2 and, in graphics mode,
gesture/NGS2 modules. Scoped host imports call reviewed adapters or an actual initialized
GPU provider. Unknown imports stop; missing services report explicit unavailability.

The host build copies the CPU archive and replaces two members with local context/
vector adaptations. Do not mutate the original archive. Mutex statistics and GPU
notification storage are transformed only in checked private build copies. The memory
preparation validates the complete pinned source digest and replaces just its storage
declaration and capacity guard. The source checkout remains unchanged.

Do not run two host build/test scripts concurrently: both regenerate the same private
archive and diagnostic sources in `work`.

## Prioritized debugging work

1. **Investigate the host watchdog incident without reproducing it.** Read the latest
   instrumented stall and earlier initialization-only/panic-associated logs together. Review renderer
   startup, shared guest-memory residency, queue progress and the pinned upstream
   sources. The cause is unresolved; do not assume DMA, memory pressure or the
   controller is responsible. The owner has resumed gradual testing; run one
   supervised comparison, inspect it, and do not replay the DMA-on incident.
   Scene entry and introduction video remain later goals; the owner has confirmed
   logos/menu and opening audio.
2. **MSL fragment interface support.** The preceding complete low-resolution log has 118 `PerVertexKHR` conversion
   failures. `python3 tools/check_fragment_interface.py` isolates this with the actual
   installed SPIRV-Cross library: an ordinary input converts, raw per-vertex input
   reading vertex one fails. Exit 2 means the known limitation is reproduced; it does
   not mean shader support passed. Neither case initializes a GPU. The Vulkan device
   advertises fragment barycentrics, but this alone does not prove the selected MSL
   conversion path supports raw vertex arrays. Trace `V_INTERP_MOV_F32` and
   `Qualifier::PerVertex` through `vector_interpolation.cpp` and
   `spirv_emit_context.cpp`. Do not remove the decoration or substitute interpolated
   input without preserving the requested vertex selection. The earlier duplicate
   auxiliary location error is absent; not every tessellation path is verified.
3. **SRT resource tracking.** Shader direct memory access is disabled and enabling it
   is blocked after the host incident. The earlier SRT/shader-info fixture checks
   uniform reads,
   Phi, per-lane and arithmetic offsets with DMA off/on: dynamic resources are
   selected only when the setting is enabled. This does not execute those accesses
   on a GPU or validate game resources. The existing walker still logs its static
   offset failures. Its result is offline compiler evidence, not hardware validation;
   do not rerun real-library experiments during incident containment or describe
   the messages as fixed. `flatten_extended_userdata_pass.cpp` fails offset
   computation on `Phi` and then fails its SRT walk. Investigate predecessor/value
   handling and resource provenance in the recompiler. Buffer-size clamps may relate
   to bad descriptors, but their causal link is not established. Do not treat clamps
   as a repair or replace unknown offsets with zero.
4. **Compressed texture usage.** BC1/BC5 format/usage errors request combinations
   including storage and extended views. Check capability detection and whether each
   image actually requires storage access; design a supported decode/view path if it
   does. Dropping usage flags without tracing consumers is not sufficient.
5. **Prove visible output.** Trace registered VideoOut buffers, actual submissions,
   completed GPU work and presentation, then inspect a real frame locally. Encoding
   commands, a window, accepted audio buffers or zero “unsupported commands” do not
   establish correct game pixels.

Renderer correctness remains open; distinguish the validated SPIR-V correction from
its pending visual verification. The float-denorm capability warning alone does not
identify the cause of the final stop. Do not silently toggle label/fence completion
policies to advance execution; completion must reflect actual GPU work.

## Resolving further imports in batches

For `STOP_AT_IMPORT`, retain the exact scoped name, thread, first argument and caller
context privately. Numeric import indices depend on the prepared image; do not use
them as API identity. Inspect the supplied module's symbol identity, pinned source
and original calling convention before binding a function. Review related calls as
a family to avoid one-import-per-run progress, but keep unknown contracts unresolved.

The original x86-64 guest passes its first six integer arguments in registers and
later arguments on its stack. The host ARM64 ABI differs. Public `bbcpu_call` takes
at most six arguments: for seven or more, construct a synthetic x86 thunk as in
`tests/test_graphics_submit.c` or `tests/test_graphics_draw.c`. Test 32-bit narrowing,
signed/full-width values, output layout, untouched failure buffers, namespace and
capability gates, errno and actual backend errors. Symbol presence alone is not ABI
compatibility.

Known compatibility repairs include compute packet header bits, indirect-draw stage
validation, colored-marker layout, submit pointer/size checks and flip-wait label
queries. Do not replace these adapters with raw native exports just because names
match. SaveData, trophy, AppContent and dialogs mostly expose explicit unavailable/
no-instance behavior; they are not working persistence/UI services.

Where useful, compare a synthetic fixture against the owner's original module in a
private local harness. Provide only the real dependencies or narrowly controlled test
inputs necessary for that comparison. Distinguish original-compatible valid behavior
from deliberate malformed-input safety guards. Publish the synthetic fixture and
findings, never the original binary, flattened module or generated guest code.

## Validation and shipping a change

For runtime changes, run the full contracts command. CPU context/vector changes also
require `--test`. For renderer changes, run the relevant synthetic regression and
rebuild the isolated GPU library before requesting a combined Terminal run. Preserve
these pins unless an explicit, reviewed dependency update is intended:

| Source | Revision |
|---|---|
| runtime | `3170535f59cadbe5ce9c38e6bb961ed5fd215194` |
| recomp | `f749c155c2ff3bb932795de85c9c8fe9c4aa59ad` |
| SotC reference | `2250f328feafab953aac85e758e7bddb2e80e891` |

The queue regression is `tests/test_memory_hooks.c`: provider replay and guest-ABI
release of 257 real aliases, 8,193 mixed FIFO events, callbacks outside the region
lock, reentry, reuse, count rollback, absent hooks and explicit allocation/capacity
failure. `tests/test_prepare_memory.py` checks the restricted transformation and
rejection of changed upstream input. The SSE4a fixture is `tests/test_cpu_sse4a.c`, run by both `--test` and
`--contracts-test`, including the documented QEMU-compatible truncation policy in all four forms.
The widening family is `tests/test_cpu_widen.c`, also run in both modes by those
commands. It exercises all existing width pairs as well as the new qword pairs.
The GPU-only auxiliary emitter regression is `python3 tools/check_tessellation.py`;
it needs the isolated build but neither a GPU device nor game data. Backport source
checks are part of `tests/test_gpu_build.py`. Other graphics regressions are split across
`test_graphics_{routes,color,shaders,submit,present,draw}.c`; synthetic dynamic-loader
coverage lives in `tests/test_graphics_entry.py`. RTC is `tests/test_rtc.c`.

Before committing, stage only intended public source/documentation and run:

```sh
git diff --cached --check
python3 tools/check_repository.py
git remote -v
```

The audit reads staged/tracked content. The Linux source-check CI is a partial static/
fixture gate; a green CI run does not replace native ARM64 contracts or GPU testing.
Record the exact command, CPU mode, build revision, first stop and verification limit
in the change description. Never promote game addresses, execution logs, generated
images or private paths into public documentation.

## Local state and handoff maintenance

`game/`, `work/` and `logs/` are ignored private state. They contain the owner's input,
generated executable images/module analysis, build products, caches and execution
logs. Fresh clones do not contain them. Recreate input from the owner's dump; do not
copy an unrelated developer's `work` directory or assume private oracle scripts ship
with this repository. Keep game content read-only and do not access saves.

Update this page when the observed stopping boundary or reproduction command changes.
Separate what was seen in Terminal, what synthetic tests establish, what is only a
hypothesis and what remains unverified. A successful next run should replace the
current timeout/renderer boundary above with its actual result, including any timeout.

## Preferred upstream reference

The user explicitly selected [bloodborne-mac](https://github.com/bloodborne-native/bloodborne-mac/)
as the reference when resolving development doubts. Start there and follow its
[runtime](https://github.com/bloodborne-native/bloodborne-runtime) and
[recomp](https://github.com/bloodborne-native/bloodborne-recomp) sources for the actual
implementation. Check relevant current commits against our pins before adapting code.
Keep source provenance and tests, and reject Bloodborne-specific object/layout
assumptions for SotC. This preference does not authorize unreviewed dependency bumps.
When the reference lacks an implementation, document another primary source explicitly;
the SSE4a overrun policy here follows
[QEMU v10.1.0](https://github.com/qemu/qemu/blob/v10.1.0/target/i386/ops_sse.h).
