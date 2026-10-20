# Development status

Updated 2026-10-21. **CPU bootstrap research; not playable.**

| Area | Verified scope |
|---|---|
| Host CPU | ARM64-only diagnostic and bbcpu library; no Rosetta |
| Upstream checks | Native emitter and JIT fault recovery checks passed |
| CPU string masks | PCMPISTRM/VPCMPISTRM: 61,440 synthetic cases per mode against a scalar model; native diagnostic JIT fault checks pass |
| Runtime contracts | Scoped resolution, capability gate, heap/realloc, recursive mutex lifecycle and destructor callbacks |
| Read/write locks | Non-timed kernel backend: synthetic guest lifecycle/errors, shared readers, exclusive writes and ownership checks pass in both modes |
| Memory | Largest-free-range queries against an independent bitmap; real direct aliases, release and flexible availability verified in normal Terminal and tool runs with explicit file backing |
| Console | x86-to-ARM64 writes, descriptor isolation, embedded NUL and guest errors |
| Clocks | TSC/API agreement, clock ABI, real sleeps and realtime |
| Timezones | UTC/local round trips across four zones, seasonal/half-hour DST and a transition boundary |
| Content API | Fixture ABI/error tests; latest paired tool run: 17 opens per mode; interpreter: 429 reads/81,536,320 bytes, JIT: 426 reads/81,529,921 bytes; no writes |
| Original modules | libc and Fios2 initializers returned zero in JIT and interpreter |
| Fibers | Synthetic ABI/context/lifecycle tests in both modes; actual startup creates 160 fibers and records 164 runs, with 1,109/1,073 direct switches observed (interpreter/JIT) |
| Module registry | Reference-counted fiber and local-content DiscMap providers; five-argument internal-load ABI, errors and output preservation tested in both modes |
| DiscMap | Bitmap queries report unloaded tables; request-location checks cover six-argument ABI, signed ranges, namespaces, 64-bit failure outputs and errno preservation in both modes |
| UUID generation | Scoped sceKernelUuidCreate uses macOS random v4 UUIDs in the native 16-byte uuidgen field layout; guest guards, errors, errno and concurrent non-repeating samples tested in both modes; real combined run passes UUID generation |
| Thread-specific keys | 255 process-wide keys with per-thread generation-tagged values; scoped POSIX/kernel APIs, deletion/reuse, errors and four-pass guest destructors tested in both modes; real combined run confirms one key created and two sets |
| Guest threads | Synthetic return/exit/join/detach, competing joins, separate TCB/static/module TLS and errno, DTV, low stacks, inherited FPU, condition waits and concurrent read-only I/O pass in both modes |
| User-event queues | Native lifecycle, level/edge triggers, coalescing, batching, getters, polling, const microsecond timeouts, blocked deletion and errno pass through the guest ABI in both modes; no graphics/timer filters |
| Event flags | Synthetic FIFO/single/multiple waits, 64-bit AND/OR patterns, clear modes, cancellation/deletion with blocked waiters, stale handles, timeout units and errno pass in both modes |
| Dialog availability | No-argument CommonDialog/MsgDialog calls return explicit unsupported/dependency errors; no UI, module activation or completed dialog is claimed |
| Network availability | No-argument sceNetInit returns an explicit EOPNOTSUPP error; guest ABI, scopes, repeated errors and kernel/libc errno preservation tested in both modes |
| Share availability | Ex2 mode/heap-size validation and explicit service resource failure; no capture, sharing UI or initialized state |
| Compute queue availability | Five-argument retail validation order and widths, then explicit backend failure; no queue created or read-pointer update |
| GNM registration availability | Retail fixed failures for owner/resource registration, with unchanged outputs and errno; no graphics state |
| File-backed host memory | Explicit temporary, unlinked, 0600/CLOEXEC shared backing; alias, clearing, release and linked startup checks pass |
| Archive/address audit | Six PSARC manifests, 130,238 members; nine synthetic archive/address tests; no full asset decoding or pointer-runtime replacement |
| Color application availability | Suffixed three-argument ABI; empty-display profile rejects handles before reading settings/size |
| Gamma preparation | Retail suffixed export: explicit 32-bit size plus float gamma, 12/16-byte layouts, three channels, pure data preparation |
| Display data preparation | Exact 40-byte buffer attributes with seventh stack argument; mode fill/size helper; isolated original-module comparisons and guest tests pass in both modes |
| Metal prerequisite | User reports PASS on Apple M1 in native Terminal: shader compilation, render pipeline, triangle draw and pixel readback; tool process still exposes zero Metal devices |
| GPU library build | Pinned renderer and dependencies compiled and linked as ARM64; loader exports verified; five FFmpeg libraries load natively; sixteen offline build tests pass, including spaced paths and CMake cache recovery; no renderer launch or game frame |
| GPU native loading | Actual ARM64 library load, six API exports and unload pass; all 46 runtime/CPU imports present; guest callbacks and clock/statistics adapters tested; no game renderer integration |
| Vulkan surface prerequisite | Bundled MoltenVK manifest and SDL loader selected explicitly; native enumeration exposes 19 extensions including Vulkan/Metal surfaces; five tests pass, including a real missing-driver regression |
| Renderer initialization | User confirms native Terminal initialization, guest-memory residency, four cached Metal pipelines and Metal command-buffer processing; no game draws/dispatches in this isolated check |
| Original NGS2 module | Graphics command privately links supplied libSceNgs2; its initializer returns zero in both modes; provider activation requires executable native bindings and initializer success; combined audio setup and sound output unverified |
| Original gesture module | Graphics command privately links supplied libSceSystemGesture; module startup and isolated initialize/open/close/finalize pass in JIT/interpreter; real combined run passes initialization and opens the keyboard pad; touch recognition and game frames remain unverified |
| Keyboard input profile | SDL readiness checked on main thread; six scoped pad APIs implement a keyboard-backed logical controller; lifecycle, input/state/packet layouts and loader readiness tests pass in JIT/interpreter; real combined run confirms initialization and open; reads remain unverified |
| Optional graphics/game connection | Scoped VideoOut/GNM routing, one event-queue provider, main-thread window and guest worker; original synthetic linked fixture passes in JIT/interpreter, including fault recovery; real combined run passes color/capability operations and starts RenderFlip/Game Render, passing original gesture initialization and keyboard pad open; save/trophy tasks, thread keys and UUID generation pass; latest failure writes through a null audio object after rejected NGS2 load; original NGS2 provider awaits combined verification |
| VideoOut availability | Four-argument open ABI, supported main-output profile, explicit NO_DEVICE; no window, port or handle |
| Display safe area | Fixed logical ratio 1.0, exact four-byte output, reserved bytes preserved, null-pointer error; no host display |
| Local users | Ephemeral single-user registry; guest initialization/termination, priority and lifecycle errors, guarded ID/list/name outputs and concurrent one-shot login events tested in both modes |

