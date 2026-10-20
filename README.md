<h1 align="center">sotc-mac</h1>

<p align="center">
  <strong>Shadow of the Colossus for Apple Silicon.</strong><br>
  An experimental macOS ARM64 port of the PS4 game, using native CPU translation. No Rosetta 2.
</p>

<p align="center">
  <sub>Built on <a href="https://github.com/bloodborne-native/bloodborne-mac"><strong>bloodborne-native</strong></a>, <a href="https://github.com/deadinside28/bloodborne_pc"><strong>bbport</strong></a> by deadinside28, and <a href="https://github.com/shadps4-emu/shadPS4">shadPS4</a>. SotC reference: <a href="https://github.com/AjncSystem/ShadowOfTheColossus-PC">AjncSystem/ShadowOfTheColossus-PC</a>.</sub>
</p>

> [!IMPORTANT]
> **Early development: CPU diagnostics only. The game is not playable.**
>
> The original executable reaches part of its initialization through the ARM64 translator
> and through its interpreter. The original libc and Fios2 modules initialize successfully.
> This project does not yet provide a renderer, audio, a playable launcher or save support.
> There is no downloadable game app or gameplay performance result.
>
> The goal is to run Shadow of the Colossus on Apple Silicon without Rosetta. The current
> method translates PS4 x86-64 instructions to ARM64 at runtime; it is not a source rebuild
> of the game's engine.

<p align="center">
  <a href="docs/guide/installation.md"><strong>Getting started</strong></a>
  · <a href="docs/README.md">Documentation</a>
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
- **Read-only content access:** a narrow file API tested with synthetic fixtures; no save mount.
- **Guest fibers:** suspension, resumption and switching preserve guest CPU state and stacks.
  Synthetic checks also cover resuming on another host thread.
- **Guest threads:** native pthread workers with separate guest stacks, static/module TLS,
  DTV tables and errno. Synthetic checks cover return, exit, join, detach and condition waits.
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

**Experimental, not playable.** In the latest paired normal Terminal run, JIT and interpreter
both complete the original module initializers, create Fios2's three workers, four game job
workers and a resource-loading worker. Both modes create 19 event flags and 160 fibers,
record 164 fiber runs. The interpreter records 196 direct fiber switches and 86 reads
(80,193,830 bytes); JIT records 445 switches and 198 reads (80,742,980 bytes). Both open
17 content files without writes, pass three owner-registration and seven compute-queue
failures, and query the logical safe area once. Both then reach the same unresolved
VideoOut color-application call after one failed VideoOut open and one gamma preparation
per mode. The concurrent worker counters differ at the stop. No rendered frame
or successful archive/asset-decoding result is established.

Guest fibers and the narrow system-module registry are implemented. Fiber synthetic tests pass
in both modes, including direct and PLT calls, stack/register/FPU preservation, contextless
restart and host-thread migration. Non-timed kernel read/write locks pass synthetic lifecycle,
ownership and concurrency tests in both modes. Checked thread creation and lifecycle operations
now pass synthetic tests with isolated per-thread TLS and concurrent read-only fixture I/O.
The DiscMap profile and five-argument internal loader pass synthetic guest ABI and error
tests in both modes. The new request-location errors and 64-bit failure outputs were also
checked against the supplied original DiscMap module with empty mapping tables. FIFO event
flags now pass synthetic lifecycle and contention tests, including cancellation and deletion
with blocked waiters. The new PCMPISTRM/VPCMPISTRM CPU extension passes 61,440 synthetic
cases per mode and native JIT fault checks. The diagnostic now reports explicit unavailable
dialog errors: CommonDialog is unsupported, so message-dialog initialization lacks its system
dependency. Normal Terminal confirms one CommonDialog request and no message-dialog request
before VideoOut open. Network initialization returns the explicit libSceNet
EOPNOTSUPP error; no network backend is initialized. Normal Terminal confirms subsequent
content reads in both modes. A process-local UserService profile now provides initialization,
termination, user queries and one-shot login events, with synthetic guest ABI and concurrency
checks in both modes. Normal Terminal confirms successful initialization in both modes with priority 700.
ShareUtility Ex2 now returns checked early validation/resource errors without initializing
a sharing service; normal Terminal confirms progress past it in both modes.
GNM owner/resource registration now reproduces the retail fixed-error behavior, without
creating graphics state. Both modes now pass three owner-registration requests.
Compute-queue mapping validates the retail argument contract and reports an explicit
backend failure; normal Terminal confirms seven such requests in each mode. A fixed
logical safe-area query writes ratio 1.0 while preserving reserved bytes; normal Terminal
confirms one query per mode. VideoOut open now reports an explicit no-device failure
for the main output; normal Terminal confirms progress past it in both modes. This policy
creates no display port or window. The retail gamma helper now prepares three channels in
12/16-byte structures with its explicit size argument and original range behavior. Guest ABI
and original-module checks pass in both modes; normal Terminal confirms progress to color
application. That call now rejects the invalid display handle before reading settings.
Its effect on the game path needs a normal Terminal run.

**Major remaining work:** the game's packed static pointers require a suitable address model;
the diagnostic's temporary high image mapping does not solve that. Game-level multithreading,
additional system services, archive/asset decoding and graphics integration
still need validation.

See [Development status](docs/STATUS.md) for the measured scope and next checks.

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
