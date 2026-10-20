# How it works

The host executable is native ARM64. bbcpu decodes the PS4 executable's x86-64 instructions
and either translates them to ARM64 with its JIT or interprets them. System calls cross a
typed host-call bridge into narrowly scoped runtime functions.

A vector-interpreter extension implements PCMPISTRM/VPCMPISTRM string masks. It handles
implicit string ends, signed byte/word ranges, comparison modes, polarity and flags. The JIT
uses its existing interpreter fallback for these instructions; that fallback is compiled for
ARM64. SSE/VEX upper-register behavior and later translated flag consumers are tested.
All other vector operations delegate to the unchanged pinned implementation.

The loader reads a private image prepared from the user's executable. In linked mode it also
initializes the original libc and Fios2 modules from the same input. Unknown imports stop
execution with an explicit diagnostic instead of receiving blanket success results.

## Runtime adapters

The adapters reuse the pinned upstream platform, mutex, thread metadata, memory and file
backends. SotC-specific namespace mappings are explicit. Checked guest thread creation, join,
detach and exit use native pthreads and separate low guest stacks. Each thread has its own
static/module TLS, DTV table, errno and inherited FPU control state. Creation and lifecycle
claims are serialized; a blocking join does not hold that serialization lock. Competing
joins cannot both claim the same thread. Thread-key APIs remain unresolved.

The thread adapter includes the pinned backend unchanged and wraps thread start, TCB attachment
and lifecycle calls. A private build copy changes only the mutex/condition diagnostic counter
declarations to atomic types. The source checkout and its synchronization algorithms are
unchanged. Read-only filesystem operations are serialized around the backend descriptor table,
including close versus read; parallel asynchronous I/O performance is not claimed.

The non-timed kernel read/write-lock family reuses the upstream ownership-aware pthread backend.
Tests call it through synthetic x86 bridges from several host threads. Kernel relative-timeout
calls remain unresolved until their distinct timeout ABI has a dedicated adapter.

FIFO event flags use native mutexes and condition variables. A protected registry validates
opaque handles without dereferencing guest-supplied values. AND/OR tests use all 64 bits;
successful waits report the pattern before applying their selected clear mode. Explicit clear
takes a keep-mask. Queued waiters consume matching bits in FIFO order, while unmatched earlier
waiters do not prevent later matches.

Cancel and delete remove pending waits under the same lock, preserving an already completed
wait's result. Deletion removes the handle immediately and releases the object after all active
waiters return. Handles are not reused. Relative microsecond waits use the upstream platform's
monotonic condition-variable helper and update the remaining duration after blocking. Priority
queues return ENOTSUP; named open/close remain unresolved.

Worker completion releases module TLS/DTV storage. Explicit exit resets abandoned CPU call depth
before cleanup. Exit through a live fiber or trap frame stops rather than discarding those
frames silently. Stacks, TCBs and diagnostic CPU records retain upstream process lifetime.

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

## System modules and DiscMap

The module registry tracks references to the implemented fiber library and, when read-only
content is mounted, the local-content DiscMap provider. Its internal loader preserves 32-bit
module IDs. The five-argument loader checks the reserved field, accepts only empty initialization
arguments, and writes the initializer result only after a successful load. Missing modules
return an unavailable result.

The host provider does not load PS4 optical mapping tables. Bitmap-availability queries return
the no-bitmap error without changing outputs. The request-location query validates required
pointers and signed ranges before writing. Valid requests initialize three 64-bit outputs to
unavailable values; unsupported namespaces return a location error, while recognized disc
namespaces return a missing-mapping-entry error. No physical placement is invented and no
filesystem lookup is implied by that result.

Synthetic x86 calls verify the loader ABI, reference counts and both kinds of query failure
in JIT and interpreter. Request-location tests check output guards, aliasing, full-width values,
invalid ranges and errno preservation. The same request cases were checked privately against
the supplied original DiscMap function with empty mapping tables. Generated inspection data
remains outside Git.

## Limits

CommonDialog initialization reports NOT_SUPPORTED because this diagnostic has no dialog
provider. MsgDialog initialization then reports its missing system dependency. Both are
no-argument functions; register residue is not treated as a parameter pointer. Neither call
activates a module or claims a dialog result. Other dialog APIs remain unresolved.

Network initialization also has a no-argument ABI. Because this diagnostic has no network
backend, sceNetInit returns the encoded libSceNet EOPNOTSUPP error without changing kernel/libc
errno. Repeated calls do not create initialized state, sockets or pools. Other network APIs
remain unresolved. This is an explicit diagnostic unavailability policy, not networking support.

A separate UserService adapter maintains an ephemeral local console session. Successful
initialization makes user 1 ("Wander") available and queues one login event; a second
initialization returns an error. The priority parameter is validated and recorded without
creating a background worker. ID/list/name queries and event consumption share a mutex with
initialization and termination. Only a successful event query consumes the pending login.
Termination clears session state. The profile has no host-account, PSN or storage connection,
and the other UserService functions remain unresolved.

ShareUtility Ex2 has a narrow failure adapter. It validates the full-width mode and minimum
heap size, then reports a service resource failure before examining thread attributes.
This matches the original module's early failure path with an unavailable allocation;
it does not initialize sharing, capture media, create a UI or resolve the share-menu API.

GNM owner/resource registration uses the retail module's fixed-error profile. Both calls
leave output handles untouched and do not examine names or resource descriptors. The
resource call's seventh stack argument is not forwarded because this error-only provider
uses no arguments. No registry, command submission or renderer is initialized.

Compute-queue mapping validates the retail pipe/queue bounds, ring-size bit test and
address alignment, then reports COMPUTEQUEUE_INTERNAL. There is no GPU backend, so the
adapter creates no queue and leaves the ring and read pointer untouched. Validating this
failure path does not implement asynchronous GPU compute or command submission.

The temporary high image mapping does not validate SotC's packed static pointers. CPU progress
does not establish renderer correctness, game content loading, saving or playability. Those
need separate implementation and end-to-end checks.