The latest paired tool run uses explicit temporary-file backing and reaches the same
`scePadInit` call in JIT and interpreter. Each mode creates three Fios2 workers, four job
workers, one resource-loading worker, RenderFlip and Game Render (ten guest threads plus
main), 19 event flags, one event queue and 160 fibers, with 164 fiber runs. A flip-event
queue wait begins, without any fabricated notification. The interpreter records 1,109 direct
switches and 429 reads/81,536,320 bytes; JIT records 1,073 switches and 426 reads/81,529,921
bytes. Counters vary with scheduling. Both open 17 content files without writes. Both reject
three owner registrations, 80 resource registrations, seven compute queues, one color update,
two flip-rate changes, one capability query, one flip-event registration, one display-buffer
registration and one mode configuration. Safe area, gamma, buffer attributes and mode-data
preparation each run once; no output device or renderer exists.

The user reports that the Metal prerequisite check passes on Apple M1 in native Terminal,
including GPU draw/readback. This tool process still reports no default device and zero
enumerated devices. The isolated upstream GPU library has now compiled and linked as ARM64,
with its required loader exports checked and unresolved runtime/CPU symbols inventoried.
Zydis compiler flags, FFmpeg generated shell/linker metadata and installed pkg-config paths
now preserve spaces in the project path. CMake can recover previously cached broken dependency
paths. All five installed FFmpeg shared libraries passed native loading/version checks;
sixteen offline builder tests pass. The GPU library targets macOS 15 or later.
The user has now confirmed renderer initialization in Terminal, including Metal command-buffer
encoding with zero game draws/dispatches. The optional graphics/game connection is implemented
and checked with original synthetic fixtures. A real combined Terminal run now passes color/capability operations, starts RenderFlip and
Game Render, passes original touch-recognizer initialization and opens the keyboard-backed
controller. Save and trophy task threads then start; one thread key is created and set
twice, and UUID generation now passes. The latest real combined run then rejects NGS2
module loading and faults while writing through a null audio configuration object. The
original NGS2 module is now linked for the next run, with availability published only after
its actual initializer returns zero. This is an audio-startup dependency failure; no CPU
translator defect has been established by that fault.
Starting a save/trophy task does not establish save access or trophy support. No game frame is verified.
See [Graphics bring-up](GRAPHICS_BRINGUP.md) for the remaining loader integration.

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
fiber switches. Subsequent completion and resource churn remain unverified. Tool execution still rejects named POSIX shared-memory creation, but explicit
`SOTC_MEMORY_BACKING=file` now preserves shared mappings through an unlinked file and
allows the linked diagnostic to run. The default named-shared-memory backend is unchanged.

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
the latest baseline records 1,404/2,095 game-level event sets (interpreter/JIT). Cancellation, timeout and deletion
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

