# Building from source

Use an ARM64 macOS Terminal. `uname -m` must print `arm64`. Rosetta is not required for this
workflow. The official upstream comparison suite uses an x86 reference; it is not part of
these native-only checks.

> [!WARNING]
> After a reported host watchdog panic, the owner has authorized one brief GPU
> comparison at a time. DMA remains disabled; do not replay the incident settings
> or chain tests automatically. Use the [current comparison](../DEVELOPMENT_HANDOFF.md#gradual-gpu-testing).

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

Both commands also run 8,064 original scalar indexed-load cases per CPU mode,
including masked 64-bit indices, wraparound, scales, displacement and guarded
page edges. Contracts additionally exercise the actual terminating diagnostic
with synthetic invalid metadata. See [CPU fault diagnostics](../CPU_DIAGNOSTICS.md).

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

Headless probes default to a 30-second watchdog. At the owner's request, the combined
graphics/game path now defaults to no duration limit. `SOTC_DIAGNOSTIC_SECONDS=0`
disables both the supervisor wall deadline and the native guest alarm. Explicit
1..1800-second runs remain available; their supervisor deadline includes renderer
startup. The combined launcher passes the same setting to both layers.
GPU-progress supervision and cooperative cancellation stay active. Close the window
or press Ctrl+C to stop manually; inspect the log before another GPU test.

Renderer initialization keeps its independent 60-second budget, and the Metal
prerequisite retains its ten-second supervisor. A timeout still fails the paired
test and does not establish successful gameplay. Neither cancellation nor a
user-space timer guarantees recovery from a kernel/driver hang.

## Isolated renderer build

The checked Metal retention regression (`python3 tools/check_metal_retention.py`)
uses production batch creation, rollover, commit and scratch-return methods with
Foundation CPU doubles. The original unretained policy must reproduce premature
resource release and the adapted policy must preserve objects until completion.
No Metal framework/device, native game or GPU test runs in this regression.

The checked staging-tail regression (`python3 tools/check_staging_tail.py`)
compares actual preceding/adapted allocation methods with CPU resource doubles.
It verifies disjoint payloads/flush offsets, alignment, ownership, completion,
full tails and growing requests without allocating multi-GiB payloads or using
a GPU. Native memory savings remain pending.

The checked binding-copy regression (`python3 tools/check_binding_copy.py`) runs
the actual unified binding/copy and staging allocation methods against CPU
doubles. It compares duplicate read-only bindings before/after the correction,
checks exact synthetic payloads and verifies mapping, tick, range, write,
formatting and scope guards. Large capacities are symbolic; the test neither
allocates GiB of RAM nor initializes a GPU. The builder runs it automatically.

The checked integer-blit regression (`python3 tools/check_integer_blit.py`) uses
the production format selector, compatibility guards and shader bodies with CPU
texture doubles. It checks exact signed/unsigned texels and mip selection without
creating a Metal device. The builder runs it automatically. This is not a Metal
shader compilation or a native pixel test; see the
[handoff](../DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

The [dependency inventory](../DEPENDENCIES.md) and
[lock](../../tools/dependencies.json) select current source heads and stable
library versions. Runtime/CPU/reference pins are updated; the renderer uses a
separate integration workspace (`gpu-source-22923f080229` and
`gpu-build-22923f080229`). The workspace key remains fixed within this update;
the builder independently verifies the selected b2ff9cd0 Git HEAD. Older qualified
source/dependencies are preserved. Do not manually rebuild the legacy
`work/gpu-build` directory expecting the new library.

The Metal prerequisite has passed on Apple M1 in the user's native Terminal. The isolated
ARM64 renderer library has also built successfully. To build or resume it (macOS 15 or later):

```sh
bash packaging/build-gpu-arm64.sh
```

This uses a separate pinned source checkout and build area under `work`, leaving the current
CPU diagnostic and `runtime/upstream` unchanged. The first build downloads and compiles
dependencies and needs network access. Completed matching-version dependencies are
reused; old empty marker files cannot certify a new version. Re-run after an interrupted
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

The current full build also checks pipeline validity, copy chunking, staging
reuse, staging tails, adjacent binding-copy sharing, memory observations, deferred scene-proxy
ownership, completed coroutine frames, collector-content safety and synthetic shader conversion without
initializing a GPU. Cache is 24/9. These checks do not establish native game video.

glslang 16.6.0 is selected for host shaders. The builder recognizes a locally built
`work/host-tools/prefix/bin/glslang`, or an explicit `SOTC_GLSLANG` executable.
To reproduce the local host-tool build without installing system software:

```sh
git clone --depth 1 --branch 16.6.0 https://github.com/KhronosGroup/glslang.git work/host-tools/glslang-source
cmake -S work/host-tools/glslang-source -B work/host-tools/glslang-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_INSTALL_PREFIX="$PWD/work/host-tools/prefix" -DENABLE_OPT=OFF -DENABLE_GLSLANG_BINARIES=ON -DBUILD_TESTING=OFF
cmake --build work/host-tools/glslang-build --parallel 2
cmake --install work/host-tools/glslang-build
```

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
`BB_JIT=0` for the interpreter. Unknown and incompatible imports stop explicitly;
GPU-progress supervision remains active even with no game-duration limit. Logs and
configuration stay private under the repository.
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

For the checked vertex-zero compatibility policy and wave64 scratch correction:

```sh
python3 tools/check_shader_compat.py
```

The original SPIRV-Cross limitation above remains: this correction admits only
vertex-zero inputs through the exact Flat path where provoking vertex is fixed
to the first vertex, and retains the other unsupported
interfaces. The combined regression also reproduces the original mixed-width
workgroup declaration defect before testing the corrected compute emitter. It
uses no GPU or game data. The builder runs it after rebuilding the library.


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

No renderer rebuild is required for host adapter changes. The combined command
rebuilds that adapter. Use the current single comparison in the handoff after the
host incident, and inspect its log before proceeding.

Synthetic keyboard and gamepad ABI tests are part of `--contracts-test` in both CPU
modes; no physical controller or game files are needed for those tests.


## Dynamic shader-memory reads

The default-on `BB_DIRECT_MEMORY_ACCESS=1` experiment has been withdrawn after the
reported host watchdog panic. The loader forces the previous setting, 0, and rejects
any explicit value other than literal `0` before loading the library. The script
also rejects it before Vulkan discovery or the Metal precheck. This applies to
combined, renderer-only and load-only modes. It does not establish that DMA caused
the panic or that the old configuration is safe: the preceding run already logged
a stalled GPU submission.

The earlier offline `tools/check_shader_memory.py` fixture checks four original
synthetic IR cases through the actual installed SRT and shader-info passes with
DMA off and on. It verifies
uniform data, dynamic-read flags and page-table/fault-buffer resources without
initializing a GPU. It requires the isolated renderer build. The existing walker's
Phi/lane error messages still appear. That result only verifies compiler resource
selection, not hardware execution, system stability or game-image correctness.
Do not rerun real-library experiments during incident containment. No GPU rebuild
is required for the host rollback. PerVertexKHR conversion and GPU memory mapping
remain unresolved source-analysis tasks.
