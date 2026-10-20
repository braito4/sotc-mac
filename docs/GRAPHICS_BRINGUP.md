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

The development tool process reports **zero enumerated Metal devices and no default device**.
The check compiles, but stops before shader compilation or rendering. Its successful-device
path is therefore not yet verified here. This is a process-access result, not evidence that
the physical Mac has no GPU. A Terminal result is needed before verifying an actual renderer.

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
an experimental Metal backend. This diagnostic does not build or link that library. Its
presence as source does not provide a renderer to the diagnostic automatically. An initial
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
