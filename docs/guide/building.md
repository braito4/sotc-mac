# Building from source

Use an ARM64 macOS Terminal. `uname -m` must print `arm64`. Rosetta is not required for this
workflow. The official upstream comparison suite uses an x86 reference; it is not part of
these native-only checks.

## Sources

Clone this repository from its GitHub page, enter its directory and initialize the required
submodules:

```sh
bash packaging/test-arm64.sh --fetch
```

Only the runtime, CPU and SotC reference sources are needed for these diagnostics; GPU and
launcher dependencies are not built. The runtime and CPU revisions are pinned in
[Credits](../CREDITS.md). The fetch script checks out those revisions, not the latest upstream
branch. Locally supplied ZIP snapshots can be used at the same paths; do not run `--fetch`
over non-Git snapshot directories.

## CPU library and dependencies

Install Xcode Command Line Tools, CMake, Ninja and Python 3 if needed. Then:

```sh
bash packaging/test-arm64.sh --check
bash packaging/test-arm64.sh --test
bash packaging/test-arm64.sh --contracts-test
```

The upstream CPU build installs Zydis 4.1.1 and its bundled Zycore when absent. Its default
prefix in this project is `work/deps-arm64`. For an existing compatible static installation:

```sh
export BB_DEPS="/absolute/path/to/arm64-dependencies"
bash packaging/test-arm64.sh --test
bash packaging/test-arm64.sh --contracts-test
```

The prefix must contain `include/Zydis`, `include/Zycore`, `lib/libZydis.a` and
`lib/libZycore.a`. Both libraries must be ARM64 builds.

The build copies the CPU archive privately and removes `cpu.c.o` and `interp_vec.c.o`.
It includes unchanged pinned source files through a context adapter and a vector extension.
The extension handles PCMPISTRM/VPCMPISTRM; other vector instructions use the original
implementation. The decoder, JIT and remaining CPU objects are reused. The original archive
and upstream source files are unchanged.

`--test` runs the native emitter, JIT fault checks linked to the adapted archive, and synthetic
string-mask checks in both CPU modes. `--contracts-test` also includes the string-mask checks
alongside the runtime contracts. These include guest user-event queues with level/edge
notifications, timeouts and concurrent deletion. Neither command needs game files or Rosetta.

Both commands also run synthetic EXTRQ/INSERTQ register/immediate checks in JIT and
interpreter, covering every six-bit control pair, including the explicit QEMU-compatible policy
for architecturally undefined overrunning fields. These use a scalar bit oracle, not native x86 hardware execution.

They also run `tests/test_cpu_widen.c`: 64,512 cases per mode across signed/unsigned
byte, word and dword widening, SSE128/VEX128/VEX256, aliases/high registers, exact-width
memory against inaccessible pages, upper lanes and flags. No complete AVX2 coverage
or x86 hardware oracle is implied.

The thread adapter similarly includes the pinned thread backend and wraps its start, TLS and
lifecycle boundaries. A private build copy of the mutex backend makes only diagnostic counters
atomic. The source checkout is not patched. Synthetic tests now also start concurrent guest
workers and test condition waits, isolated TLS, thread completion and serialized read-only I/O.

The memory adapter includes a checked private build copy of the pinned backend. Its GPU
notification storage grows instead of stopping at 64 entries; the preparation script checks
the full source digest and changes only the storage declaration and capacity guard. The
upstream allocator, locking and hook dispatch remain intact. Contracts cover replay/release
of 257 real aliases, mixed bursts beyond 8,000 events and explicit storage failure.

For the current game/renderer boundary and a debugging roadmap, use the
[development handoff](../DEVELOPMENT_HANDOFF.md). Later sections here describe earlier
integration milestones and are not a statement of the latest observed import stop.

## Optional game initialization probe

Prepare [your own game files](game-files.md), then run `--linked-test` in a normal Terminal.
The linked test requires POSIX shared-memory creation for direct-memory backing. Some
restricted automation environments reject that operation; `--contracts-test` does not need it.

No renderer or playable app is built by these commands.

## Content and memory diagnostics

