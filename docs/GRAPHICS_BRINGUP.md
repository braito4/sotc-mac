# Graphics bring-up

No SotC frame has been rendered or presented. The default CPU diagnostic has an explicit headless
VideoOut profile: opening a port returns NO_DEVICE. Successful data-preparation helpers do not create
an output device, register display buffers or execute GPU commands.

## Check host GPU access first

From the repository in a native macOS Terminal:

```sh
bash packaging/test-arm64.sh --graphics-check
```

This compiles an ARM64 Metal prerequisite check. With a device, it compiles synthetic vertex
and fragment shaders, renders a triangle into a small offscreen texture, waits for completion
and verifies the pixels. It uses no game data, opens no window, and does not establish SotC
rendering. Resource or pixel-verification failures return exit 24.

On 2026-10-20 the user ran this exact check in a native Terminal on **Apple M1** and reported
PASS for shader compilation, pipeline creation, the triangle draw and pixel readback. The
host prerequisite is therefore established in that Terminal session. No SotC frame was involved.

The development tool process still reports zero enumerated devices and no default device.
That process-access restriction does not invalidate the successful Terminal test. GitHub DNS
also fails in the tool process, so dependency downloads must currently run from Terminal.

## Build the isolated GPU library

```sh
bash packaging/build-gpu-arm64.sh
```

This downloads a separate checkout at the pinned bloodborne-runtime revision, including the
exact FSR-Vulkan, ImGui and CPU submodules recorded in that commit. The current source snapshot
is missing FSR-Vulkan and ImGui contents. The builder preserves `runtime/upstream` and the
CPU diagnostic, and does not read game input or start a renderer.

The build uses the pinned upstream ARM64 dependency recipes, omitting the unrelated app-bundle
Bash/Python runtimes and the x86 assembler prerequisite. It verifies the recipe's SHA-256 before
preparing that subset; an unexpected recipe is rejected. The generated recipe runs with the
system Bash and limits compiler parallelism (two jobs by default; `SOTC_GPU_JOBS=1..8`).

A glslang compiler is required. The builder uses `SOTC_GLSLANG`, a compiler on PATH, or the
existing ARM64 glslang from the adjacent local shadPS4 build. It does not install Homebrew
packages. Other missing tools are reported before downloads.

Sources, dependencies, build output and a private ABI inventory stay under `work`; build logs
stay under `logs`. Existing source checkouts must have the exact expected revision, remote
and clean submodule gitlinks. Foreign or modified checkouts are preserved and rejected.
Successful builds must produce an ARM64-only library with the loader API exports. The build
report records its unresolved runtime/CPU symbols for the next integration step.

Useful separate stages:

```sh
bash packaging/build-gpu-arm64.sh --check  # Read-only inventory; exit 2 if incomplete
bash packaging/build-gpu-arm64.sh --fetch  # Source/submodules only
```

Sixteen offline tests cover recipe integrity and shell syntax, bounded parallelism,
preservation of foreign/redirected paths, cleanup after failed fetches, gitlink validation,
command-failure reporting and real native builds under dependency prefixes containing spaces.
The Zydis include flag retains literal quotes for CMake's compiler-flag parser. FFmpeg's
generated shell configuration and Darwin install-name flags receive separate quoting fixes;
installed pkg-config metadata also escapes absolute directory values. The builder clears
only dependency pkg-config cache entries when reconfiguring CMake, so an earlier broken
configuration can recover without discarding completed dependencies. Tests build and load a
synthetic library through Make/Clang and a real CMake/pkg-config consumer after cache recovery.

On 2026-10-20 all dependencies were installed and the actual isolated GPU library compiled
and linked successfully as ARM64. All five FFmpeg shared libraries were checked for ARM64
architecture, native loading and expected version majors. The GPU builder verified its
required loader exports and recorded unresolved runtime/CPU symbols for integration.
An incremental rerun uses completed packages. The GPU library targets macOS 15 or later,
matching the pinned runtime's documented minimum and its C++ library requirements.
No renderer was launched and no game frame was verified.

## Check host symbol coverage before loading

