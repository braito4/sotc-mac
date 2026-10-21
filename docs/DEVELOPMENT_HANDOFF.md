# Development and debugging handoff

Snapshot: 2026-10-21. This is the starting point for the next developer. Read
[AGENTS.md](../AGENTS.md), this page and [Build instructions](guide/building.md)
before changing code. [Status](STATUS.md) and [Graphics bring-up](GRAPHICS_BRINGUP.md)
contain chronological investigations; their earlier “latest run” sections describe
older boundaries, not the present state.

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

### Black scene and native submission evidence

**Current result:** the owner reports a black screen where the 3D animation should
appear. The latest native run uses the same low-resolution, DMA-off, async-off,
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
run is needed for this inspection. The combined game default is 120 seconds;
headless remains 30.
`SOTC_DIAGNOSTIC_SECONDS` accepts only 1..1800. Renderer-only initialization retains
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
| `SOTC_DIAGNOSTIC_SECONDS` | Accepts 1..1800; combined default 120, headless 30; not protection from a kernel hang |
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
2. **MSL fragment interface support.** The latest native log has 118 `PerVertexKHR` conversion
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