```sh
bash packaging/test-arm64.sh --content-check
SOTC_MEMORY_BACKING=file bash packaging/test-arm64.sh --linked-test
```

The first command runs synthetic archive/address checks and audits only the supplied PSARC
name manifests and file extents. Its private report does not establish asset decoding.
The second preserves shared physical aliases using an unlinked temporary file when POSIX
shared-memory creation is unavailable; it does not initialize graphics.

## GPU prerequisite

```sh
bash packaging/test-arm64.sh --graphics-check
```

This compiles and runs a synthetic Metal offscreen shader draw/readback check. It does not
build the game renderer. No device is exposed to the development tool process; run from a
native macOS Terminal to establish host access. See [Graphics bring-up](../GRAPHICS_BRINGUP.md).

The initialization probe has a 30-second watchdog. `SOTC_DIAGNOSTIC_SECONDS` accepts 1 through
60 seconds; it cannot disable the watchdog. A timeout still fails the paired test.

## Isolated renderer build

The Metal prerequisite has passed on Apple M1 in the user's native Terminal. The isolated
ARM64 renderer library has also built successfully. To build or resume it (macOS 15 or later):

```sh
bash packaging/build-gpu-arm64.sh
```

This uses a separate pinned source checkout and build area under `work`, leaving the current
CPU diagnostic and `runtime/upstream` unchanged. The first build downloads and compiles
dependencies and needs network access; GitHub DNS is unavailable in the development tool
process. Completed dependencies are reused. Re-run the same command after an interrupted
build; generated FFmpeg metadata and cached dependency paths are repaired for spaced prefixes.
A successful build checks ARM64 architecture and API exports and records the host runtime
symbols it needs. It does not launch the game. See [Graphics bring-up](../GRAPHICS_BRINGUP.md)
for stages, prerequisites and remaining loader integration.

The builder also applies a digest-checked set of reviewed upstream renderer backports
to the private checkout. It rejects unknown or partially applied modifications. See
the [upstream review](../UPSTREAM_REVIEW.md) for the exact changes. A successful GPU
build now also runs `python3 tools/check_tessellation.py`, which checks the actual
auxiliary SPIR-V emitter without initializing a GPU. Keep this separate from the
native Terminal visual verification.