After building both the GPU library and the CPU diagnostic on the Mac:

```sh
python3 tools/check_gpu_link.py
```

This reads the actual binaries with `nm`, compares the GPU's runtime/CPU imports with
external definitions in the diagnostic, and checks the required GPU API exports. Undefined
references and local definitions do not satisfy a host import. It does not load either binary
or invoke library constructors. Missing symbols return exit 2; missing files or an invalid
inventory return exit 1. The private report is `work/gpu-link-check.json`; explicit
`--library` and `--executable` paths can select another build.

A complete symbol inventory is only a linking prerequisite. It cannot verify function
signatures, dependent-library loading, memory/fault hooks, shared queue ownership or window
threading. A successful inventory therefore still reports ABI and game video as unverified.
Seven tests cover missing APIs, undefined/local host symbols, malformed inventories and a
native shared-library/executable fixture with a deliberately missing then supplied host
function. These fixtures now run on macOS as well as Linux.

## Load and initialize in separate stages

```sh
bash packaging/test-arm64.sh --gpu-load-check
bash packaging/test-arm64.sh --renderer-check
```

The first command rebuilds the diagnostic with host symbol exports, checks symbol coverage,
and loads the actual library with immediate binding. It finds all six required API exports
and unloads without calling them. On 2026-10-20 this passes on ARM64 macOS: all 46 runtime/CPU
imports are present. This is an actual native load test, separate from the symbol inventory.

The missing host contracts now reuse the diagnostic's real clock origin and TSC frequency,
existing CPU callback bridge, sleep measurements and wait accounting. Callback tests cover
native arities 0 through 6 and nested guest-host-guest execution in both CPU modes. Sleep
statistics return and reset measured intervals. Restart remains explicitly unsupported: a
request exits with `STOP_RESTART_UNAVAILABLE` and code 27; it cannot execute the upstream
Bloodborne launcher or claim a successful restart.

The library creates cache folders in a C++ static initializer, before `bbgpu_init` runs.
The commands therefore create an isolated directory under `work` and set `BB_GPU_USER_DIR`
before loading. A direct invocation without an existing writable absolute directory is rejected
with code 28. This avoids falling back to the user's ordinary shadPS4 configuration. Load,
missing-export and missing-host-hook failures also return 28. Generated directories stay private.

The renderer command first selects the bundled Vulkan loader for SDL and sets both
`VK_DRIVER_FILES` and the legacy `VK_ICD_FILENAMES` to the bundled MoltenVK manifest.
This Metal backend still uses Vulkan for its surface and initialization layer. Selecting
Metal does not remove that prerequisite. The standalone loader, without its driver manifest,
was observed to advertise only four extensions and no `VK_KHR_surface`, causing SDL window
creation to abort. With the manifest selected it advertises 19 extensions, including
`VK_KHR_surface` and `VK_EXT_metal_surface`. No upstream source changes are required.

Check that prerequisite separately, without creating an instance, device or window:

```sh
bash packaging/test-arm64.sh --vulkan-check
```

The native regression test reproduces rejection with a missing driver manifest, verifies
the real loader with the bundled manifest, and checks that the shell command replaces
inherited incorrect loader/driver paths. Five surface-contract tests pass on the Mac.

The renderer command then checks real Metal shader/draw/readback access and initializes the
renderer and pumps its window on the macOS main thread. It selects Metal, disables game-specific
profiles and the launcher settings channel, and keeps configuration in the isolated directory.
No boot image is read, no kernel queue provider is registered, no image patches are applied
and no game commands are submitted. Its diagnostic config uses the SotC serial, a 1280x720
window and zero SDK/attribute metadata; these do not claim to represent the game's metadata.
Close the window to exit, or the 60-second watchdog terminates with code 21.

Seven native loader/renderer fixture tests pass, including configuration delivery, entry into
the event loop and initialization failure. On 2026-10-20 the user confirmed real renderer
initialization in native Terminal: guest-memory imports and residency setup completed,
four Metal pipelines were cached and thousands of command buffers were encoded in Metal.
The renderer-only run reported zero draws and dispatches because it ran no game code.
The initially absent cache profile did not prevent initialization from returning success.
This tool process still exposes no Metal device and stops at the prerequisite with code 24.
No SotC frame has been verified.

