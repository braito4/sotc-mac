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
| Content API | Fixture ABI/error tests; normal Terminal confirms 17 content opens and 37 reads totaling 80,002,009 bytes, without writes, in both modes |
| Original modules | libc and Fios2 initializers returned zero in JIT and interpreter |
| Fibers | Synthetic ABI/context/lifecycle tests in both modes; actual startup creates 160 fibers and records 164 runs, with two direct switches observed |
| Module registry | Reference-counted fiber and local-content DiscMap providers; five-argument internal-load ABI, errors and output preservation tested in both modes |
| DiscMap | Bitmap queries report unloaded tables; request-location checks cover six-argument ABI, signed ranges, namespaces, 64-bit failure outputs and errno preservation in both modes |
| Guest threads | Synthetic return/exit/join/detach, competing joins, separate TCB/static/module TLS and errno, DTV, low stacks, inherited FPU, condition waits and concurrent read-only I/O pass in both modes |
| Event flags | Synthetic FIFO/single/multiple waits, 64-bit AND/OR patterns, clear modes, cancellation/deletion with blocked waiters, stale handles, timeout units and errno pass in both modes |
| Dialog availability | No-argument CommonDialog/MsgDialog calls return explicit unsupported/dependency errors; no UI, module activation or completed dialog is claimed |
| Network availability | No-argument sceNetInit returns an explicit EOPNOTSUPP error; guest ABI, scopes, repeated errors and kernel/libc errno preservation tested in both modes |
| Share availability | Ex2 mode/heap-size validation and explicit service resource failure; no capture, sharing UI or initialized state |
| GNM registration availability | Retail fixed failures for owner/resource registration, with unchanged outputs and errno; no graphics state |
| Local users | Ephemeral single-user registry; guest initialization/termination, priority and lifecycle errors, guarded ID/list/name outputs and concurrent one-shot login events tested in both modes |

The latest paired normal Terminal baseline creates three Fios2 workers, four game job
workers and one resource-loading worker in both modes. There are 19 event flags and 160
fibers, with 11 event waits, ten event sets, 164 fiber runs and two direct fiber switches.
The counters record 17 content opens and 37 reads totaling 80,002,009 bytes without writes;
the game reports its resource dependency file loaded. Both modes pass the CommonDialog,
network and ShareUtility failures, initialize the local user service with priority 700,
and stop at the same GNM owner-registration call. One CommonDialog request, no message-dialog
request, one network request and one share initialization request are recorded per mode.

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
the latest run also confirms a resource-loading worker and initial event signals/direct
fiber switches. Subsequent completion and resource churn remain unverified. Tool execution still
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
the latest baseline records ten game-level event sets. Cancellation, timeout and deletion
paths in the game remain unverified.

CommonDialog and MsgDialog initializers use a no-argument ABI. This diagnostic has no dialog
provider: CommonDialog returns NOT_SUPPORTED and message initialization returns
NOT_SYSTEM_INITIALIZED. Repeated calls preserve that unavailable state and do not load either
module. Other dialog APIs remain unresolved. Synthetic guest checks cover stale argument
registers, preserved buffers/errno and module availability in both modes. Normal Terminal
confirms progress past the CommonDialog error into network initialization; message initialization
has not been called on this startup path.

The diagnostic has no network backend. Its no-argument sceNetInit response is the encoded
libSceNet EOPNOTSUPP error, not a successful initialization or a reported connection state.
Repeated calls remain unsupported, ignore stale argument registers and preserve kernel/libc
errno and buffers. Other network APIs remain unresolved; no sockets, pools, module references
or initialized state are created. Synthetic guest ABI and scope checks pass in both modes.
Normal Terminal confirms progress past this error into additional content reads and UserService
initialization. This does not establish networking support.

The new UserService adapter implements a process-local console profile: user ID 1, name
"Wander", and one pending login event after each successful initialization. It uses no host
account, PSN identity, user directory or save storage. Initialization validates the single
32-bit priority field, accepts a default parameter, rejects duplicate initialization and
records the priority without creating a service worker. Termination clears the session and
pending event. Queries before initialization, unknown users, short buffers and empty event
queues report distinct errors while preserving outputs. A mutex protects lifecycle, queries
and event consumption. The remaining UserService APIs stay unresolved.

Synthetic x86 calls cover the four-byte parameter at a protected page boundary, priority
bounds, reinitialization, exact ID/list/event output widths, NUL-terminated names and 64-bit
buffer sizes. Eight concurrent guest callers test single-owner initialization, exactly-once
event consumption and repeated queries with separate errno values. Full contracts pass in
both modes. Normal Terminal confirms one successful initialization with priority 700 in
each mode; no user queries or login-event consumption have occurred yet.

Thread keys and their destructors are unavailable and remain unresolved. Exiting through a live
fiber or trap frame stops explicitly. Guest stacks, TCBs and diagnostic CPU records retain the
upstream process-lifetime allocation policy; long-running thread churn is not validated.

ShareUtility Ex2 now checks the full-width mode and heap-size arguments. Mode 1 with at
least 128 KiB reaches an explicit service resource failure; other prefixes return the
original early validation error. This diagnostic has no sharing backend. It fails before
thread-argument validation and never dereferences the final attributes pointer, creates
initialized state or exposes the share menu. Synthetic guest calls check boundary and
high-bit values, repeated errors, untouched buffers/errno and scoped capability handling.
The supplied original module also matches 75 early validation/resource-error cases per
CPU mode with an injected failed mapping dependency; that isolated check does not start
the original service. Normal Terminal confirms progress past this failure in both modes. Subsequent sharing
behavior remains unimplemented.

The retail GNM module returns the same fixed failure for owner and resource registration,
without reading arguments or modifying output handles. The adapter reproduces that narrow
behavior and counts requests. It creates no registry or graphics state. Synthetic guest
checks cover capability/scopes, repeated calls, invalid pointers, unchanged buffers/errno
and resource calls with a seventh stack argument. Original-module checks pass the same
cases in both CPU modes. This is not graphics submission or rendering support.

## Next verification

Run `bash packaging/test-arm64.sh --linked-test` in a normal native Terminal. Compare both
stops, original module initialization, memory accounting, GNM registration requests and actual file
counters. Content reads do not establish successful archive/asset decoding or gameplay.

## Remaining work

- Resolve the game's packed static-image pointer/address requirements.
- Implement additional required system services with checked ABI and error behavior.
- Validate further Fios2 worker behavior and subsequent game threading; add missing thread-key contracts
  and complete lifecycle/resource reclamation where needed.
- Validate archive/asset decoding and subsequent content-loading behavior.
- Integrate and validate graphics, audio, input and a SotC launcher.
- Add save support with a separate tested workflow when gameplay is possible.

The diagnostic budget is deliberately bounded. No release, gameplay compatibility or FPS
claim is implied by initialization progress.