The optional graphics image now also links the supplied original `libSceNgs2.sprx`.
Its initializer returns zero in JIT and interpreter with the existing original libc. It
is a static resident provider for module ID 0x000B: load/is-loaded/unload maintain references,
while code remains mapped and its initializer runs once. The loader identifies it using two
scoped, executable guest export bindings in the same linked module, then publishes availability
after successful initialization. Merely naming an import or supplying a data/non-executable
binding cannot enable it; missing or wrongly scoped bindings leave it unavailable, and
inconsistent modules or failed initialization abort before game entry. Synthetic loader tests
cover those cases in both modes and verify reference lifecycle and rejection during initialization.
The default headless test still has no NGS2 provider. No silent system/rack/voice stand-ins,
NGS2 event fabrication, audio output device or sound playback is introduced. AJM/AudioOut
services remain unresolved until implemented. Fatal fault diagnostics now print the signal
and fault address before the CPU registers; recovered tracking/speculative faults stay silent.

UUID creation uses macOS `uuid_generate_random`, then converts the RFC byte order to the
native little-endian 32/16/16-bit uuidgen fields. The remaining eight bytes retain their
order. This diagnostic uses random version-4/standard-variant IDs; it does not reproduce
the console's UUID generation policy or expose a hardware identifier. The supplied
original wrapper confirms a single UUID output and EINVAL for null. Synthetic guest tests
cover an exact 16-byte output, unaligned/guarded buffers, native version/variant fields,
null and scope errors, unchanged guest errno and 1,024 concurrent non-repeating samples per
CPU mode. Sample non-repetition is a regression check, not a proof of global uniqueness.

Thread-specific keys now implement scoped POSIX and kernel create/delete/set/get APIs.
The profile has 255 live key slots with 32-bit IDs and full-width per-thread pointer values.
Destructors must be null or registered guest code; native/unknown addresses are rejected.
Registry synchronization and internal generations prevent deleted/reused slots inheriting
values or destructors from another thread. Key deletion never runs a destructor. Guest-thread
return and explicit exit reset abandoned CPU frames, invoke non-null guest destructors with
the value cleared first, and keep static/module TLS and errno alive through all callbacks.
Rearming is bounded to four passes; remaining per-thread values are then discarded. Null
values do not invoke callbacks. Callback selection occurs under the registry lock and
execution occurs outside it, allowing a destructor to use the key APIs.

Synthetic tests in both CPU modes cover exact four-byte guarded/unaligned output, full-width
values and 32-bit key arguments, POSIX versus kernel errors, errno preservation, 255-key
exhaustion and cross-thread delete/recreate isolation. Real guest workers cover return,
explicit exit, join and detached completion, live TLS/errno inside guest destructors, value
clearing, repeated callbacks and the four-pass limit. Recursive thread exit from a destructor
stops explicitly (exit 23), with negative checks in both modes. Host-owned thread records
retain their process-lifetime policy; their automatic key teardown is not implemented.

Exiting through a live fiber or trap frame stops explicitly. Guest stacks, TCBs and diagnostic CPU records retain the
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
cases in both CPU modes. Normal Terminal confirms three owner-registration calls and no
resource-registration calls before compute-queue mapping. The latest baseline now
confirms 20 resource-registration calls per mode before VideoOut open. This is not rendering support.