## Optional game connection

From a native Terminal with the prepared game dump and built GPU library:

```sh
bash packaging/test-arm64.sh --graphics-game-test
```

This prepares a separate private linked diagnostic image with the supplied original
libc, Fios2, system-gesture and NGS2 modules, initializes the renderer on the host main thread and runs module initializers/game entry on a separate host thread. Its default
CPU mode is JIT; `BB_JIT=0` selects the interpreter. The normal bounded game watchdog remains
active. Read-only content mounts, private logs/configuration and unknown-import stops remain.
The guest SDK version comes from the validated process parameter; PSF attributes are still
zero for this SDR diagnostic. The renderer retains its standard extra memory pool unless
`BB_DMEM_EXTRA_MB` is set explicitly. There is no save mount or playable launcher.

A scoped allowlist connects selected VideoOut and GNM entry points. One GPU provider owns
all supported kernel event-queue creation, waits, user events and graphics notifications.
Activation rejects existing CPU queue handles or an incomplete provider. Unknown scopes and
missing selected symbols cannot fall back to a second queue registry. The checked gamma,
buffer-attribute and mode-data helpers remain local. Retail resource registration keeps its
checked failure path. Color adjustment and device-capability operations use explicit retail-to-HLE adapters.
The supported color profile has equal RGB gamma in 12 or 16 bytes (zero reserved word).
It is converted to the renderer's scalar gamma and applied through the real backend. Other
color sizes, unequal channels or nonzero reserved data stop with `STOP_GRAPHICS_COLOR` (29).
Capability queries accept an exact eight-byte output with a full-width size, and return the
backend's actual flags. Output is staged so native failures preserve the caller's buffer.
Successful native open/close operations determine live display ownership, and invalid or
closed handles are rejected before accessing caller buffers. Additional GNM operations remain unresolved until reviewed.

Guest TLS templates are published before the GPU creates workers. GPU page-tracking faults
and speculative-read recovery are connected before renderer initialization. No Bloodborne
image patches are called. Synthetic tests run a generated original x86 fixture through the
actual loader in both CPU modes, proving main-thread initialization, worker-thread guest
execution, scoped VideoOut routing, linked module startup, speculative-read fault recovery and
the next unknown-import stop. A separate contract test checks provider activation, strict
scopes, ABI exclusions and queue ownership. The default paired game test still reaches the
same `scePadInit` boundary in both modes.

The user ran the combined graphics/game path in Terminal. It reached a real VideoOut handle
and then stopped at the formerly blocked color-adjustment import. The color and capability
adapters now pass synthetic guest ABI checks in both CPU modes, including backend failures,
unaligned/bounded buffers, 32/64-bit size handling and live display lifecycle. The real combined run now passes both adapters and reaches input startup. Tool execution still stops at the Metal prerequisite; game
presentation and asset/pointer compatibility remain unverified.

## Keyboard-backed input

The real combined Terminal run now passes color/capability operations, starts RenderFlip and
Game Render, passes original gesture initialization and opens the keyboard-backed
controller. Save/trophy task threads then start, and a thread key is created and set twice
and UUID generation passes. The original NGS2 load now succeeds and a sound-loader thread
starts before the next stop at `sceAudioOutInit`.
The controller has not been sampled yet. Graphics mode connects a keyboard-backed logical controller after the renderer has initialized SDL video/gamepad services. SDL readiness and
key names are checked on the main thread before guest startup. Headless mode retains its
existing unresolved input boundary.

The supported scoped APIs are initialization, local-user standard-port open, close,
read-state, buffered read and controller information. A successful open creates a real
logical-device lifecycle; reads sample SDL's current keyboard state. The 120-byte guest
packet has neutral sticks when no direction is held, a real process-clock timestamp,
translated buttons/triggers and keyboard touchpad clicks. Buffered read produces one current
sample rather than a fabricated backlog. The renderer menu captures input while open.

