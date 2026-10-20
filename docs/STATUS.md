# Development status

Updated 2026-10-20. **CPU bootstrap research; not playable.**

| Area | Verified scope |
|---|---|
| Host CPU | ARM64-only diagnostic and bbcpu library; no Rosetta |
| Upstream checks | Native emitter and JIT fault recovery checks passed |
| CPU string masks | PCMPISTRM/VPCMPISTRM: 61,440 synthetic cases per mode against a scalar model; native diagnostic JIT fault checks pass |
| Runtime contracts | Scoped resolution, capability gate, heap/realloc, recursive mutex lifecycle and destructor callbacks |
| Read/write locks | Non-timed kernel backend: synthetic guest lifecycle/errors, shared readers, exclusive writes and ownership checks pass in both modes |
| Memory | Largest-free-range queries against an independent bitmap; real direct aliases, release and flexible availability verified in normal Terminal |
| Console | x86-to-ARM64 writes, descriptor isolation, embedded NUL and guest errors |
| Clocks | TSC/API agreement, clock ABI, real sleeps and realtime |
| Timezones | UTC/local round trips across four zones, seasonal/half-hour DST and a transition boundary |
| Content API | Fixture ABI/error tests; normal Terminal confirms four initial game-file reads totaling 1,788 bytes in both modes |
| Original modules | libc and Fios2 initializers returned zero in JIT and interpreter |
| Fibers | Synthetic ABI/context/lifecycle tests in both modes; actual startup creates 160 fibers and records 164 runs, with no direct switches observed |
| Module registry | Reference-counted fiber and local-content DiscMap providers; five-argument internal-load ABI, errors and output preservation tested in both modes |
| DiscMap | Bitmap queries report unloaded tables; request-location checks cover six-argument ABI, signed ranges, namespaces, 64-bit failure outputs and errno preservation in both modes |
| Guest threads | Synthetic return/exit/join/detach, competing joins, separate TCB/static/module TLS and errno, DTV, low stacks, inherited FPU, condition waits and concurrent read-only I/O pass in both modes |
| Event flags | Synthetic FIFO/single/multiple waits, 64-bit AND/OR patterns, clear modes, cancellation/deletion with blocked waiters, stale handles, timeout units and errno pass in both modes |
| Dialog availability | No-argument CommonDialog/MsgDialog calls return explicit unsupported/dependency errors; no UI, module activation or completed dialog is claimed |

The latest paired normal Terminal baseline passes the string-mask instruction and enters
the game's job initialization in both modes. It creates three Fios2 workers and four game job workers,
12 event flags and 160 fibers. The counters record four event waits and 164 fiber runs, with
no direct fiber switches or event sets. Four initial content files totaling 1,788 bytes are
read without writes. Both modes stop at the same CommonDialog initialization call on main.

The PCMPISTRM/VPCMPISTRM extension covers every control-byte mode, implicit terminators,
signed/unsigned byte and word comparisons, output masks and arithmetic flags. Legacy SSE keeps
the upper YMM0 half; VEX clears it. The JIT uses its existing synchronized interpreter fallback
for these instructions. Other vector operations still use the unchanged pinned implementation.

There are 61,440 synthetic cases per CPU mode, with an independent comparison-matrix oracle,
closed-form self comparisons, mixed-sign ranges, boundary matches, register aliasing and
unaligned memory. Translated flag consumers, preserved source registers and MXCSR are checked.
The native emitter and JIT fault checks pass with the adapted CPU archive. These are synthetic
checks, not a native x86 hardware comparison. Normal Terminal confirms progress past the
instruction previously unsupported during game initialization.

Non-timed kernel read/write-lock calls now reuse the upstream backend, with synthetic guest
lifecycle, shared-reader, exclusive-writer, ownership and busy-destroy checks. The kernel
relative-timeout NIDs stay unresolved because their argument contract differs from the upstream
absolute-timespec helpers.

Checked thread creation, return, exit, join and detach are now connected to native pthreads.
Dynamic module TLS and DTV tables are isolated per thread; module TLS allocations are released
when a worker finishes. Synthetic guest workers test this concurrently, including two callers
competing to join one worker. Original libc/Fios2 initializers still return zero in both modes.
The normal Terminal baseline confirms Fios2 and game-worker creation and initial event waits;
subsequent scheduling, completion and resource churn remain unverified. Tool execution still
rejects shared-memory backing with EPERM.

The internal loader now preserves full-width module IDs and validates its reserved argument.
Its supported empty-argument profile loads the implemented fiber provider or, with a read-only
content mount, the DiscMap provider. Other modules and initialization arguments remain unavailable.
This provider does not load PS4 optical mapping tables. Bitmap-availability queries return the
no-bitmap error and leave output buffers untouched. The request-location query has a separate
contract: null required pointers or invalid ranges leave outputs unchanged; valid requests initialize three
64-bit outputs to unavailable values, then report an unmapped namespace or missing mapping
entry. A missing mapping entry does not mean the ordinary content file is absent.

The request-query cases were also run against the supplied original DiscMap function with
empty mapping tables in both CPU modes. This isolated check does not initialize the original
module or run the game. Full synthetic checks pass; both original libc/Fios2 initializers still
return zero. Normal Terminal now confirms progress past this query into initial content reads.

FIFO event flags are implemented with native mutex/condition waits and a protected registry.
Waiters receive the full pattern before clear operations; FIFO consumption determines which
matching waiters complete. Cancellation and deletion wake blocked waits with distinct errors.
Opaque handles are never reused; deleted objects are released after their last active waiter.
Relative microsecond timeouts use monotonic deadlines. Priority-ordered queues return ENOTSUP;
named open/close remain unresolved. Normal Terminal confirms creation and blocking waits;
game-level signal, cancellation, timeout and deletion paths remain unverified.

CommonDialog and MsgDialog initializers use a no-argument ABI. This diagnostic has no dialog
provider: CommonDialog returns NOT_SUPPORTED and message initialization returns
NOT_SYSTEM_INITIALIZED. Repeated calls preserve that unavailable state and do not load either
module. Other dialog APIs remain unresolved. Synthetic guest checks cover stale argument
registers, preserved buffers/errno and module availability in both modes. This error profile's
effect on subsequent game initialization needs a normal Terminal run.

Thread keys and their destructors are unavailable and remain unresolved. Exiting through a live
fiber or trap frame stops explicitly. Guest stacks, TCBs and diagnostic CPU records retain the
upstream process-lifetime allocation policy; long-running thread churn is not validated.

## Next verification

Run `bash packaging/test-arm64.sh --linked-test` in a normal native Terminal. Compare both
stops, original module initialization, memory accounting and actual file counters. Initial
small-file reads do not establish archive/asset loading or gameplay.

## Remaining work

- Resolve the game's packed static-image pointer/address requirements.
- Implement additional required system services with checked ABI and error behavior.
- Validate further Fios2 worker behavior and subsequent game threading; add missing thread-key contracts
  and complete lifecycle/resource reclamation where needed.
- Validate archive/asset loading beyond the first small game-content reads.
- Integrate and validate graphics, audio, input and a SotC launcher.
- Add save support with a separate tested workflow when gameplay is possible.

The diagnostic budget is deliberately bounded. No release, gameplay compatibility or FPS
claim is implied by initialization progress.
