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

Twelve offline tests cover recipe integrity and shell syntax, bounded parallelism,
preservation of foreign/redirected paths, cleanup after failed fetches, gitlink validation,
command-failure reporting and a real CMake/Ninja compilation with spaces in the dependency
prefix. The Zydis include flag retains literal quotes for CMake's compiler-flag parser;
shell quoting alone had split the prefix into separate Clang arguments.

The user completed the source fetch and initial dependencies from Terminal. After correcting
the quoting, the actual cached Zydis was configured, built and installed as ARM64 under the
existing path with spaces. A builder retry skips that completed package and reaches xxHash,
whose download still fails on GitHub DNS in the tool process. Re-run the same build command
in Terminal to resume pending dependencies. The complete GPU library build remains unverified;
a built library will still require the loader integration below before game video.

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
an experimental Metal backend. The new isolated builder prepares that library, but it has not yet completed a build or
linked the renderer to this diagnostic. Its presence as source does not provide a renderer
to the diagnostic automatically. An initial
CMake configuration cannot find an installed Vulkan SDK. Explicit use of the existing local
Vulkan headers/loader passes that step and stops at a missing fmt package configuration;
other dependencies have not yet been resolved and no GPU library has been built.

A first game frame requires building its dependencies and integrating its initialization,
scoped import resolution, guest memory/fault tracking and command submission with this loader.
macOS window creation/event pumping belongs on the host main thread; game execution must leave
that thread available. Kernel event queues must have one consistent owner across VideoOut,
GNM and the kernel adapter. The existing CPU-only queue cannot independently validate or signal
renderer-owned handles. Bloodborne-specific image hooks must not be applied to SotC.

Only after a valid output port, actual display-buffer registration, translated GNM commands,
shader execution and successful presentation can this project claim game video. A window,
synthetic checker image, mode structure or artificial flip wakeup does not establish that.
