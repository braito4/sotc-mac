<h1 align="center">sotc-mac</h1>

<p align="center">
  <strong>Shadow of the Colossus for Apple Silicon.</strong><br>
  An experimental macOS ARM64 port of the PS4 game, using native CPU translation. No Rosetta 2.
</p>

<p align="center">
  <sub>Built on <a href="https://github.com/bloodborne-native/bloodborne-mac"><strong>bloodborne-native</strong></a>, <a href="https://github.com/deadinside28/bloodborne_pc"><strong>bbport</strong></a> by deadinside28, and <a href="https://github.com/shadps4-emu/shadPS4">shadPS4</a>. SotC reference: <a href="https://github.com/AjncSystem/ShadowOfTheColossus-PC">AjncSystem/ShadowOfTheColossus-PC</a>.</sub>
</p>

**The game reaches the first gameplay image on Apple M1, but it is not playable yet.**
The owner confirms this milestone on 2026-10-22. Rendering defects, memory failures
and instability remain; correct graphics, sustained gameplay, audiovisual
synchronization and save persistence are not validated.

Latest owner capture (2026-10-23, 13:39:40 UTC) stops after 126.462 seconds
on an invalid allocator link. Retained observations narrow the same-field value
change to 404.567 ms; the writer and allocation lifetime remain unresolved.
The follow-up prioritizes two allocator-link watches while retaining the earlier
failure-family observations. See the first [handoff checkpoint](docs/DEVELOPMENT_HANDOFF.md).

Our most productive approach so far is the
[bounded evidence and synthetic-control workflow](docs/MEMORY_ERROR_WORKFLOW.md).
It is the recommended method for this project's intermittent memory failures;
it does not guarantee that a specific run captures every writer or fixes corruption.

The owner diagnostic also enables a bounded CPU coroutine-frame quarantine:
freed frames become inaccessible temporarily, with a separate lifecycle journal.
Persistent object-field and referring-table capture remains active. Synthetic
controls catch stale reads and preserve ordered consumers; the native corruption
cause and correct guest object lifetime remain unresolved.