Compute-queue mapping now validates the five-argument ABI in the supplied retail module's
order: pipe, queue, ring size, ring alignment, then read-pointer alignment. Pipe/queue/size
use 32-bit arguments and addresses retain their full width. The retail bit test allows
zero size to reach the backend; the adapter preserves that early behavior instead of
claiming a usable zero-size queue. Valid prefixes return COMPUTEQUEUE_INTERNAL because
there is no GPU backend. No queue ID is invented, and neither address is dereferenced or
modified. Synthetic guest tests cover 2,800 combinations, repeated calls, high bits,
validation precedence, null/aligned/unaligned addresses and errno preservation per mode.
The same 2,800 cases pass against the supplied original module with an injected failed
GPU ioctl in both modes. Normal Terminal confirms seven compute-queue requests in both modes and subsequent
progress to the display-safe-area query; no operational queue is implied.

The display-safe-area query now exposes a fixed logical-viewport profile: ratio 1.0.
The 132-byte guest structure contains a four-byte float followed by 128 reserved bytes;
only the float is written. A null pointer returns the system-service parameter error.
This does not query a host display, create a window or initialize rendering. Synthetic
guest checks cover scope/capability, repeated and unaligned outputs, guards around the
whole structure, reserved fields on a protected page and errno preservation. The same
checks pass against the supplied original export with an injected full-area setting
provider in both modes. Other SystemService APIs remain unresolved. Normal Terminal confirms one
query per mode and subsequent progress to VideoOut open.

VideoOut open now has a deliberately headless availability profile. Its four-argument ABI
uses 32-bit user, bus and index fields and a full-width service-options pointer. MAIN bus,
index zero returns NO_DEVICE. Other buses return UNSUPPORTED_OUTPUT_MODE under this
profile, without asserting that the retail API lacks those modes; a nonzero index on
MAIN returns INVALID_VALUE. No options pointer or user record is read because no device
exists. No window, output port, display handle, flip event or initialized state is created.
Guest tests cover 32-bit argument widths, system/local user values, repeated failures,
untouched options/errno and library/capability isolation in both modes. Close, buffer
registration, resolution and flip APIs remain unresolved. Normal Terminal confirms one failed open per mode and subsequent gamma preparation;
opening a working display still requires a graphics backend.

The suffixed gamma-preparation export now uses the supplied retail ABI: a settings pointer,
an explicit 32-bit size and a scalar float in XMM0. It accepts 12 or 16 bytes, zeros that
extent and writes the input float to three channels. This differs from the simplified
shadPS4 declaration; the adapter follows the supplied module. Null output, invalid size
or out-of-range gamma returns INVALID_VALUE without changing outputs. Ordered comparisons
preserve the original NaN acceptance/bit patterns. The helper only builds a caller-owned
structure; it does not require a valid output handle or apply display settings.

Synthetic guest checks cover mixed integer/float arguments, size truncation, exact lower
and upper range boundaries, infinities/NaNs, repeated calls, unaligned buffers, protected
page boundaries, invalid/null outputs and errno preservation. The same cases pass against
the original export in both CPU modes. Normal Terminal confirms one gamma preparation per mode followed by color application.

The suffixed color-application export takes a 32-bit handle, settings pointer and explicit
32-bit size. With no successful VideoOut open, every handle is invalid. The adapter
returns INVALID_HANDLE before inspecting the settings pointer or size, including when
passed NO_DEVICE from the failed open. Original-module checks with an empty handle table
and isolated synchronization dependencies match this precedence in both modes. Guest
tests cover repeated errors, protected/null settings, failed-open values and errno.
The module test's trampolines now register their shared code page once to avoid exhausting
the CPU's range table; no runtime range limit was changed. Full contracts pass in both
modes. Paired tool runs pass this rejection and proceed through flip-rate and capability queries.

Flip-rate and the suffixed device-capability query now reject invalid display handles
before validating rates or accessing output structures. Original retail exports with empty
handle tables match the failure paths in both modes. No supported capabilities or frame
rate are invented. Full linked tests pass with file backing and now proceed through real
queue creation and explicit flip-event rejection to buffer-attribute and mode preparation.

