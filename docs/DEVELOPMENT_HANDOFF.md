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
completed game run detects a DualSense. An earlier complete summary records 778 physical samples,
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

**Current result:** the latest run includes the output-backpressure correction and
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
has ended. Window closure alone is insufficient when the supervisor cannot reap
it. The next investigation needs the exit-phase process state/stack rather than
another renderer flag. For a future supervised comparison, the private log's PID
identifies the owned diagnostic in Activity Monitor. If it remains present after
the failure, use Sample Process for that specific diagnostic and keep the report
under ignored `work`. Record a sampling failure or an already-exited process as
such; do not request elevated access or substitute external termination signals.

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
latest acknowledged-but-unconfirmed stop and the CPU-only logging changes are
summarized above. After confirming the previous process has ended and preparing
the exit-phase investigation, a future single native Terminal comparison keeps that
instrumentation, the same 30-second budget, DMA off, asynchronous submission off
and JIT on:

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
| `bash packaging/test-arm64.sh --gpu-load-check` | Load the library without renderer initialization |
| `bash packaging/test-arm64.sh --renderer-check` | Initialize the renderer and diagnostic window without guest commands |
| `bash packaging/test-arm64.sh --graphics-game-test` | One supervised combined experiment; see the current 30-second comparison above |

Important environment variables:

| Variable | Meaning |
|---|---|
| `BB_DEPS` | Optional compatible ARM64 static Zydis/Zycore prefix; default `work/deps-arm64` |
| `SOTC_GAME_ROOT` | Owner's readable game directory containing `eboot.bin` and `sce_module` |
| `SOTC_SYSTEM_MODULE_ROOT` | Original gesture/NGS2 modules; default sibling `sys_modules` directory |
| `SOTC_MEMORY_BACKING=file` | Unlinked temporary-file backing with shared aliases when POSIX shm is unavailable |
| `BB_JIT=1` / `BB_JIT=0` | ARM64 translation / interpreter |
| `SOTC_DIAGNOSTIC_SECONDS` | Accepts 1..1800; combined default 120, headless 30; not protection from a kernel hang |
| `BB_DIRECT_MEMORY_ACCESS` | Unset or literal 0 only; all other values rejected before GPU loading/prechecks |
| `BB_ASYNC_SUBMIT=0` | Current single comparison: submit on the calling thread; preserve recording workers |
| `BB_FRAME_STATS=1` | Opt-in host-side flip/wait statistics for the first-logo investigation; remove the variable for baseline timing |
| `BB_FRAME_LOG` | Optional CSV destination; with statistics on, the combined launcher defaults to private per-run `frames.csv`; final rows may be buffered |
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
   initialization-only log and the earlier GPU-stall log together. Review renderer
   startup, shared guest-memory residency, queue progress and the pinned upstream
   sources. The cause is unresolved; do not assume DMA, memory pressure or the
   controller is responsible. The owner has resumed gradual testing; run one
   supervised comparison, inspect it, and do not replay the DMA-on incident.
   Scene entry and introduction video remain later goals; the owner has confirmed
   logos/menu and opening audio.
2. **MSL fragment interface support.** An earlier game log has 78 `PerVertexKHR` conversion
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