| Input | Keys |
|---|---|
| Left / right stick | WASD / arrow keys |
| Cross / Circle / Square / Triangle | Space / Left Shift / E / Q |
| L1 / R1 / L2 / R2 | 1 / 3 / R / F |
| L3 / R3 / Options | Z / C / Enter |
| D-pad | I / J / K / L for up / left / down / right |
| Left / right touchpad click | Tab / Backspace |

This is a fixed keyboard profile. Physical gamepads, mouse, remapping, vibration and motion
sensor operations are not connected and remain unresolved. It incorporates no Bloodborne heap
inspection, input injection or recording. Synthetic guest tests pass in both CPU modes for
service lifecycle, state changes, menu capture, touch IDs, exact wire layout, error/guard
behavior and stale handles. The synthetic linked-loader test also reaches `scePadInit` and
checks that an unavailable SDL backend prevents guest startup. Actual keyboard input in the
combined game run remains to be verified in Terminal.

## Original system-gesture module

The combined command now reads the supplied `libSceSystemGesture.sprx` from the sibling
`sys_modules` directory. Set `SOTC_SYSTEM_MODULE_ROOT` to an alternate directory containing
that module if necessary. It is required only by `--graphics-game-test`; the default
headless linked test still uses just libc/Fios2. Missing, incomplete or wrong-identity
modules fail explicitly. Nothing is downloaded or copied into the game directory.

The pinned linker binds the original gesture exports by library/module identity and writes
`work/sotc-graphics-arm64/boot-linked.bin`, separate from the headless image. All output stays
private. Original executable bytes and FS-relative instructions are preserved; an empty
PT_TLS metadata entry allocates no thread-storage template. There is no host gesture stub
that reports success or fabricates an empty event list.

The module's SDK query reads the application's actual four-byte SDK field from the validated
process parameter. Null outputs fail with EINVAL; a missing application profile reports the
diagnostic's explicit ESRCH policy without changing output. Guest tests cover exact-width,
guarded and unaligned buffers and errno in both CPU modes. Seven preparation fixtures cover
spaced paths, input preservation, strict module identity, missing data, unresolved gestures,
empty TLS metadata and foreign output preservation.

Private checks against the supplied original library pass in JIT and interpreter: all three
module initializers return zero; the gesture service rejects open before initialization,
accepts repeated initialization, opens/closes a valid service and permits repeated finalization.
These checks do not run game entry or establish touch recognition. The real combined Terminal
run now confirms progress through original gesture initialization and keyboard pad open. It
starts save/trophy tasks, uses a thread key and passes UUID generation. This does not establish touch
recognition, save support or trophy support.

## Original NGS2 module

The latest real combined run reaches an audio configuration write with a null object after
module ID 0x000B (NGS2) fails to load. The graphics command now reads the supplied original
`libSceNgs2.sprx` from `SOTC_SYSTEM_MODULE_ROOT` (the same directory as the gesture module),
validates its library identity and required exports, and links it into the private graphics
image. No original file is patched or distributed, and no silent-audio stand-in is used.

Both CPU modes initialize that actual module successfully. The loader requires executable
native system-create/render bindings in the same module and a successful initializer before
publishing it as available. Load/is-loaded/unload then use checked static-provider references;
unload does not unmap code or rerun original finalizers. Synthetic linked fixtures cover
reference lifecycle, premature load, missing/wrong-scope/data/non-executable bindings,
inconsistent module ownership and initialization failure in JIT/interpreter. The headless
command remains unchanged. Real NGS2 system creation, AJM decoding and AudioOut playback
remain unverified; missing downstream services still stop explicitly. The combined Terminal
run now confirms progress beyond that failed audio-object setup: NGS2 loads and a sound-loader
thread starts before the unresolved AudioOut initialization call. No game frame is established.

## AudioOut device-stream lifecycle

The next adapter connects only three scoped AudioOut calls: Init, Open and Close. SDL audio
subsystem setup stays on the renderer main thread; only a ready CoreAudio backend is accepted.
The guest service has checked one-time initialization, local/system-user main/BGM port validation,
48 kHz formats and bounded stream capacity. Open must create/resume an actual SDL playback
stream before returning a positive opaque handle; failure releases partial resources. Close
owns stream destruction, and handles are not reused. There is no timer-only or dummy-device
success path. The default headless diagnostic still leaves these imports unresolved.

