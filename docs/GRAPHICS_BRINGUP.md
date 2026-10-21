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

The adapter connects six scoped AudioOut calls: Init, Open, Close, GetPortState, Output and Outputs. SDL audio setup
stays on the renderer main thread; only a ready CoreAudio backend is accepted. Main/BGM
outputs accept the supported local user or system user, with a shared bounded port pool.
Open must create/resume an actual SDL playback stream before returning a positive opaque
handle; failures release partial resources. Close owns stream destruction, and handles
are not reused. The default headless diagnostic leaves these imports unresolved.

The low parameter byte selects one of eight 48 kHz sample formats. Bit 16 is the restricted
attribute, as defined in [shadPS4's AudioOut header](https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/audio/audioout.h).
It is retained as port metadata independently of format/channel selection. Other unsupported
attributes are rejected. The adapter exposes playback only, with no recording, loopback or
dump route; this does not implement a host-wide capture restriction. Volume updates and
downstream AJM services remain unresolved. Samples are submitted only to the actual SDL
playback stream, without a timer-only sink or fabricated silence.

GetPortState queries the device bound to an owned live SDL stream and its current format.
Main/BGM use the logical primary route. Mono/stereo streams retain their retail channel
count; multichannel streams use current device channels, capped at the stream's input count.
Volume is the retail non-pad sentinel, flags are zero and the explicit reroute count is zero;
this profile does not track automatic host routing events. A missing device, unavailable query
or invalid device information fails without changing the output. Null pointers are checked
before initialization and handle validity, following the supplied module. Defined fields are
written only through byte 15, preserving the padding byte and both reserved 64-bit words.

Guest tests cover paired normal/restricted system-user streams, all formats, ABI widths,
unknown flags, provider failures, independent handles, mixed-user capacity and lifecycle in
both CPU modes. State checks also cover live device format changes/failures, stale handles,
unaligned output, reserved bytes and a buffer ending at a protected-page boundary. The
isolated supplied original export also matches main/BGM mono/stereo/multichannel field
values, null/init/handle error order, the volume sentinel and preserved padding/reserved
bytes in both modes. That private comparison supplies synthetic device/port tables and
suppresses locking; it does not initialize the original audio driver. The
synthetic graphics loader executes restricted Open/GetPortState/Close through actual dynamic
SDL symbol resolution, including the main-thread setup and guest-worker callbacks, in both
CPU modes. Native SDL selects CoreAudio in this tool process, but no default playback device
is visible there. Terminal confirms all three real streams open without failures, including
the restricted stream, starts sndx_out_thread and passes two GetPortState queries. That
worker passes the kernel/POSIX thread-self alias and starts the stream scheduler before
submitting PCM to SDL. Terminal confirms sustained accepted buffers with no output errors;
main passes HideSplashScreen and the NP title response, then finishes initialization and
starts the main loop and Shadow Game Update after world-loading progress.

Output/Outputs validate the full batch before staging samples: initialized service, 1..25
16-byte descriptors, live owned non-duplicate handles, equal buffer sizes and readable input.
Only a successful SDL put counts as submitted PCM. Legacy 8-channel layouts swap the rear
and side pairs for SDL; standard layouts and mono/stereo remain unchanged. The retail return
is frames, not frames multiplied by channels. All streams in a batch share one monotonic
cadence deadline. Queue backpressure caps input at two buffers; a stalled device returns an
error after a bounded wait. Null sample pointers flush/drain queued data without submitting
silence. A later provider failure can leave earlier ports submitted; counters report only
accepted buffers/bytes. Close cannot destroy streams during submission or drain.

Guest tests cover exact sample bytes in all eight formats, legacy/standard channel order,
ABI widths, unaligned descriptors, protected descriptor/sample boundaries, validation before
submission, maximum-sized batches, partial backend failure, missing device/queue support,
one batch cadence and queue/drain failures in both CPU modes. The synthetic graphics loader
also executes individual and batch submission through dynamically resolved SDL callbacks.
Private isolated original Output/Outputs comparisons confirm frame returns, null drains,
raw copies and relevant init/count/pointer/duplicate/size errors with synthetic driver state.
They do not initialize the original driver. Real combined SDL acceptance is now confirmed;
audible sound and a game frame remain unverified.

The earlier audio configuration SIGBUS is not reproduced in that latest run. Fatal reports
retain ARM64 ESR/FAR, signal code, host mapping protection and GPU trap flags without touching
the faulting bytes, acquiring the guest VMA lock or changing protection. Synthetic guest
writes cover read-only mappings and writable mappings beyond file backing; speculative-read
recovery still passes. Bounded AudioOut init/open diagnostics identify preceding errors.
Re-run the combined command in Terminal and keep the private log and next stop boundary.
No general memory-fault repair is claimed.

## Launch-splash availability

The scoped no-argument HideSplashScreen call reports the supplied retail wrapper's
absent launch-control error. The diagnostic owns no PS4 launch-control context or splash
control event, so it does not claim a splash was dismissed or create a replacement image.
The game caller discards this result. Synthetic guest tests cover repeated calls, ignored
stale registers, exact scope/capability, unchanged buffers/errno and continuation through
the graphics loader in both modes. An isolated supplied original export gives the same
error with no launch-control context and a supplied SDK lookup, without original module
initialization. The real combined run continues after this response while PCM submission
continues through the NP title response, then finishes initialization and starts the startup
script and reaches the main loop after world-loading progress.

## NP availability

The exact-scoped NpSetNpTitleId and adjacent NpSetContentRestriction calls use the supplied
NP manager's uninitialized path. They return NP NOT_INITIALIZED before inspecting title ID,
secret, restriction pointer or size. The diagnostic does not initialize NP, inspect/copy/log
title data or report a successful registration, restriction update or signed-in account.
The game caller discards these results. Guest tests check the one/two-pointer ABI, repeated
errors, null/unreadable pointers, unchanged buffers/errno and exact scope/capability in both
CPU modes. An isolated supplied original manager confirms those paths without initialization;
the synthetic graphics loader also exercises both responses and continues in both modes.
Terminal confirms continuation after the title response; the optional restriction call was
not taken. The language query now passes in Terminal; initialization finishes and the startup script
passes NetCtlInit and reaches the main loop.

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
use, original NGS2 loading and three AudioOut stream opens. The sound-output worker now
passes device-state and thread-self calls and submits PCM to SDL without output errors.
Main passes the absent launch-control and NP title responses and reaches
the main loop after world-loading progress and creates Shadow Game Update. SoundSystemSndx
then stops at scePthreadGetthreadid.
Additional retail ABI adapters, system imports and GNM operations will be
needed as the game reaches them. Static-image pointer/address assumptions and subsequent
asset loading remain unresolved. Bloodborne-specific image hooks must never be applied to SotC.

Only after a valid output port, actual display-buffer registration, translated GNM commands,
shader execution and successful presentation can this project claim game video. A window,
synthetic checker image, mode structure or artificial flip wakeup does not establish that.

## Local system language

The exact-scoped SystemServiceParamGetInt adapter supports only language ID 1 with an
explicit English-US profile (value 1), matching the supplied retail wrapper's fallback
when its settings query fails. This does not read host preferences or create a PS4 settings
service. Null output and other IDs return PARAMETER; other settings remain unsupported.
Only four output bytes are written, including for an unaligned destination. Synthetic
guest ABI, ID width, guarded output, errors, scopes and errno checks run in both CPU modes;
a private original-module comparison confirms the language fallback and null/invalid-ID
behavior. The graphics-loader fixture checks the language value during initialization.
Terminal confirms five language queries, completed initialization and entry into the startup
script and main loop. SoundSystemSndx then reaches scePthreadGetthreadid; no game frame has
been verified.

## Network-control availability

The exact-scoped no-argument sceNetCtlInit returns NetCtl NOT_AVAIL because the diagnostic
has no PS4 network-control IPC service. It creates no client, callbacks, initialized state
or apparent connection. Guest tests cover repeated errors, stale argument registers, exact
scopes/capability and unchanged buffers/errno in both CPU modes. A private supplied-module
check injects this error at IPC-client creation and confirms that the original initializer
propagates it, unlocks and leaves the client absent; this is a dependency-failure comparison,
not a successful original service startup. The synthetic graphics loader also continues
after this response in both modes. Terminal confirms actual continuation through world-load
progress into the main loop, creation of Shadow Game Update and four keyboard samples.

## Numeric thread identity

The exact kernel-scoped scePthreadGetthreadid adapter returns a positive 32-bit numeric
identity for the current native-backed guest thread. IDs are assigned once, remain stable
through waits and destructor callbacks, differ between concurrent threads and are never
reused within the process. They are separate from ScePthread handles and macOS thread IDs;
no pointer or 64-bit host ID is truncated. Exhaustion stops explicitly instead of wrapping.
The supplied original export confirms a no-argument ABI and a 32-bit descriptor ID read.
Synthetic guest tests cover main/worker identity, stale registers, concurrent uniqueness,
post-join non-reuse, callback stability, capability/scope checks and errno in both CPU modes.
This does not change CPU context switching or make guest handles into original kernel
structures. Actual game continuation after this import requires Terminal verification.