User-event queues use a protected native registry and monotonic condition waits. Level
notifications remain ready; edge notifications clear when returned. Repeated triggers
coalesce with the latest user data, while re-registration preserves readiness and the
original flags and resets user data, following the inspected shadPS4 implementation.
Batches follow registration order. Timeout input is not modified; zero polls immediately.
Deletion removes the handle immediately, wakes blocked callers with EBADF under the local
lifetime policy, and frees the queue after its final waiter. Handles are opaque, full-width
and never reused. The accepted 32-byte name length follows the reference implementation;
its comment describes a tighter terminator-inclusive bound, so retail boundary compatibility
remains unverified. Null output pointers receive explicit errors. Synthetic guest tests cover
guarded layouts, invalid/stale handles, signed IDs, level/edge contention, deletion, getters,
queue isolation and errno preservation in both modes. Actual startup creates one queue;
startup now also begins one blocking wait from RenderFlip; game-triggered notifications
remain unverified. Timer, graphics, descriptor
filters and POSIX kevent remain unresolved.

VideoOut flip-event registration rejects invalid display handles before queue validation,
following shadPS4's port-first check. Guest tests include valid native user queues, invalid
queue values and protected user-data pointers; rejection leaves the queue empty. No display
notification is registered and no frame completion is synthesized.

The supplied reference ZIP review produced independent archive/address checks and an
optional host-memory adaptation, not imported source. See [Reference review](REFERENCE_REVIEW.md).
The six-archive manifest audit and synthetic malformed/truncated/compressed/case-collision
fixtures pass. Checked pointer arithmetic remains diagnostic-only; static-image pointers
are still unresolved. File backing passes both-view aliasing, zeroing, post-close mapping,
permissions, unlinked lifetime and close-on-exec tests, plus real direct/flexible memory
checks in the linked suite. No game content or persistent save is modified.

## Next verification

Renderer-only initialization has passed in native Terminal after selecting the bundled
MoltenVK driver. Next run `bash packaging/test-arm64.sh --graphics-game-test` there to
exercise the optional combined path. This mode keeps the window on the host main thread,
executes linked modules/game entry on a worker and routes reviewed graphics/queue imports.
It retains read-only content, private diagnostics and explicit unknown-import stops.
The latest real combined run passes the color/capability adapters and starts the render
threads, passes original gesture initialization and opens the keyboard-backed controller,
then starts save/trophy tasks, creates/uses a thread key and passes UUID generation. The
latest run faults in an audio configuration write after NGS2 load is rejected. The new
original NGS2 provider passes initialization and strict loader lifecycle tests in both
modes; its use together with real game entry/rendering awaits the next Terminal run. No
keyboard sample or sound output has been verified. The separate private graphics image preserves original executable bytes;
unsupported sensors, vibration and physical gamepads remain unresolved.

The combined path passes synthetic JIT/interpreter checks, including linked initialization,
scoped VideoOut routing, main/worker thread separation and speculative-read fault recovery.
Queue-ownership tests and the full default headless contracts pass; the paired real-game
headless test still stops at the same `scePadInit` call. The tool process's graphics/game
attempt stops at its Metal prerequisite (exit 24). Combined game execution is confirmed through the input-initialization boundary; no game
presentation or game frame has been verified.

Run `SOTC_MEMORY_BACKING=file bash packaging/test-arm64.sh --linked-test`
in the tool environment, or use the default backend in a normal native Terminal. Compare both
stops, original module initialization, memory accounting, GNM requests, logical safe-area queries, VideoOut requests and actual file
counters. Content reads do not establish successful archive/asset decoding or gameplay.

## Remaining work

- Resolve the game's packed static-image pointer/address requirements.
- Implement additional required system services with checked ABI and error behavior.
- Validate further Fios2 worker behavior and subsequent game threading, host-owned key teardown
  and lifecycle/resource reclamation where needed.
- Validate archive/asset decoding and subsequent content-loading behavior.
- Integrate and validate graphics, audio, input and a SotC launcher.
- Add save support with a separate tested workflow when gameplay is possible.

The diagnostic watchdog defaults to 30 seconds, with optional `SOTC_DIAGNOSTIC_SECONDS=1..60`.
Longer startup in the interpreter exhausted the earlier five- and fifteen-second limits;
with 30 seconds both CPU modes reach controller initialization. The watchdog is never disabled.
Malformed, zero and out-of-range settings are rejected before input is opened.
The diagnostic budget is deliberately bounded. No release, gameplay compatibility or FPS
claim is implied by initialization progress.