Guest tests verify lifecycle, format/argument ABI, failures, capacity and ownership in both
CPU modes; the synthetic linked fixture also calls the real Init adapter. Native SDL audio
setup selects CoreAudio in this tool process, but a stream open fails because no default
playback device is visible there. The next Terminal run must verify real stream access.
PCM submission, volume/state queries and downstream AJM services are still unresolved; no
samples are submitted, recorded or replaced with fabricated silence by this adapter.

The latest Terminal execution faults at an audio configuration write to non-null direct
memory in both CPU modes. A matching interpreter fault rules out a failure exclusive to
JIT emission; it does not identify the underlying cause. Fatal reports now include ARM64
ESR/FAR, signal code, host mapping protection and GPU trap flags, without reading the
faulting bytes, acquiring the guest VMA lock or changing protection. Synthetic guest writes
exercise read-only mappings and writable mappings beyond file backing in both modes, while
the existing speculative-read recovery still passes. Audio initialization and the first
sixteen port-open attempts also report their arguments/results before a possible fatal exit.
These are diagnostics, not a memory or audio repair. Re-run the command above in Terminal;
keep its private log, including any AudioOut lines and the complete fatal report.

The resulting Terminal report confirms Init succeeds, then Open rejects the system user
(the pinned UserService header defines system ID 0xff). That restriction in the adapter was
incorrect: global main/BGM streams now accept either the supported local user or the system
user. Both identities share the existing port capacity and CoreAudio device requirements;
other IDs remain outside this profile. Guest regression checks exercise a system-owned
eight-channel floating-point stream, failed open/resume, close and shared mixed-user limits
in JIT and interpreter. The subsequent faulting page has no host permissions and no GPU
trap flags. The corrected open validation still needs a real combined run to establish
whether startup proceeds; no memory protection is bypassed and no game frame is claimed.

## Verified CPU-side display contracts

- Buffer attributes write the exact 40-byte layout. Pitch is the seventh integer argument on
  the guest stack; guarded and unaligned outputs and 32-bit widths pass in both CPU modes.
- RegisterBuffers checks null required pointers before the invalid display handle. No buffers
  are registered in the headless profile, including when passed non-null protected pointers.
- ModeSetAny fills the caller's requested extent and writes the four-byte size field, even
  for a shorter requested size. It only prepares data.
- ConfigureOutputMode checks reserved/null mode arguments before rejecting the invalid port.
  It never dereferences non-null mode/options on that failure path.

These four contracts were also compared with the supplied original VideoOut exports in both
CPU modes. The isolated empty-port comparison suppresses original initialization and locking;
it establishes these data/error paths, not a functioning original display driver. The original
ConfigureOutputMode rejects invalid arguments before handle lookup, unlike the inspected
shadPS4 implementation; the adapter follows the observed retail order.

The latest paired game test reaches scePadInit after starting RenderFlip and Game Render.
RenderFlip enters a real event-queue wait. No flip notification
is generated because no output port or rendering submission has succeeded.

## Remaining integration

The pinned library and its renderer-only initialization are verified. The optional game
connection now implements scoped import routing, shared queue ownership, a main-thread event
loop, a guest worker and memory/fault hooks, with synthetic checks. The real combined run
passes original gesture initialization, keyboard pad open, UUID generation, thread-key
use and original NGS2 loading, then faults on a non-null audio configuration write in both
CPU modes. Fatal mapping/syndrome and bounded AudioOut diagnostics await a Terminal run.
Additional retail ABI adapters, system imports and GNM operations will be
needed as the game reaches them. Static-image pointer/address assumptions and subsequent
asset loading remain unresolved. Bloodborne-specific image hooks must never be applied to SotC.

Only after a valid output port, actual display-buffer registration, translated GNM commands,
shader execution and successful presentation can this project claim game video. A window,
synthetic checker image, mode structure or artificial flip wakeup does not establish that.