Before attempting loader integration, run `python3 tools/check_gpu_link.py` on the Mac after
building the diagnostic and GPU library. It reports missing host symbols without loading the
library. Exit 2 identifies incomplete coverage; exit 0 establishes symbol presence only.
See [Graphics bring-up](../GRAPHICS_BRINGUP.md#check-host-symbol-coverage-before-loading).

After symbol coverage is complete, `bash packaging/test-arm64.sh --gpu-load-check` rebuilds
and loads the actual library without calling its GPU APIs. This now passes on ARM64 macOS.
Use `bash packaging/test-arm64.sh --renderer-check` in native Terminal to attempt renderer
initialization and open its diagnostic window. The latter first checks Metal device access;
it uses isolated cache/configuration directories and a 60-second budget. It does not run
the game or verify a game frame. See [Graphics bring-up](../GRAPHICS_BRINGUP.md#load-and-initialize-in-separate-stages).

The renderer command selects the bundled MoltenVK ICD and Vulkan loader for SDL explicitly.
The Metal backend still needs Vulkan surface extensions during initialization. If SDL reports
that `VK_KHR_surface` is missing, use the updated command and run
`bash packaging/test-arm64.sh --vulkan-check` to verify extension discovery separately.
This check now passes with 19 extensions and needs no window or game input.

## Optional game/renderer connection

Renderer-only initialization has now passed in the user's native Terminal. To attempt the
combined path with the existing prepared game dump and GPU library:

```sh
bash packaging/test-arm64.sh --graphics-game-test
```

It prepares the linked diagnostic, keeps the window on the main thread, runs the guest on a
worker and selects one GPU provider for reviewed graphics and kernel queue imports. Use
`BB_JIT=0` for the interpreter. Unknown and incompatible imports stop explicitly; the normal
bounded game watchdog remains active. Logs and configuration stay private under the repository.
A real combined run reached color adjustment after opening a display. Typed color/capability
adapters now cover that boundary; synthetic loader/routing/ABI tests pass in both modes.
The existing CPU-only `--linked-test` stays headless and still passes its paired comparison.

The latest combined Terminal run passes original gesture initialization and opens the
keyboard-backed controller after display setup. Save/trophy task threads then start before
a thread key is created/used and UUID generation passes. The latest run then stops at
`sceAudioOutInit` after the original NGS2 load succeeds and its sound-loader thread starts.
Graphics mode now connects a checked keyboard-backed logical controller to initialized SDL;
headless mode retains its input import stop. For fixed controls and the supported API scope,
see [Keyboard input](../GRAPHICS_BRINGUP.md#keyboard-backed-input) and
[DualShock input](#dualshock-input). Physical SDL3 gamepads are supported by the host
adapter; remapping and motion APIs remain unsupported. The command now privately links the supplied original `libSceSystemGesture.sprx` from the
sibling `sys_modules` directory; set `SOTC_SYSTEM_MODULE_ROOT` if it lives elsewhere. It uses
a separate image under `work/sotc-graphics-arm64`, preserving the headless diagnostic. Original
gesture lifecycle checks pass in both CPU modes. Thread-specific keys and their guest
destructors now also pass both modes; key creation and setting are confirmed in the real
combined run. UUID generation also passes. The command now additionally links the supplied
original `libSceNgs2.sprx` from that same system-module directory. Its availability is
published only after checked native bindings and successful original initialization. The
next combined run must verify the newly connected SDL CoreAudio Init/Open/Close calls.
The adapter requires an actual stream for a successful open and has no silent fallback.
Downstream PCM submission is still unresolved; sound remains unverified. No save/trophy
operation or keyboard sample has been verified by merely starting those task threads.

## Isolating the raw per-vertex fragment limitation

After the isolated GPU dependencies are built, run:

```sh
python3 tools/check_fragment_interface.py
```

This uses Sirit and the installed SPIRV-Cross MSL compiler without a GPU or game.
Exit 0 means both synthetic interfaces convert; exit 2 reproduces the currently known
PerVertexKHR limitation after ordinary-input conversion succeeds. Other failures are
errors. It does not validate driver compilation, pixels or a semantic workaround.


## DualShock input

Connect a DualShock 4 by USB or pair it with macOS before running the combined
`--graphics-game-test`. The adapter opens the first SDL-recognized gamepad, including
one connected after startup. Disconnect other controllers when selecting a DualShock.
The existing keyboard mapping is used whenever no physical controller is available;
a connected gamepad takes precedence. Headless CPU diagnostics remain unchanged.

Supported input includes the four face buttons, D-pad, Options, L1/R1, L3/R3,
both analog sticks, L2/R2 analog values and digital presses above half travel,
touchpad click and two touch contacts. Share/back maps to the touchpad shortcut.
The overlay captures physical input just as it captures keyboard input. Motion,
gyroscope, PS-button system actions and user remapping are not implemented.

The game can request RGB light-bar colors and small/large motor vibration. These
calls forward to SDL and succeed only if SDL accepts the operation. Missing hardware
features and backend failures return an explicit error. Rumble is limited to one
second per request and stopped on controller close. USB/Bluetooth feature availability
and actual physical output remain unverified until tested with hardware.

Look for `Input: physical controller connected:` in the private log. The final
`Input: SDL3 gamepad backend ready` line reports connection/disconnection counts,
physical samples, and accepted LED/rumble operations. Accepted operations do not
prove that a motor moved or a light changed. After disconnection a log message confirms
keyboard fallback; reconnection is detected on the next pad query.

No renderer rebuild is required. The existing command rebuilds the host adapter:

```sh
SOTC_MEMORY_BACKING=file SOTC_DIAGNOSTIC_SECONDS=60 BB_JIT=1 bash packaging/test-arm64.sh --graphics-game-test
```

Synthetic keyboard and gamepad ABI tests are part of `--contracts-test` in both CPU
modes; no physical controller or game files are needed for those tests.