The latest owner capture (October 23, 2026, 13:03:52 Madrid) stops after
86.457 seconds on an invalid link read in another tree lookup. The saved terminal
registers match the fault text; 3072 CPU and 1024 shader observations survive.
The offending link is visible in a readable header, but its writer/lifetime
remains unknown and the current probe tails contain no matching source event.
See the first [handoff checkpoint](docs/DEVELOPMENT_HANDOFF.md#continuation-checkpoint).
Native corruption remains unresolved.

The next prepared capture targets the newly localized pointer source. History
now emits recent records first when output fills. A separate reproduced SIGPIPE
bug in diagnostic shutdown is fixed and passes 100 closed-output controls; the
game's invalid pointer still has no proven writer or correction.

Fatal CPU reporting now attempts a compact register/thread/history minimum before
rich diagnostics, including nested faults, so a second reporter has a chance to
retain evidence before exiting. The combined report explicitly flags truncation.
Synthetic checks pass; actual pointer/shader/freeze causes remain unresolved.

The prepared follow-up retains a bounded snapshot of the already owned shader
bytes when decoding fails, records the specific rejection in the combined report,
and flags long presentation gaps even if window closure returns zero. The next
capture uses a smaller private CPU selection. ARM64 and synthetic checks pass;
the actual unknown-format and freeze causes remain unresolved. See the first
[handoff checkpoint](docs/DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

Earlier owner captures follow; their dates identify historical boundaries.

The latest owner run (October 23, 2026, 10:11:06 Madrid) stops after **165.141
seconds** on required fragment-shader metadata lookup, exits 23/23. It records
1,588 API presentations, 462 pipeline failures and 805 SRT errors, with no
recorded OOM/large copies. The direct stop bypasses CPU history, so the active
tree selection has no emitted evidence; missing history is not an empty ring.
The last native presentation sample is 10.507 GiB, not failure-time. Invalid
program origin/lifetime/visibility versus lookup limits remain unresolved;
prior CPU faults and GPU stall are not proven fixed. See the first
[handoff checkpoint](docs/DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

The prepared follow-up retains CPU history on required shader-metadata and
Metal-command stops, preserves the real failure status, and saves the complete
bounded metadata scan slice. Each run now produces a private combined diagnostic
report with separate recent failures and explicit missing evidence. ARM64 and
synthetic checks pass; the actual shader/pointer causes remain unresolved.

The preceding owner run (October 23, 2026, 09:57:24 Madrid) stops after **160.533
seconds** on a translated CPU `SIGSEGV` in linked-node key comparison, through
an already invalid argument. It differs from the earlier array fault. Fatal
history is now exercised, but the array-focused selection covers only 4.366 ms
and omits the new root/link source. There are 1,470 API presentations, 444 pipeline
failures and 805 SRT errors, with no recorded OOM/large copies. The last native
presentation sample is 9.905 GiB, not a fault-time measurement. The larger image
journal has zero drops in its last periodic summary; this improves evidence,
not game memory use. Writer/lifetime/CPU and previous GPU-stall causes remain
open. See the first [handoff checkpoint](docs/DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

The next prepared capture freezes newly entering history observations before
fatal reporting and targets the container root/node links instead of the broad
array consumer. ARM64 and synthetic contracts pass; the invalid pointer's origin
and correction still require native evidence. See the first handoff checkpoint.

The preceding owner run (October 23, 2026, 09:39:10 Madrid) stops after **159.147
seconds** on GPU progress: two awaited Metal command chunks are scheduled but
not completed. Cooperative child/supervisor exits 21/31 are confirmed. There
are 1,336 API presentations, 477 pipeline failures and 805 SRT errors; no Metal
OOM or large copies. A stall-time native probe samples **10.023 GiB**. CPU note
candidates do not identify the precise stalled shader. The array trace is active,
but cooperative stopping bypasses its fatal history dump, so pointer-origin
evidence remains missing. Prior CPU/shader failures remain unresolved.
See the first [handoff checkpoint](docs/DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

The prepared diagnostic now attempts 128 CPU history records during cooperative
stopping and retains 128 guest GPU draw/dispatch candidates separately from state
notes. It distinguishes missing/partial dumps and preserves exit status with a
closed output reader. The image-event file cap is 32 MiB; this improves evidence,
not game memory use. ARM64, full synthetic contracts and all 22 renderer CPU
checks pass. The actual hang and pointer/shader causes remain unresolved; native
validation is pending the next owner run.

The checked follow-up rejects a sample count absent from the supported device
mask and records count/mask/caller role on failure. Renderer assertions also
retain the existing bounded CPU history. Original defect controls, 1,032 valid
conversions, 248 mask rejections, ARM64/all 22 renderer CPU checks and full
JIT/interpreter contracts pass. Native crash resolution remains unverified.
Vulkan-Headers **1.4.366**, loader 1.4.365, base pins and cache 28/9 stay fixed.
Earlier captures and preparation milestones follow.

**Development has resumed.** The October 22, 2026 owner run confirms that the
balanced guest-memory import layout avoids the large staging snapshot: zero
recorded large copies, with the last Metal allocation sample at **8.917 GiB**
versus **11.697 GiB** at the preceding run's memory failure. Different workload
and stopping boundaries prevent treating that difference as a controlled benchmark.

The first balanced-layout run ends after **82.902 seconds** on an unmapped host
instruction `SIGSEGV`, outside translated code (exit 139). It records 905
presentations, 306 pipeline-creation failures and 271 SRT errors. The port is
still experimental: the memory mechanism is exercised, but sustained stability,
correct pixels and OOM avoidance remain unproven.

The newest comparison (October 23, 2026, 07:46:41 Madrid) ends after **4.033
seconds** on Metal insufficient memory, at **11.905 GiB** actual failure-time
allocation. A binding slightly larger than 3 GiB misses complete import-window
coverage and reserves **3.5 GiB** staging; its copy completion is not recorded.
There are 20 presentations, zero logged pipeline failures and 50 SRT errors.
The early stop does not establish a shader fix. The two preceding morning runs
also hit OOM with large snapshots; the earlier translated CPU SIGSEGV remains
unresolved. Current executable/renderer UUIDs match the prepared binaries. Game
Mode integration is removed and startup remains windowed. See the first handoff.

The prepared **balanced-v2** layout moves the middle import origin to 1.75 GiB.
It avoids the three morning captures' large fallback requests in CPU replay,
without adding imports or imported bytes for this pool. Across 10,390 retained
observations it covers 28 ranges missed by v1, with no observed direct-hit loss.
ARM64, sanitizer and synthetic contracts pass; restoring logos and reducing native
memory pressure still require an owner-run comparison. The exact previous layout
remains available as `BB_POOL_LAYOUT=balanced-v1`; see the handoff for tradeoffs.

Prepared follow-up repairs guest libc write notifications, validates portable
SRT bytecode against its fixed stacks, and fixes full-width bitfield extraction.
Original defect controls and ARM64/synthetic tests pass; cache is now **28/9**.
These changes do not establish the cause of the native SIGSEGV or resolve the
remaining dynamic SRT/PerVertex failures. No game run validates them yet.

A new bounded fatal diagnostic records ARM64 registers, including the link
register, and a startup binary UUID/text-range inventory for offline analysis.
It does not recover from invalid execution or claim to fix its cause. CPU-only
controls pass; startup image inventory is exercised in the owner's latest run,
and native fatal register context is exercised by the latest fault. Raw
captures, addresses, shaders and game assets remain private. See the
[current status](docs/STATUS.md), [developer handoff](docs/DEVELOPMENT_HANDOFF.md)
and [pause/resumption record](docs/DEVELOPMENT_PAUSE.md). Earlier investigations
below are historical.

An earlier native run stops on a translated CPU SIGSEGV after
118.281 seconds, with confirmed exits 139 and 11.646 GiB sampled native peak.
The eight-command admission cap is exercised, but the preceding capped run
still ends on Metal OOM. An opt-in protected fatal field reader is prepared to
trace the invalid pointer's source; its cause and correction remain unresolved.
No additional game run validates that diagnostic preparation.

An earlier native run stops on Metal insufficient memory after
100.149 seconds, with confirmed child/supervisor exit 23 and 12.434 GiB native
allocation at failure. Earlier shader-decoder assertions, CPU faults and GPU
stalls remain unresolved. This M1 has 16 GB of unified memory; CPU and GPU share
it. Scene work at 640x360 does not shrink every output, original-size resource
or large upload. A prepared native admission limit bounds unfinished command
batches; native memory savings and performance remain unverified.
A checked decoder correction rejects reads beyond supplied shader slices and
adds private failure provenance. ARM64 and synthetic contracts pass; the cause
of the game's unsupported opcode remains unverified.
A correction for duplicated
read-only upload snapshots is built and CPU-tested; the latest captures do not
exercise the duplicate case, so native memory savings remain unverified. Earlier GPU
stalls, CPU allocation-metadata corruption and an audio queue stop remain unresolved.
A further prepared staging improvement uses disjoint unused buffer tails for
aligned uploads in one recording tick. CPU fixtures and the ARM64 build pass;
native memory savings and OOM avoidance remain unverified.
Native Metal batches also request retained resource references at initial
creation and after splitting a batch. This strengthens explicit-resource
lifetime through Metal completion; native stability and memory effects remain
unverified.
The latest selective upstream adaptation preserves Metal tessellation instance
IDs and handles multiple instances; GPU correctness remains unverified. The
[upstream review](docs/UPSTREAM_REVIEW.md) records all five owner-requested
references and a new nested-coroutine ownership warning to investigate.
See the [current status](docs/STATUS.md) and
[developer handoff](docs/DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

The port translates PS4 x86-64 instructions to ARM64 at runtime without Rosetta;
it is not a source rebuild of the game's engine. There is no downloadable game app.

<p align="center">
  <a href="docs/guide/installation.md"><strong>Getting started</strong></a>
  · <a href="docs/README.md">Documentation</a>
  · <a href="docs/DEVELOPMENT_HANDOFF.md">Developer handoff</a>
  · <a href="docs/STATUS.md">Development status</a>
  · <a href="docs/DEPENDENCIES.md">Dependency versions</a>
  · <a href="CONTRIBUTING.md">Contributing</a>
</p>

> [!IMPORTANT]
> **No game files are included.** Game-based diagnostics require your own readable dump
> of **Shadow of the Colossus, CUSA08809, update 1.01**. Other editions are untested.
> The synthetic CPU tests do not need a game. See [Game files](docs/guide/game-files.md).

## Features

The current [dependency inventory](docs/DEPENDENCIES.md) records updated runtime,
CPU and SotC reference pins, stable library versions and exact source revisions.
The ARM64 build and CPU/source checks pass. Ordered frame maintenance now
destroys stale proxies in native owner comparisons. The latest run ends on
Metal OOM; memory stability remains unresolved.

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

The owner confirms the first image of the gameplay section on Apple M1.
**Reaching gameplay does not yet make this a playable port.** Rendering defects
and crashes remain, and no stable gameplay performance or working save support
is claimed.

The latest owner comparison ends on **Metal OOM 8 after 104.474 seconds**, with
confirmed child/supervisor exit **23** and **11.588 GiB native allocation**.
It records 983 presentation results and reduced-scene rendering activity; these
are progress measurements, not correct-pixel or gameplay validation. One large
fallback copy allocates 3 GiB. The previous comparison stopped much earlier,
with one copy rounded to 3.5 GiB. The existing rounding rule is unchanged.
A correction for adjacent duplicate read-only snapshots passes actual-source CPU
fixtures, the ARM64 build and contracts, but neither of these two runs exercises
that large duplicate case. Native savings remain unverified and OOM persists.
Earlier GPU stalls, guest allocator corruption and a stopped audio queue remain
unresolved.

The renderer adds typed integer image blits, addressing the float-output
mismatch reported for R32Uint and RG32Uint. ARM64 compilation, actual-source CPU
regressions and JIT/interpreter contracts pass. Those two errors do not recur in
the native comparison, but exact GPU copy pixels and all supported cases still
need validation. PerVertexKHR failures persist. This is not an OOM or corruption fix.

Dependencies remain unchanged; shader cache is now 28/9 after the SRT/bitfield and defined SetPC
arithmetic and CFG target-validation correction described in the handoff.
The current launcher uses original scene resolution, a windowed default,
DMA/async off and metadata diagnostics on. Native comparisons remain
one brief supervised run at a time; see the
[developer handoff](docs/DEVELOPMENT_HANDOFF.md#continuation-checkpoint) for the
procedure, known limits and historical evidence.

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
