# Graphics bring-up

No SotC frame has been rendered or presented. VideoOut currently has an explicit headless
profile: opening a port returns NO_DEVICE. Successful data-preparation helpers do not create
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

The second command first checks real Metal shader/draw/readback access, then initializes the
renderer and pumps its window on the macOS main thread. It selects Metal, disables game-specific
profiles and the launcher settings channel, and keeps configuration in the isolated directory.
No boot image is read, no kernel queue provider is registered, no image patches are applied
and no game commands are submitted. Its diagnostic config uses the SotC serial, a 1280x720
window and zero SDK/attribute metadata; these do not claim to represent the game's metadata.
Close the window to exit, or the 60-second watchdog terminates with code 21.

Seven native loader/renderer fixture tests pass, including configuration delivery, entry into
the event loop and initialization failure. The real renderer check currently stops at the
Metal prerequisite with code 24 because the tool process exposes no device. Run it in the
native Terminal session where the Metal prerequisite previously passed. Real renderer
initialization and a game frame remain unverified; a diagnostic window would not establish
SotC rendering.

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

The pinned bloodborne-runtime contains a shadPS4-derived GPU library with Vulkan/MoltenVK and
an experimental Metal backend. Its dependencies and isolated ARM64 library now build, but
the library is only loaded in the isolated prerequisite mode. Its APIs are not connected
to the game execution path. Native loading does not establish successful renderer
initialization or command execution.

A first game frame requires integrating initialization, scoped import resolution,
guest memory/fault tracking and command submission with this loader.
macOS window creation/event pumping belongs on the host main thread; game execution must leave
that thread available. Kernel event queues must have one consistent owner across VideoOut,
GNM and the kernel adapter. The existing CPU-only queue cannot independently validate or signal
renderer-owned handles. Bloodborne-specific image hooks must not be applied to SotC.

Only after a valid output port, actual display-buffer registration, translated GNM commands,
shader execution and successful presentation can this project claim game video. A window,
synthetic checker image, mode structure or artificial flip wakeup does not establish that.
