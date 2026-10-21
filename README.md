<h1 align="center">sotc-mac</h1>

<p align="center">
  <strong>Shadow of the Colossus for Apple Silicon.</strong><br>
  An experimental macOS ARM64 port of the PS4 game, using native CPU translation. No Rosetta 2.
</p>

<p align="center">
  <sub>Built on <a href="https://github.com/bloodborne-native/bloodborne-mac"><strong>bloodborne-native</strong></a>, <a href="https://github.com/deadinside28/bloodborne_pc"><strong>bbport</strong></a> by deadinside28, and <a href="https://github.com/shadps4-emu/shadPS4">shadPS4</a>. SotC reference: <a href="https://github.com/AjncSystem/ShadowOfTheColossus-PC">AjncSystem/ShadowOfTheColossus-PC</a>.</sub>
</p>

> [!IMPORTANT]
> **Early development: game logos and the menu are visible on Apple M1. Gameplay is unverified.**
>
> The owner has visually confirmed the logos and menu in a native Terminal run.
> The diagnostic reaches the main loop with game GPU commands and a physical controller.
> Entering a playable 3D scene, audible output, save persistence and gameplay stability
> remain unverified. There is no downloadable game app or gameplay performance result.
>
> The goal is to run Shadow of the Colossus on Apple Silicon without Rosetta. The current
> method translates PS4 x86-64 instructions to ARM64 at runtime; it is not a source rebuild
> of the game's engine.

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

**Experimental: logos and menu visually confirmed by the owner on Apple M1.**
The latest native Terminal log reaches the main loop in about 1.48 seconds and detects
a DualSense controller. It records 1,018 physical input samples and accepted SDL LED/
rumble requests; the actual output effect and connection transport are unverified.
Game draw/dispatch activity is present. These counters are not gameplay FPS.

The observed run stops at `sceSaveDataMount2` when saving the user profile. Eight
related mount, cleanup and metadata calls now return the measured NOT_INITIALIZED
error, preserving outputs and avoiding save access. Full synthetic contracts and
original-module failure-path comparisons pass in both CPU modes. A new native Terminal
run must establish the game's response; this change does not implement persistence.

The renderer still reports PerVertexKHR-to-MSL conversion failures, SRT/Phi offset
errors and buffer-size clamps. A matching SotC issue and exact MoltenVK reports are
linked in the [upstream review](docs/UPSTREAM_REVIEW.md). Correct in-game scenes and
stability remain unverified. The current reproduction steps and stopping boundary
are in the [developer handoff](docs/DEVELOPMENT_HANDOFF.md).

See [Development status](docs/STATUS.md) for the measured scope and next checks.

## Additional diagnostics

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
