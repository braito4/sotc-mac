<h1 align="center">sotc-mac</h1>

<p align="center">
  <strong>Shadow of the Colossus for Apple Silicon.</strong><br>
  An experimental macOS ARM64 port of the PS4 game, using native CPU translation. No Rosetta 2.
</p>

<p align="center">
  <sub>Built on <a href="https://github.com/bloodborne-native/bloodborne-mac"><strong>bloodborne-native</strong></a>, <a href="https://github.com/deadinside28/bloodborne_pc"><strong>bbport</strong></a> by deadinside28, and <a href="https://github.com/shadps4-emu/shadPS4">shadPS4</a>. SotC reference: <a href="https://github.com/AjncSystem/ShadowOfTheColossus-PC">AjncSystem/ShadowOfTheColossus-PC</a>.</sub>
</p>

> [!IMPORTANT]
> **Early development: game graphics on Apple M1; gameplay is unverified.**
>
> Earlier runs showed logos/menu and defective 3D animation, with much improved
> audio and measured input delivery close to 48 kHz. The latest owner run has black
> output despite working input, then stops on confirmed native Metal memory
> exhaustion after about 161 seconds. An unexercised buffer-bound experiment is
> withdrawn for comparison with the previous renderer; restored visible output
> remains unverified. The audio and staging-reuse corrections remain. The error
> handler exits with confirmed status. See the
> [continuation checkpoint](docs/DEVELOPMENT_HANDOFF.md#continuation-checkpoint).
> The diagnostic reaches the main loop with game GPU commands and a physical controller.
> Introduction video correctness, entering a playable scene,
> save persistence and gameplay stability remain unverified. There is no downloadable game app or gameplay performance result.
>
> The goal is to run Shadow of the Colossus on Apple Silicon without Rosetta. The current
> method translates PS4 x86-64 instructions to ARM64 at runtime; it is not a source rebuild
> of the game's engine.

> [!WARNING]
> **GPU testing is proceeding gradually after a reported macOS watchdog kernel panic.**
> The owner authorized one brief supervised experiment at a time. Dynamic shader
> reads remain disabled and enabling them is blocked before GPU loading. The latest
> reviewed capture stops on a confirmed native memory error; earlier captures
> stopped on the GPU watchdog, with both confirmed and unconfirmed cooperative exits.
> At the owner's request, game-duration limits now default to disabled; GPU-progress
> supervision, manual cancellation and prerequisite budgets remain active.
> Integer-format image-copy errors remain, and lower total memory pressure has
> not been demonstrated.
> Low-resolution comparisons are authorized, one at a time; see
> the [scene-preset comparison](docs/DEVELOPMENT_HANDOFF.md#low-resolution-scene-comparison).
> Lower pixel counts do not guarantee lower total memory pressure or host stability.
> See the
> [current test](docs/DEVELOPMENT_HANDOFF.md#gradual-gpu-testing).

<p align="center">
  <a href="docs/guide/installation.md"><strong>Getting started</strong></a>
  · <a href="docs/README.md">Documentation</a>
  · <a href="docs/DEVELOPMENT_HANDOFF.md">Developer handoff</a>
  · <a href="docs/STATUS.md">Development status</a>
  · <a href="CONTRIBUTING.md">Contributing</a>
</p>

> [!IMPORTANT]
> **No game files are included.** Game-based diagnostics require your own readable dump
> of **Shadow of the Colossus, CUSA08809, update 1.01**. Other editions are untested.
> The synthetic CPU tests do not need a game. See [Game files](docs/guide/game-files.md).

## Features

- **Native Apple Silicon host:** the diagnostic and bbcpu are built as ARM64 binaries.
- **JIT and interpreter checks:** both modes are compared at explicit initialization boundaries.
- **String-mask CPU extension:** PCMPISTRM/VPCMPISTRM comparisons and flags are checked
  against a scalar model in both modes, including SSE/VEX register behavior.
- **Original system modules:** libc and Fios2 are linked from the user's own game dump.
- **Initial runtime support:** mutexes, read/write locks, destructor callbacks, clocks, timezone conversion,
  console output and direct/flexible memory queries.
- **Event flags:** native FIFO synchronization with AND/OR conditions, clear modes,
  cancellation, deletion and relative timeouts; tested through the guest ABI in both modes.
- **User-event queues:** native level/edge notifications, polling, timed/blocking waits and
  safe deletion with blocked consumers; no synthetic GPU notifications.
- **Read-only content access:** a narrow file API tested with synthetic fixtures; no save mount.
- **SDL3 input:** physical gamepad buttons, sticks, triggers, touch, LED/rumble forwarding,
  hotplug and keyboard fallback; guest ABI and error contracts tested in both CPU modes.
- **Guest fibers:** suspension, resumption and switching preserve guest CPU state and stacks.
  Synthetic checks also cover resuming on another host thread.
- **Guest threads:** native pthread workers with separate guest stacks, static/module TLS,
  DTV tables and errno. Thread-specific keys include checked POSIX/kernel APIs and guest
  destructors with bounded rearming. Synthetic checks cover return, exit, join, detach and condition waits.
- **Local user profile:** an ephemeral console user with checked initialization, termination,
  ID/list/name queries and a single login event; no account or user storage is accessed.
- **Local-content DiscMap profile:** unloaded mapping tables produce explicit errors without
  inventing disc locations. Request queries validate ranges and preserve the 64-bit output ABI.
  Internal module loading has checked argument and reference handling.
- **Local diagnostics:** generated game-derived images and execution logs remain outside Git.

## Getting started

1. Use an Apple Silicon Mac with Xcode Command Line Tools, CMake, Ninja and Python 3.
2. Initialize the pinned sources and build the CPU library using the
   [build guide](docs/guide/building.md).
3. Run the synthetic tests without game files:

   ```sh
   bash packaging/test-arm64.sh --contracts-test
   ```

4. For the optional initialization probe, prepare your own game files and follow
   [Game files](docs/guide/game-files.md).

The [installation guide](docs/guide/installation.md) explains the current developer-only workflow.

## Status

The [continuation checkpoint](docs/DEVELOPMENT_HANDOFF.md#continuation-checkpoint)
supersedes the historical run summaries below. The latest owner run has black
output despite working input and stops on confirmed native Metal memory exhaustion
after 160.610 seconds, with confirmed child/supervisor exit 23. The experimental
shader-derived buffer bound records no applied reductions and is withdrawn,
including its metadata/cache changes. The prior renderer overlays and cache 23/9
are rebuilt for ARM64; source regressions and full CPU contracts pass. Visible
output still needs an owner comparison. The scene
setting remains 640x360, DMA/async off. General PerVertexKHR, dynamic SRT and
buffer-clamp failures persist. The resource exhausting memory, correct rendering
and gameplay stability remain unverified.
The owner reports audio much improved: native delivery rises from about 23,912 to
47,684 input frames/s per port after sizing the queue to the actual device quantum.
A staging-cache correction now exercises four reuses previously rejected by size
class, with 512 MiB less dedicated upload inventory. The run lasts 137.642 seconds
versus 59.026 previously, but image allocations grow and Metal still runs out of
memory. Residual audio gaps, the large upload allocation and image lifetimes need
further investigation; this is not a gameplay performance or stability result.

**Experimental: earlier runs reached the logos/menu on Apple M1; the last
uninstrumented 30-second comparison showed only the first logo.**
The owner also previously confirmed hearing the opening of the game's introduction. This
establishes audible game output, without proving full audio coverage or audiovisual
synchronization. An earlier native run reaches the main loop at about 1.38 seconds,
detects a DualSense and continues beyond the save-dialog boundary. It ends at the
60-second diagnostic timeout, with no unresolved-import stop or CPU execution fault
recorded during that interval. Draw/dispatch counters are not gameplay FPS.
The POSIX `usleep` import from an earlier instrumented run is corrected. The preceding
complete low-resolution run detects a DualSense and stops on the renderer's GPU watchdog
after about 114.91 seconds, with no import stop or CPU-fault report. The child exits
with status 21; supervisor status 31 preserves the GPU-stall reason. Its CSV retains
438 complete flip rows; cancellation can discard buffered rows. A record
of presentation calls does not establish correct image contents or gameplay performance.

An earlier capture omits the low-resolution preset and uses a 30-second budget. It
stops on the GPU watchdog after about 19.49 seconds; the supervisor observes the
cancellation notice but cannot confirm child exit. A later targeted check finds
the process absent. Its pending batch includes compute, transfers and one draw in
the retained tail, and its CSV is empty. This is a different configuration and
earlier stopping boundary, not evidence that the later scene-rendering errors passed.

The bounded journal now identifies indexed indirect draw candidates in the awaited
batch, but not the precise executing command. A checked correction rejects invalid
cached pipelines and null Metal registration; black-scene shader support remains
unresolved. See
[pending-submission tracing](docs/DEVELOPMENT_HANDOFF.md#pending-submission-tracing).

Save-dialog initialization/open error contracts complement the eight mount/cleanup/
metadata failure paths. Full synthetic contracts pass in both CPU modes; private
comparisons check original validation and injected dependency-error propagation.
No save storage or completed UI interaction is created. Following the reported host
panic, dynamic shader-memory reads are disabled and nonzero overrides are rejected.
The combined default now has no game-duration limit at the owner's request;
`SOTC_DIAGNOSTIC_SECONDS=0` disables both duration timers, while 1..1800 selects
a bounded run. GPU-progress cancellation, DMA protection and prerequisite budgets
remain active. These cannot protect against a kernel/driver hang. The owner has authorized gradual
supervised GPU testing. See the
[current incident and handoff](docs/DEVELOPMENT_HANDOFF.md#host-watchdog-incident).

The renderer still reports PerVertexKHR-to-MSL conversion failures, SRT/Phi offset
errors and buffer-size clamps. A matching SotC issue and exact MoltenVK reports are
linked in the [upstream review](docs/UPSTREAM_REVIEW.md). Correct in-game scenes and
stability remain unverified. The current reproduction steps and stopping boundary
are in the [developer handoff](docs/DEVELOPMENT_HANDOFF.md).

See [Development status](docs/STATUS.md) for the measured scope and next checks.

## Additional diagnostics

[Visual diagnostics](docs/VISUAL_DIAGNOSTICS.md) now record private presentation,
draw-skip and allocation evidence, with optional bounded game-frame previews and
an offline HTML report. Scene rendering stays 640x360; capture defaults off and
optional readback remains untested on the native GPU. The
[PerVertexKHR review](docs/PERVERTEX_REVIEW.md) documents experimental upstream
proposals and an isolated CPU conversion test. The
[dependency watch](docs/DEPENDENCY_WATCH.md) records continuing upstream review and
the adapted fix for oversized buffer-copy recording.
[Audio delivery diagnostics](docs/AUDIO_DIAGNOSTICS.md) record timing/queue evidence
without PCM. The owner and native delivery measurements confirm substantial audio
improvement; residual gaps and audiovisual alignment remain open.

```sh
bash packaging/test-arm64.sh --graphics-check
bash packaging/test-arm64.sh --content-check
SOTC_MEMORY_BACKING=file bash packaging/test-arm64.sh --linked-test
```

The content audit checks archive extents and name manifests without extracting assets.
File backing avoids named-shared-memory restrictions without changing shared aliases or
mapping bounds. See [Reference review and applied checks](docs/REFERENCE_REVIEW.md).

## Documentation

| | |
|---|---|
| [Installation](docs/guide/installation.md) | Developer setup and available test modes |
| [Game files](docs/guide/game-files.md) | Supported input, private output and read-only mounts |
| [Building from source](docs/guide/building.md) | Pinned dependencies and ARM64 builds |
| [How it works](docs/guide/how-it-works.md) | CPU translation, module linking and fibers |
| [Performance](docs/guide/performance.md) | What has been measured and what remains untested |
| [Troubleshooting](docs/guide/troubleshooting.md) | Expected stops, memory restrictions and failures |

## Under the hood

| Component | Role |
|---|---|
| **sotc-mac** (this repository) | SotC adapters, CPU diagnostics, synthetic tests and documentation |
| `runtime/src/` | Scoped PS4 runtime adapters and the diagnostic loader |
| [bloodborne-runtime](https://github.com/bloodborne-native/bloodborne-runtime), `runtime/upstream/` | Pinned platform, memory, mutex, thread and file backends |
| [bloodborne-recomp](https://github.com/bloodborne-native/bloodborne-recomp), `runtime/upstream/recomp/` | bbcpu: x86-64 decoder, interpreter and ARM64 translator |
| [ShadowOfTheColossus-PC](https://github.com/AjncSystem/ShadowOfTheColossus-PC), `reference/` | Pinned SotC input preparation and runtime reference |
| `tools/`, `tests/`, `packaging/` | Private module preparation, synthetic fixtures and build/test entry points |

Upstream sources are separate dependencies. This project adds adapters around them; it does
not rename Bloodborne's launcher or claim that its renderer already supports SotC.

## Credits

This work depends on **bloodborne-native**, on **bbport** by **deadinside28**, on **shadPS4**,
and on **AjncSystem's ShadowOfTheColossus-PC**. Their work provides the CPU translator,
runtime foundations and SotC reference implementation. The README layout follows
[bloodborne-mac](https://github.com/bloodborne-native/bloodborne-mac), adapted to this project's
actual development stage. Full attribution: [Credits and licenses](docs/CREDITS.md).

Licensed under the [GNU GPL v2 or later](LICENSE).

<sub>Independent project. Not affiliated with Sony Interactive Entertainment, Bluepoint Games,
Team Ico, bloodborne-native or shadPS4. Report SotC adapter issues to this project.</sub>
