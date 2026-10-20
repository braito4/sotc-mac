# How it works

The host executable is native ARM64. bbcpu decodes the PS4 executable's x86-64 instructions
and either translates them to ARM64 with its JIT or interprets them. System calls cross a
typed host-call bridge into narrowly scoped runtime functions.

The loader reads a private image prepared from the user's executable. In linked mode it also
initializes the original libc and Fios2 modules from the same input. Unknown imports stop
execution with an explicit diagnostic instead of receiving blanket success results.

## Runtime adapters

The adapters reuse the pinned upstream platform, mutex, thread metadata, memory and file
backends. SotC-specific namespace mappings are explicit. Guest thread creation, lifecycle
operations and thread keys are intentionally unavailable in this single-guest-thread probe.

Direct/flexible availability queries inspect the real allocator state. The tessellation
address query reserves a collision-aware guest range within the native ARM64 address policy.
Fixed maps outside that policy remain rejected. This is separate from the unresolved packed
static-image pointers used by the game.

The clock family agrees with bbcpu's virtual TSC units and uses real host clocks and sleeps.
Date-specific timezone conversion preserves the guest ABI. Console writes are restricted to
stdout and stderr. Content reads use an explicit read-only mount.

## Fibers

Fibers keep guest CPU state and the guest stack, not suspended ARM64 C stacks. A private
dispatcher exits the translator at a yielding host-call boundary. It restores the caller
when a fiber returns to the thread, or resumes the selected fiber's guest continuation.

The continuation differs for interpreter direct calls and PLT tail calls; synthetic tests
cover both, plus the JIT path. A resumed fiber uses the destination host thread's translation
cache, watch records and guest thread pointer. Register state, local stack data and FPU control
state are checked across switches and host-thread migration.

Contextless fibers restart from their entry on the next run. Attach-context and lifecycle
errors follow the reference API. Yielding through a nested host-to-guest callback is rejected:
those live host frames cannot be suspended by this guest-only dispatcher. Active trap-handler
and ahead-of-time recompiled-function paths are also outside this diagnostic's fiber scope.

The module registry advertises only the implemented fiber library. Missing modules return
an unavailable result; no trophy or share service is fabricated.

## Limits

The temporary high image mapping does not validate SotC's packed static pointers. CPU progress
does not establish renderer correctness, game content loading, saving or playability. Those
need separate implementation and end-to-end checks.
