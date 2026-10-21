# Development status

Updated 2026-10-21. **CPU and graphics bring-up; not playable.**

**Current incident: GPU testing resumes gradually at the owner's request after a
reported host watchdog kernel panic.** The latest instrumented comparison uses DMA
off, asynchronous submission off, JIT on and a 30-second budget. It enters the main
loop at about 1.63 seconds, then stops on an unresolved POSIX `usleep` import in the
original audio module. Its footer confirms child/supervisor status 20 after about
19.67 seconds, without a CPU-fault report, GPU-watchdog warning or cleanup error.
This is an import stop before the deadline, not a timeout or a graphics result.
The CSV retains 134 flip records over about 9.03 seconds from its own first record;
it does not establish image contents, total frame count or gameplay FPS. The prior
uninstrumented comparison showed only the first logo according to the owner; the
latest run's visual progression has not been reported. Narrow sleep/time aliases
now route both verified POSIX library scopes to the existing checked adapters.
The next comparison preserves the instrumentation, renderer settings and duration.
Dynamic shader reads stay blocked. See the
[current handoff](DEVELOPMENT_HANDOFF.md#gradual-gpu-testing).

| Area | Verified scope |
|---|---|
| Host CPU | ARM64-only diagnostic and bbcpu library; no Rosetta |
| Upstream checks | Native emitter and JIT fault recovery checks passed |
| CPU string masks | PCMPISTRM/VPCMPISTRM: 61,440 synthetic cases per mode against a scalar model; native diagnostic JIT fault checks pass |
| Runtime contracts | Scoped resolution, capability gate, heap/realloc, recursive mutex lifecycle and destructor callbacks |
| Read/write locks | Non-timed kernel backend: synthetic guest lifecycle/errors, shared readers, exclusive writes and ownership checks pass in both modes |
| Memory | Largest-free-range queries against an independent bitmap; real direct aliases, release and flexible availability verified in normal Terminal and tool runs with explicit file backing |
| Console | x86-to-ARM64 writes, descriptor isolation, embedded NUL and guest errors |
| Clocks | TSC/API agreement, clock ABI, real sleeps and realtime; POSIX sleep/time aliases checked through guest ABI in both library scopes, including microsecond width/duration, interruption/errno and guarded outputs |
| Timezones | UTC/local round trips across four zones, seasonal/half-hour DST and a transition boundary |
| Content API | Fixture ABI/error tests; latest paired tool run: 17 opens per mode; interpreter: 429 reads/81,536,320 bytes, JIT: 426 reads/81,529,921 bytes; no writes |
| Original modules | libc and Fios2 initializers returned zero in JIT and interpreter |
| Fibers | Synthetic ABI/context/lifecycle tests in both modes; actual startup creates 160 fibers and records 164 runs, with 1,109/1,073 direct switches observed (interpreter/JIT) |
| Module registry | Reference-counted fiber and local-content DiscMap providers; five-argument internal-load ABI, errors and output preservation tested in both modes |
| DiscMap | Bitmap queries report unloaded tables; request-location checks cover six-argument ABI, signed ranges, namespaces, 64-bit failure outputs and errno preservation in both modes |
| UUID generation | Scoped sceKernelUuidCreate uses macOS random v4 UUIDs in the native 16-byte uuidgen field layout; guest guards, errors, errno and concurrent non-repeating samples tested in both modes; real combined run passes UUID generation |
| Thread-specific keys | 255 process-wide keys with per-thread generation-tagged values; scoped POSIX/kernel APIs, deletion/reuse, errors and four-pass guest destructors tested in both modes; real combined run confirms one key created and two sets |
| Guest threads | Synthetic return/exit/join/detach, kernel/POSIX self aliases with matching main/worker identities, competing joins, separate TCB/static/module TLS and errno, DTV, low stacks, inherited FPU, condition waits and concurrent read-only I/O pass in both modes |
| User-event queues | Native lifecycle, level/edge triggers, coalescing, batching, getters, polling, const microsecond timeouts, blocked deletion and errno pass through the guest ABI in both modes; no graphics/timer filters |
| Event flags | Synthetic FIFO/single/multiple waits, 64-bit AND/OR patterns, clear modes, cancellation/deletion with blocked waiters, stale handles, timeout units and errno pass in both modes |
| Dialog availability | No-argument CommonDialog/MsgDialog calls return explicit unsupported/dependency errors; no UI, module activation or completed dialog is claimed |
| Network availability | No-argument sceNetInit returns an explicit EOPNOTSUPP error; guest ABI, scopes, repeated errors and kernel/libc errno preservation tested in both modes |
| NP availability | Scoped title/content-restriction setters return NP NOT_INITIALIZED before inspecting input pointers; supplied original comparisons, guest ABI/errors, untouched buffers/errno and graphics-loader continuation pass in both modes; no title data read or online state created |
| AppContent availability | Initialization and add-on listing preserve the unavailable module state and return the supplied original module-before-parameter error; guest/original checks pass in both modes; no license/package/DLC state or filesystem enumeration |
| Trophy availability | Six typed scoped exports return the supplied original uninitialized error before arguments/outputs; guest/original checks pass in both modes; no trophy contexts, handles, progress or files |
| Share availability | Ex2 mode/heap-size validation and explicit service resource failure; no capture, sharing UI or initialized state |
| Compute queue availability | Five-argument retail validation order and widths, then explicit backend failure; no queue created or read-pointer update |
| Indirect draw batch | Four scoped encoders match original/native packet matrices in both modes; two stage guards repair native over-acceptance; 5/7/9-argument guest ABI and 64-bit count addresses checked, no new game frame verified |
| Combined Metal activity | Terminal records one draw and 61 dispatches in a sample; shader-resource and BC1 format/usage errors remain; correct game image and presentation unverified |
| Save-dialog absent lifecycle | Five scoped exports preserve original NOT_INITIALIZED/null-result precedence and untouched outputs; guest/original checks pass in both modes; no dialog or save access |
| Flip wait and marker batch | Real VideoOut label query feeds checked retail wait packets; SetMarker native route and two corrected color encoders pass original/guest matrices in both modes; no completed wait or game frame verified |
| Graphics submission adapters | Four scoped submit/flip variants share retail preflight and actual backend calls; 5/6/9/10-argument guest routing, errors and original controlled-transport checks pass in both modes; real graphics submission/frame still requires Terminal verification |
| Batched graphics shader review | Five PS350/VS-update/VGT encoders match original and native GPU packet/ABI matrices in both modes, including null shader disable and register read bounds; actual shader execution/frame unverified |
| Batched compute review | Workload doorbell plus five related APIs reviewed together; scoped native routes and retail packet corrections pass guest/original checks in both modes; nonzero GPU queue execution remains unverified |
| GNM submission permission | Scoped query forwards the real provider lock state; allowed/blocked original/native guest comparisons, ignored arguments, errno/scopes and missing-export activation checks pass in both modes; no submission or frame |
| GNM default compute state | Scoped base-PS4 provider writes real register/barrier/NOP packets; byte-level original/native comparisons and short-size/guard/ABI checks pass in both modes; no submission or game frame |
| GNM compute shader packets | Scoped local modifier encoder preserves retail compute headers, register/mask semantics and untouched NOP tail; 3,072 guarded synthetic packets and original-module comparisons pass in both modes; no shader execution or draw |
| GNM debug markers | Scoped push/pop route to real pinned PM4 encoders; 256 synthetic lengths, byte-level packets/guards/errors and guest ABI match supplied originals and actual native GPU functions in both modes; no submission or draw |
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
| AudioOut streams/state/PCM | Scoped lifecycle, live SDL device-state queries and single/batch PCM submission; synthetic byte/channel ordering, batch ABI/errors, cadence, bounded queues/drains and loader integration pass in both modes; Terminal opens three real streams and passes state/self calls; Terminal accepts sustained PCM submissions without output errors; initialization and the startup script pass NetCtlInit; the main loop begins; audible playback unverified |
| Launch-splash availability | Scoped no-argument HideSplashScreen returns the supplied original module's absent launch-control error; repeated calls, ignored stale arguments, scopes, buffers/errno and loader continuation pass in both modes; Terminal continues after the response; no splash dismissal or game frame claimed |
| Original NGS2 module | Graphics command privately links supplied libSceNgs2; its initializer returns zero in both modes; provider activation requires executable native bindings and initializer success; real combined run passes its load and creates sndx_loader_thread; sound output unverified |
| Original gesture module | Graphics command privately links supplied libSceSystemGesture; module startup and isolated initialize/open/close/finalize pass in JIT/interpreter; real combined run passes initialization and opens the keyboard pad; touch recognition and game frames remain unverified |
| Keyboard input profile | SDL readiness checked on main thread; eight scoped pad APIs implement keyboard input and explicit light-bar/vibration unavailability; lifecycle, input/state/packet layouts and loader readiness tests pass in JIT/interpreter; real combined run confirms initialization, open and four input samples |
| Optional graphics/game connection | Native renderer initialization in Terminal; resource loading reaches the main loop and Shadow Game Update. Keyboard samples and sustained SDL PCM are recorded. Explicit unavailable responses for saves, trophy context and AppContent are passed. JIT reaches the main loop at 1.63 seconds and records a sample of 43 draws/303 dispatches; current observed boundary: GnmDrawIndirect; no game frame verified |
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
twice, and UUID generation now passes. The latest real combined run loads the original
NGS2 module, opens three actual AudioOut streams (normal main, restricted main and BGM)
without failures, starts the sound-output worker, passes two device-state queries and
passes the kernel/POSIX self alias. A stream-scheduler worker and SoundSystemSndx also start.
Terminal now confirms sustained PCM acceptance by SDL without output errors. The adapter
stages and validates the entire descriptor/PCM batch,
submits actual samples through SDL and returns the retail frame count. Both CPU modes test
sample bytes, legacy/standard channel ordering, batch errors, partial provider failure,
queue backpressure, null drains and one batch cadence. Audible sound has not been verified.
Terminal continues after the absent-launch-control response from HideSplashScreen. The
latest run passes `sceNpSetNpTitleId`; the adjacent optional content-restriction call
is also covered by its own typed adapter. Both return the supplied NP manager's
NOT_INITIALIZED error before inspecting either structure/pointer. No title ID/secret is
read, stored or logged, and no online/account state is fabricated. Synthetic guest and
loader tests pass in both modes; isolated supplied original exports confirm the same
uninitialized paths without original module initialization. Terminal confirms title-response
continuation; the optional restriction branch was not taken. The language query now passes
in Terminal; initialization and the startup script pass NetCtlInit; the main loop begins. This run no longer reproduces the audio configuration SIGBUS.

The new scoped state adapter queries the actual SDL device bound to a live owned stream.
It reports the logical primary route and available channel count, with the retail main/BGM
volume sentinel and no explicit reroutes. Missing device/query support or invalid device
information returns an error without changing output. Only the defined fields through byte
15 are written; the padding byte and reserved words are preserved. Guest tests cover guarded
and unaligned outputs, null/init/handle error order, all formats, device changes/failures and
closed streams in both CPU modes.
The private isolated original export matches the main/BGM format/channel matrix, defined
fields, reserved bytes and null/init/handle error order in both modes, using synthetic port
tables without original driver initialization. The actual graphics loader exercises open/state/close
through dynamically resolved SDL callbacks in both modes. Terminal confirms the state
adapter runs and SDL accepts PCM; audible playback remains unverified.

Fatal diagnostics retain ARM64 syndrome, signal code, host mapping protection and lock-free
GPU trap flags in either mode. Synthetic guest writes verify read-only-page faults and SIGBUS
from writable mappings beyond file backing; speculative-read recovery still passes. No host
memory protection is bypassed and no general memory-fault fix is claimed.
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

The scoped NP title and content-restriction setters also report an unavailable service.
Their two-pointer/one-pointer guest ABI and initialization-before-pointer/size validation
match the isolated supplied original exports in both modes. The title secret is never
inspected, copied or logged. Repeated calls, null/unreadable pointers, scopes, capability
gates, untouched input/errno and loader continuation are checked. No NP initialization,
sign-in, user-state callbacks, title registration or restriction configuration is claimed.

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

AudioOut is configured only in optional graphics mode. Main-thread SDL setup accepts a
ready CoreAudio backend; no dummy/disk sink or timer-only port is used. Local/system users
can open main/BGM streams at 48 kHz with 256-frame multiples through 2048. Eight formats and
the restricted attribute are supported. Eight main ports and one BGM port share the bounded
registry. Open publishes a non-reused handle only after actual stream creation/resume;
close releases the owned stream. Live device-state queries preserve retail reserved bytes.

Output/Outputs stage guest descriptors and samples before submission. Batch count/order,
duplicate handles, live ownership and equal buffer sizes are checked first. Legacy 8-channel
samples are reordered for SDL; standard layouts and mono/stereo retain their bytes. Actual
SDL submission must succeed. Cadence uses one monotonic deadline per batch; queue backpressure
allows at most two input buffers and fails after a bounded device stall. A null sample pointer
flushes/drains the stream without inserting silence. Return values count frames, not channels.
A provider failure after an earlier stream was accepted is reported as a partial failure;
accepted buffer/byte counters reflect only actual SDL acceptance. Volume updates remain
unresolved. No recording, dumps, silent fallback or fabricated playback progress is added.

Synthetic guest tests verify all formats, byte/channel order, guarded and unaligned pointers,
validation before submission, missing/erroring backends, partial submission, maximum-sized
batches, queue/drain behavior, cadence, ownership and errno in both modes. The synthetic
linked loader also executes single and batch PCM submission through dynamically resolved
SDL functions in both modes. An isolated supplied-module comparison confirms frame returns,
null-buffer behavior, raw copying and relevant validation order/errors with synthetic driver
state and no real audio driver initialization. Native tool SDL exposes no default playback
device. Terminal confirms three streams open and sustained actual PCM acceptance by SDL
without output errors; audible output is still unverified.

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
NGS2 event fabrication, audio samples or sound playback is introduced by the NGS2 provider itself. AJM services
remain unresolved; downstream AudioOut submission now has confirmed SDL acceptance in Terminal. Fatal fault diagnostics now print the signal
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
latest run loads NGS2, opens three real audio streams, passes two device-state queries and
the thread-self alias, then starts the stream scheduler and submits PCM to SDL before stopping
at sceGnmInsertPushMarker in a job worker after the window-margin response after numeric thread identity passes, world loading begins and the main loop starts
and Shadow Game Update is created.
Title and adjacent content-restriction uninitialized paths pass guest/original/loader checks;
the optional restriction branch was not taken in Terminal. Four keyboard samples are recorded; audible sound
and game frames remain unverified. The private graphics image preserves original executable bytes;
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
Terminal passes the unavailable save, trophy-context and AppContent responses and passes SaveDataDialogGetStatus (NONE) and passes the vibration denial and passes the window-margin response and reaches GnmInsertPushMarker in a job worker.
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
structures. Terminal confirms continuation after numeric thread identity and the light-bar
Terminal passes the unavailable save, trophy-context and AppContent responses and passes SaveDataDialogGetStatus (NONE) and passes the vibration denial and passes the window-margin response and reaches GnmInsertPushMarker in a job worker.

## Keyboard-controller light-bar availability

The scoped two-argument scePadSetLightBar adapter validates initialization, null parameter,
handle and the retail RGB brightness rule in that order. It reads exactly three RGB bytes;
reserved storage is not inspected. All three channels at or below 12 produce
INVALID_LIGHTBAR_SETTING. A valid color on the live keyboard-backed logical controller
returns NOT_PERMITTED: this is an explicit diagnostic policy because no physical light bar
is available, not a claim that output hardware changed. Unreadable RGB input returns
INVALID_ARG without host faults; stale handles remain invalid.

Guest tests exercise the RGB boundary matrix, guarded/unaligned three-byte input, partial
unreadable input, 32-bit handle width, stale extra arguments, initialization/null/handle
ordering, exact scopes, capability gating and unchanged inputs/errno in both CPU modes.
A private supplied-module comparison confirms retail validation and exact RGB reads, and
propagation of an injected device-output denial. The game caller discards this result;
Terminal confirms continuation after the light-bar denial. The next import is
Terminal passes the unavailable save, trophy-context and AppContent responses and passes SaveDataDialogGetStatus (NONE) and passes the vibration denial and passes the window-margin response and reaches GnmInsertPushMarker in a job worker.

## Standalone system status

The exact-scoped one-pointer sceSystemServiceGetStatus adapter describes the diagnostic's
standalone foreground application profile: no PS4 system-event queue, shell overlay,
background-execution mode, CPU mode 7, broadcast or VR play-area state. It does not infer
host window focus or treat the renderer menu as PS4 system UI. This local metadata does
not initialize PS4 shell or launch-control services, dismiss a splash or establish video.

The supplied retail status wrapper initializes **136 bytes**, including reserved storage;
the reference port's shorter 12-byte write is not used. The adapter writes the complete
zero profile and returns PARAMETER for null output. Synthetic guest tests cover repeated
calls, exact output/reserved bytes, unaligned and protected-page boundaries, extra stale
arguments, null error, scope/capability and errno in both CPU modes. A private supplied
wrapper comparison uses synthetic same-foreground-app/empty-event helper dependencies
and confirms the same complete output. The graphics-loader fixture verifies every status
byte after initializing the destination to a nonzero pattern. Actual game continuation
after this query is confirmed in Terminal, including processing the local login event.
The SaveData initialization failure now returns in Terminal; the next unresolved import is
sceGnmInsertPushMarker in a job worker after the window-margin response.

## Savedata availability

The exact-scoped one-pointer sceSaveDataInitialize3 returns SaveData INTERNAL because
this diagnostic has no savedata backend. The optional parameter is opaque and is not
inspected, including when null. This explicit failure creates no initialized state,
contexts, save directories, substitute filesystem, mounts or files; existing saves are
not read or modified. Other initialization versions, termination and mount APIs remain
unresolved. This is an unavailable-service adapter, not save support.

Guest tests check the pointer ABI, repeated failure, null/invalid/protected pointers,
unchanged buffers/errno and exact scope/capability in both CPU modes. A private supplied
Initialize3 wrapper comparison injects backend INTERNAL and checks propagation through
the original error conversion without reading the optional parameter. The synthetic
graphics loader also continues after the response in both modes. Terminal confirms actual
continuation after this initialization failure; Save Task Thread now reaches
GnmInsertPushMarker after the window-margin response. No game frame is verified.

## Uninitialized savedata directory search

The exact-scoped two-pointer sceSaveDataDirNameSearch adapter returns SaveData
NOT_INITIALIZED before inspecting either search conditions or results, matching the
supplied original module's absent-context path. A failed Initialize3 does not create
a context. Output counts, directory entries and buffers are left untouched: no successful
empty search or filesystem access is fabricated.

Guest tests cover repeated responses before/after failed initialization, null/invalid/
protected input and output pointers, ignored stale argument registers, untouched buffers/
errno and exact scopes/capability in both CPU modes. A private supplied original export
confirms the same uninitialized path without dependency-helper replacement. The synthetic
graphics loader also continues after this response in both modes. Terminal confirms actual
continuation after the search error; main then passes trophy-context creation's uninitialized response and reaches sceAppContentInitialize. Save support
and game video remain unverified.

## Uninitialized trophy-service paths

The six exact-scoped trophy imports use separate typed adapters for context creation,
handle creation, context destruction, handle destruction, context registration and trophy
unlock. Each returns Trophy NOT_INITIALIZED before inspecting handles, user IDs, service
labels, options or pointers, matching its supplied original export's initialization guard.
The trophy module is not initialized in this diagnostic. No contexts, handles, registration,
unlocks, progress, notifications or files are created; output IDs and platinum results
remain untouched rather than being set to fabricated sentinels.

Guest tests cover all six exports, repeated failure, null/unreadable/protected outputs,
64-bit option patterns, invalid handle/user/label values, ignored extra registers, scopes/
capability and unchanged buffers/errno in both CPU modes. A private comparison executes
all six supplied original exports without initialization or dependency-helper replacement
and confirms the same behavior. The synthetic graphics loader checks context creation's
uninitialized result and continues in both modes. Actual game continuation after the
context-creation response is confirmed in Terminal. Only context creation was called there;
the other five paths remain synthetic/original checks. The next boundary is
AppContent initialization, which now also returns in Terminal. The current boundary is
sceGnmInsertPushMarker after the window-margin response; game video is unverified.

## AppContent module availability

The exact-scoped two-pointer sceAppContentInitialize and four-argument
sceAppContentGetAddcontInfoList return the retail unavailable-module error before
inspecting their parameters. AppContent module 0xb4 has no provider and remains
unavailable in the existing module registry; failed calls do not create references or
initialized state. Boot buffers, list slots and hit counts remain untouched. This does
not configure an offline entitlement profile, enumerate host add-on folders, infer
licenses or initialize package/DLC mounts or downloads.

Guest tests cover both typed ABIs, module-before-pointer checks, invalid/protected/null
buffers, repeated calls, untouched outputs/counts/errno, scope/capability and unchanged
module availability in both CPU modes. A private comparison executes both supplied
original exports with absent module references and no dependency-helper replacement,
confirming the same paths. The graphics-loader fixture continues after the initialization
error in both modes. Terminal confirms actual continuation after initialization; the add-on
list was not called. Terminal also passes SaveDataDialogGetStatus (NONE); the vibration denial also returns; the window-margin response also returns; the next boundary is GnmInsertPushMarker.
No game frame is verified.

## Absent save-dialog status

The exact-scoped no-argument sceSaveDataDialogGetStatus and UpdateStatus queries return
NONE because no save-dialog instance has been initialized or opened. This is a status
enumeration value, not a successful result, completed interaction, dismissed error message
or saved game. No dialog state, UI, file access or fabricated user input is created. Other
save-dialog operations remain unresolved until their contracts are implemented.

Guest tests cover repeated NONE queries, stale registers including protected pointers,
unchanged buffers/errno and scope/capability in both CPU modes. A private comparison runs
the supplied original status exports with an absent instance and native serialized lock
dependencies; their status logic is unchanged and gives the same result. The synthetic
graphics loader also continues after both queries in both modes. Actual game continuation
after GetStatus is confirmed in Terminal; UpdateStatus was not called there. The next
unresolved import is now sceGnmInsertPushMarker after the window-margin response; game video remains unverified.

## Keyboard-controller vibration availability

The exact-scoped two-argument scePadSetVibration validates initialization, null parameter
and the live handle before reading exactly two motor-intensity bytes, following the
supplied original wrapper. For the keyboard-backed logical controller, readable parameters
return NOT_PERMITTED because no physical motors exist. This explicit policy also applies
to zero intensities: no physical stop or vibration is claimed. Unreadable input returns
INVALID_ARG without a host fault; stale handles remain invalid.

Guest tests cover validation order, 32-bit handle width, zero/minimum/intermediate/maximum
intensities, unaligned and protected-page input boundaries, partial/unreadable buffers,
stale handles/registers, capability/scope and unchanged input/errno in both CPU modes. A
private supplied-wrapper comparison checks exact two-byte reads and motor byte ordering
and propagation of an injected device-output denial. Actual game continuation after this
new adapter is confirmed in Terminal; one request returns without output. The next boundary
is sceGnmInsertPushMarker after the window-margin response; no game frame is verified.

## Unsupported output-window margins

The scoped three-int sceVideoOutSetWindowModeMargins adapter checks the actual live
main display handle through the provider's FlipStatus lookup. Valid ports return the
retail unsupported-operation error because this renderer profile does not implement output
window margins; invalid/closed handles and provider lookup errors retain their errors.
The pinned upstream margin function is an unconditional-success stub and is deliberately
not forwarded. No margin state, image crop, viewport or host-window update is claimed.
The CPU-only profile rejects every handle before using margin values.

Guest tests cover pre-open/live/failed-open/closed handles, three-int ABI and 32-bit
widths, signed/extreme margin values, provider errors, errno, scope/capability and zero
calls to a synthetic success stub in both modes. A private supplied-module comparison
uses a controlled main-port lookup with margin support disabled and confirms unsupported
precedence, handle errors and port release without any output ioctl. The graphics-loader
fixture also checks rejection before a display has opened. Actual game continuation with
this response is confirmed in Terminal. The next unresolved import is sceGnmInsertPushMarker
in a job worker; no game frame is verified.

## GNM debug-scope packet writers

The graphics-only allowlist now routes the exact scoped sceGnmInsertPushMarker and
sceGnmInsertPopMarker imports to the pinned renderer's existing typed implementations.
These write actual PM4 NOP debug-scope packets into the caller's command buffer; they
do not submit work, draw, compile a shader or signal a flip. Push uses its required
length-dependent packet size, string terminator and zero padding; Pop requires six dwords.
Invalid required pointers or sizes preserve the provider's failure return. Headless mode
continues to leave these imports unresolved.

Guest routing tests check scope/capability gating, exact packet headers/tags/payload/padding
for 256 synthetic string lengths, unaligned output guards, 32-bit sizes, argument failures
and errno in both CPU modes. Separate private checks execute the supplied original
exports and the actual pinned ARM64 GPU library's encoders through the guest ABI against
the same byte-level expectations. Both match without renderer initialization or GPU
submission. No upstream source or library rebuild is required. Terminal now passes the marker and reaches sceGnmSetCsShaderWithModifier in a job
worker; game video remains unverified.

## Compute shader register packets with modifiers

The graphics-only scoped sceGnmSetCsShaderWithModifier import now uses a local
retail-compatible packet writer. The pinned provider defaults its three SET_SH_REG
headers to graphics; the supplied original sets the compute bit in each header.
A private comparison with the actual native GPU encoder confirms this three-bit
difference. The adapter preserves compute program/resource/thread registers, the
four-bit modifier mask, zero-modifier behavior and the trailing NOP. It initializes
only 15 dwords of the minimum 25-dword reservation, preserving the unused NOP tail.
No upstream source or GPU library rebuild is required.

Guest tests cover 3,072 guarded packets across 64 synthetic register patterns, all
16 allowed modifiers and three reservation sizes. They check 32-bit argument
narrowing, unaligned inputs/outputs, unchanged registers/tail/guards/errno, invalid
masks and sizes, null pointers and a nonzero high shader address. Protected-page
checks verify rejection after the high address field and exactly seven dwords of
register input on success. The same expectations pass against the supplied original
in JIT and interpreter. Headless and wrong-scope imports stay unresolved.

Terminal now passes the debug marker and reaches this compute shader call in a job
worker after the main loop starts. Terminal passes the new adapter and reaches sceGnmDispatchInitDefaultHardwareState;
shader execution, game draws and game video remain unverified.

## Default compute hardware-state packets

The graphics-only allowlist now routes the exact scoped
sceGnmDispatchInitDefaultHardwareState import to the pinned native provider.
Its shim explicitly selects base PS4, matching the supplied original and the
current game profile. The encoder reserves/returns 256 dwords, initializes 18
with compute static-thread management, resource limits, an ACQUIRE_MEM barrier
and a trailing NOP, and preserves the remaining NOP payload. Reservations below
256 dwords return zero before touching the destination. This is command encoding;
it does not initialize a physical GPU, submit work or verify a game frame.

Synthetic guest tests cover all 256 short sizes with null/inaccessible destinations,
four sufficient sizes including unsigned extremes, 32-bit narrowing, unaligned
guarded output, three prior buffer patterns, unchanged tail and errno. Private
comparisons execute both the supplied original and the actual pinned ARM64 encoder
against the same expectations in JIT and interpreter, without renderer initialization.
The regular contracts command now also runs graphics routing checks in both modes.
Headless, data and wrong-scope imports remain unresolved; no GPU rebuild is required.

Terminal passes the compute shader modifier adapter and now reaches default compute
state preparation in a job worker. Terminal passes this route and reaches sceGnmAreSubmitsAllowed; game draws and
video remain unverified.

## Submission permission query

The graphics-only allowlist now routes the exact scoped sceGnmAreSubmitsAllowed
import to the pinned provider's no-argument query. It returns the provider's actual
submission-lock state: one when clear, zero when set. The same provider owns command
submission, SubmitDone and completion/idle release; the adapter never substitutes an
unconditional permission result. A permission query is neither a submission nor a
verified frame. Headless, data and wrong-scope imports remain unresolved. Missing
query support rejects provider activation before any graphics routes are published.

Guest tests check repeated allowed/blocked transitions, ignored stale arguments,
untouched buffers/errno/lock contents, scopes, capability gating and missing-export
activation. Private comparisons execute the supplied original with a controlled lock
pointer and the actual pinned native query with its isolated clear/set state. Both
pass in JIT and interpreter without renderer initialization or submissions. The
native white-box test uses only the provider lifecycle's zero/one states and restores
the initial clear state; no production lock manipulation or GPU rebuild is added.

Terminal passes default compute state preparation and now stops at this permission
query in a job worker. Terminal passes this query and reaches sceGnmDingDongForWorkload; GPU submissions,
game draws and video remain unverified.

## Batched compute bring-up

Import review now batches related compute APIs rather than waiting for each one to
stop a separate game run. This batch covers six functions; at review time only the workload doorbell
was the observed stop. The others are preventive packet/ABI corrections or
reviewed routes, not six independently observed game failures.

| Function | Connection and checked behavior |
|---|---|
| sceGnmDingDongForWorkload | Real provider-owned compute queue notification; void ABI with two 32-bit arguments and a 64-bit workload value. Both original and provider ignore the workload value here. |
| sceGnmDispatchIndirect | Native encoder; predicate/ordered-append flags, 32-bit offset, exact size and trailing NOP. |
| sceGnmDispatchIndirectOnMec | Native encoder; nonzero aligned 64-bit address, predicate/flags, exact size and preserved NOP tail. |
| sceGnmInsertDingDongMarker | Native four-dword NOP packet with exact size and null-output rejection. |
| sceGnmComputeWaitOnAddress | Local retail packet encoder; register/memory address selection, 48-bit clipping, comparison/mask/reference and untouched NOP padding. The pinned HLE retains more high address bits. |
| sceGnmSetCsShader | Local retail adapter using the checked zero-modifier path; correct compute headers instead of the pinned HLE's default graphics headers. |

Synthetic guest tests check scope/capability/headless exclusions, missing-doorbell
activation rejection, argument widths, full-width workload forwarding, flags, errors,
byte-level packets, unaligned guards, untouched padding and errno in both modes.
The packet matrix includes nine address profiles and 32 flag values for four encoders.
Private original-module comparisons exercise the same packet expectations, including
full-width wait-address clipping and the plain shader path. A controlled original
workload-doorbell fixture checks mutex ordering, notification operands/counter and
32-bit guarded doorbell writes, without a kernel GPU transport. The actual native
library matches the indirect/marker encoders and representable wait-address profiles;
its workload doorbell's queue-zero no-op passes through the guest ABI in both modes.

Nonzero native queue notification, ring wrapping, command execution and game frames
still require Terminal verification. The route forwards to the same provider that
owns mapped queues and submission/completion; it does not simulate consumption or
signal a completed frame. Unknown imports still stop explicitly. No upstream source
change or GPU rebuild is required.

## Batched pixel/vertex shader and primitive-control encoders

A second batch routes five exact scoped imports to the pinned native provider:
sceGnmSetPsShader350, sceGnmUpdatePsShader350, sceGnmUpdateVsShader,
sceGnmSetVgtControl and sceGnmResetVgtControl. The observed Terminal boundary is
the PS350 setter; the other four are reviewed prospectively. No upstream patch
or GPU library rebuild is needed.

PS350 set/update accept a null register pointer as a real shader-disable packet
sequence, with the retail default color mask. Non-null input contains 12 register
words and requires a zero high shader address. The update API uses the original
NOP-tagged context updates. Both preserve unused trailing NOP data in the 40-dword
reservation. VS update reads seven words, applies the six allowed modifier bits
(with distinct zero-modifier semantics), writes the original context-update tags
and preserves unused data in its 29-dword reservation. Base-PS4 VGT control validates
primitive-group and mode ranges, encodes the actual register write and resets to
the original default; no physical hardware state is claimed from packet generation.

A new synthetic guest fixture runs automatically in both CPU modes. It covers 32
register patterns, both PS350 null/non-null paths, all 64 VS modifier combinations,
minimum/oversized/unsigned reservation widths, unaligned guarded buffers, exact
packet bytes, untouched inputs/NOP tails, parameter failures and errno. Protected
pages verify early high-address rejection after eight bytes and successful reads
of exactly 48 PS bytes or 28 VS bytes. Primitive control covers all 256 valid group
sizes and invalid boundaries/mode values, plus reset and invalid buffer sizes.
Scope/capability/headless checks and individual missing-export activation failures
cover all five imports.

Private checks execute the supplied original five exports and actual pinned ARM64
GPU implementations against the same expectations in JIT and interpreter. All
match without renderer initialization, shader execution or game command submission.
Terminal now passes the workload doorbell route and reaches PS350 preparation;
continuation after this batch, rendered game frames and video require verification.

## Batched graphics submission and flip adapters

Four scoped submit imports now use checked adapters around the native provider:
sceGnmSubmitCommandBuffers, sceGnmSubmitCommandBuffersForWorkload,
sceGnmSubmitAndFlipCommandBuffers and sceGnmSubmitAndFlipCommandBuffersForWorkload.
The two workload variants are newly connected; the existing plain variants share
the same validation and provider-owned submission/flip paths. No synthetic completion,
frame, workload lifecycle or successful backend result is substituted.

For a nonempty batch, validation checks required DCB pointer/size arrays, then the
conditional CCB pointer requirement, before reading size entries. It checks every
DCB size against the original nonzero/maximum rule and every supplied CCB size against
the maximum. Errors leave the backend uncalled. This repairs the native non-flip
variant's missing CCB prerequisite check. Retail accepts a zero-count non-flip
submission without inspecting arrays: the adapter supplies safe empty storage to
the native path and returns its actual backend result. A zero-count flip is explicitly
rejected with the submission-argument error before either implementation can select
an invalid final buffer; this is a deliberate safety guard, not an original-module
behavior claim. Valid flip submissions still require the provider's supported final
prepare-flip packet and a real VideoOut port/buffer.

A new synthetic guest fixture exercises all four adapters with 5, 6, 9 and 10
arguments. Original x86 fixture thunks place arguments seven through ten in actual
SysV stack slots. Tests verify the unchanged host bridge delivers 32-bit port/index/
mode values and full signed 64-bit flip data to native registers/stack in both modes.
They also check multi-buffer validation, CCB-before-size precedence, upper argument
bits, unchanged metadata, backend error propagation, empty submissions, scopes,
capability/headless exclusion and missing-provider activation rejection.

Private checks execute the original workload exports with their full preflight code
intact and controlled mutex/readiness/transport/flip-patch dependencies. They confirm
validation, count-zero non-flip behavior, six/ten-argument routing and backend/flip
error propagation without GPU transport. An overlapping early-rejection matrix also
passes against the actual pinned native library in JIT and interpreter. Real command
execution, flip patching/presentation and game frames still require Terminal testing.

Terminal now passes PS350 preparation and reaches the workload graphics submission
call after entering the main loop. It also reaches the provider's shader compiler
warning path; this does not verify a rendered frame. The float32 denormal-flush warning
is not the import stop. No upstream change or GPU library rebuild is required.

## Flip-label wait and debug-marker batch

Four more scoped imports are connected: sceGnmInsertWaitFlipDone,
sceGnmInsertSetMarker, sceGnmInsertPushColorMarker and sceGnmInsertSetColorMarker.
The observed stop is InsertWaitFlipDone; the marker work is prospective.

The wait adapter queries the actual native VideoOut buffer-label address for the
tracked live display and writes the original seven-dword WAIT_REG_MEM packet for
the selected label. It preserves 32-bit handle/index/size narrowing, eight-byte label
stride, four-byte alignment and the original 48-bit hardware address. No label value
is changed and no completed flip is invented. Unsupported sizes return failure before
querying VideoOut. Null output, out-of-range indices and unavailable/closed displays
are guarded explicitly. Native label-query errors preserve the destination and are
returned instead of reaching the pinned encoder's assertion. These malformed-input
and query-error guards are deliberate diagnostic behavior: the original does not
validate the query result or null output. Successful packets match the original.

The plain SetMarker route uses the checked native encoder. Both colored variants
use local original-compatible packet writers: color immediately follows the string's
NUL, followed by zero padding. The pinned native push-color implementation instead
places it eight bytes later; the pinned set-color export returns success without
writing any packet. Neither implementation is forwarded for colored imports.
Private tests confirm both differences in the actual loaded native library.

A synthetic guest fixture checks wait packets for all 16 buffer indices across
four address profiles (including high bits and wrap), query identity, opening/closing,
error/size/index precedence, unchanged failure outputs, scope/capability gating and
missing-provider activation. The three marker encoders cover 256 synthetic string
lengths and four color patterns, exact headers/tags/NUL/color/padding, unaligned
buffers, 32-bit argument widths, invalid arguments, guards and errno. All run in both
CPU modes as part of the full contracts command.

Private original-module comparisons use a controlled VideoOut label query and match
the valid wait and all marker expectations in JIT and interpreter. The actual native
SetMarker matches; native wait size rejection is checked without renderer startup.
The checked local color adapters also match the originals. These checks do not prove
a completed wait, host presentation or frame rendering.

Terminal passes graphics workload submission and now reaches the flip-wait packet
builder. Continuation after this batch and game video still require Terminal testing.
No upstream source change or GPU library rebuild is required.

## Latest combined run and absent save-dialog lifecycle

The latest Terminal log passes the flip-wait route and records Metal activity of
one draw and 61 dispatches in a per-second sample. This verifies encoded renderer
work, not correct game pixels, presentation or playability. The run also reports
SRT resource-offset failures involving Phi instructions, unflattened resource
sources, oversized buffer clamps, nonuniform wave-lane warnings and an unsupported
BC1 image format/usage combination. These are unresolved renderer issues; no shader,
texture or primitive-restart fix is claimed here. The explicit stop is now
sceSaveDataDialogTerminate in the save worker.

Five scoped absent-instance lifecycle calls are implemented together. Terminate,
Close, IsReadyToDisplay and ProgressBarSetValue return the original NOT_INITIALIZED
error. Close checks the absent instance before any parameter access; progress ignores
its two 32-bit values in that state. GetResult checks a null output first and returns
the original null-argument error, otherwise NOT_INITIALIZED without reading or writing
the output. Readiness returns an error code, not Boolean true. Existing GetStatus and
UpdateStatus remain NONE before and after repeated lifecycle failures. No dialog,
completed interaction, file, save backend or persistent state is created; Initialize
and Open remain unresolved.

Guest tests cover repeated calls, null/invalid/protected/unaligned output pointers,
stale extra arguments, unchanged buffers/errno, result-error precedence and strict
scope/capability gates. A private supplied-module comparison uses the original empty
instance and native serialized mutex dependencies, checking the same cases plus
unchanged status and instance state. The comparison and full contracts pass in JIT
and interpreter. Separate counters report all five unavailable lifecycle requests.
Continuation after this response, further renderer diagnosis and game video require
another Terminal run; logs are read directly from the local diagnostic directory.

## JIT progression and batched indirect draw encoders

A new native Terminal JIT run finishes initialization at 1.0367 seconds and enters
the main loop at 1.6307 seconds. The preceding interpreter run entered at 28.5705
seconds. These are two diagnostic timing samples, not a gameplay FPS benchmark.
The JIT run records a Metal sample of 43 draws and 303 dispatches per second and
reaches sceGnmDrawIndirect in a job worker before the budget expires. The preceding
run's absence of an import stop only described that interpreter execution; it did
not establish complete import coverage. SRT resource-offset errors and buffer clamps
persist; no correct game image or presentation is verified.

Four exact scoped draw encoders are now connected together: sceGnmDrawIndirect,
sceGnmDrawIndexIndirect, sceGnmDrawIndexOffset and
sceGnmDrawIndexIndirectCountMulti. The two simple indirect variants use a checked
stage guard before forwarding to native packet generation. The supplied retail
module only accepts VS/ES/LS, whereas the pinned native simple encoders also accept
other stages. The guard preserves the original failure without changing the output.
The offset and indexed-count variants use their compatible native implementations.
No successful GPU draw is substituted for generating a command packet.

A synthetic guest fixture checks 4,504 operand combinations plus malformed buffer
cases. It covers valid/invalid shader stages, zero and boundary SGPR indices, their
stage-specific register offsets, predicate flags and ignored base-PS4 flag bits,
indexed/auto-index/count packet fields, zero/full-width 64-bit count addresses,
32-bit argument narrowing and untouched NOP padding/guards/errno. Original x86
fixture thunks exercise real stack slots for the 5-, 7- and 9-argument variants.
Scope/capability/headless exclusions and missing-export activation cover all four.

Private checks execute the supplied original functions and the actual pinned native
GPU encoders through the checked stage guards against the same matrix in JIT and
interpreter. Both match; the actual native invalid-stage acceptance is also reproduced
in isolation to establish the need for the guard. These checks initialize no renderer
and submit no GPU work. The full contracts pass in both modes. No upstream edit or
GPU rebuild is required. Game continuation after this batch requires Terminal testing.

## Real-time calendar batch after the next JIT stop

The next Terminal JIT run enters the main loop at 1.3731 seconds and reaches
sceRtcGetCurrentClock. Metal reports samples of 96 draws/575 dispatches and
159 draws/910 dispatches per second. These are renderer work counters, not FPS or
verified game pixels. SRT Phi/resource-offset errors, unflattened resources and
buffer clamps remain unresolved; this change makes no renderer correction.

Four exact scoped RTC functions are implemented together: GetCurrentClock,
GetCurrentClockLocalTime, GetTickResolution and GetDayOfWeek. Calendar reads use
the real host clock and timezone, signed 32-bit minute offsets and the original
16-byte Gregorian output layout. Resolution is one million ticks per second.
Weekday validation follows the supported title's SDK profile, including month-before-
year error precedence. Null calendar outputs preserve guest errno. No network time
or clock-setting API is supplied.

Synthetic guest tests cover four timezone profiles, positive/negative minute offsets
and signed extremes, 32-bit argument narrowing, guarded unaligned outputs, Gregorian
weekday cases across years 1 through 9999, leap centuries, invalid arguments and
scope/capability gates. A private comparison against the supplied original four
functions passes the same cases in JIT and interpreter using real host clock reads.
The full contracts also pass in both CPU modes. No GPU rebuild is required; game
continuation after this batch still requires a Terminal run.

## GPU notification queue growth and developer handoff

The next Terminal run passes RTC, enters the main loop at 1.6131 seconds and reports
a Metal sample of 486 draws/1,985 dispatches per second. It stops at the host memory
backend's fixed 64-entry GPU notification queue, then reports secondary termination
errors. No correct game image is verified. A tessellation pipeline fails MSL
compilation because of duplicate output locations; SRT Phi offset failures, buffer
clamps and BC texture usage problems remain open.

The diagnostic now uses growable notification storage in a digest-checked private
copy of the pinned memory backend. Only storage and its capacity guard change;
allocator logic, count rollback, locks and callback dispatch remain upstream.
Events are neither dropped nor merged. Allocation/capacity exhaustion stops explicitly
instead of claiming a successful notification. The pinned checkout and GPU library
are unchanged; only the host diagnostic needs rebuilding.

Full contracts pass in JIT and interpreter. Regression coverage includes registering
hooks over 257 actual shared aliases, releasing those aliases through the guest ABI,
8,193 mixed events in FIFO order, callback reentry outside the region write lock,
count rollback, reuse, absent hooks and explicit allocation/capacity failure. This
checks local queue behavior, not GPU completion or global callback completion ordering
across competing flushers. A combined Terminal rerun must verify continuation.

The [development handoff](DEVELOPMENT_HANDOFF.md) records reproduction, build prerequisites,
code ownership, verification limits and the prioritized renderer debugging backlog.

## Reviewed upstream renderer backports

The 2026-10-21 upstream review found an exact match for the duplicate auxiliary
shader location error. Three selected runtime commits are now backported over the
existing pinned GPU base: auxiliary tessellation location deduplication, shader-cache
invalidation for that correction, and retained Metal shader/graphics resources for
deferred draws. The CPU/runtime/reference pins are unchanged. The builder accepts
only the exact reviewed patch set and records its provenance in the private build
report. See [upstream review](UPSTREAM_REVIEW.md).

The ARM64 library rebuild and actual SPIR-V emitter regression pass. Fifteen synthetic
modules cover duplicate/sparse/default/flat/full/empty interfaces across three shader
kinds; the original emitter reproduces the duplicate-location failure. This verifies
generated interface locations, not driver compilation or correct game pixels. The
lifetime backport still needs real renderer exercise. SRT Phi errors, buffer clamps
and compressed-texture usage remain unresolved. Rebuild the GPU library before the
next combined Terminal run; the host queue change alone needed no GPU rebuild.

## SSE4a instruction batch after renderer continuation

The next native Terminal run no longer reports the notification queue overflow or
duplicate auxiliary shader location error. It enters the main loop at 1.3763 seconds
and records a sample of 1,017 draws/3,145 dispatches per second. These counters do not
verify game pixels or FPS. Execution stops on the CPU instruction EXTRQ. The renderer
also reports unsupported PerVertexKHR decoration in MSL, persistent SRT Phi failures,
buffer clamps and compressed image usage errors; affected pipelines are skipped.

The vector extension now implements both EXTRQ and INSERTQ in register and immediate
forms, following AMD APM volume 4 revision 3.27. The pinned decoder/interpreter/JIT
sources remain unchanged; JIT execution uses its existing synchronized fallback.
Controls are narrowed to six bits, zero length means 64, and source data/controls are
captured before any aliased destination write. Flags, MXCSR and upper YMM state remain
unchanged. AMD leaves the destination's upper quadword undefined; preserving it here
is a deterministic diagnostic choice, not a hardware guarantee. Architecturally
undefined ranges stop explicitly rather than relying on undefined host-language shifts.

Native emitter/JIT fault checks and full contracts pass. A bit-by-bit scalar oracle
checks 266,240 cases per CPU mode across all defined length/index combinations,
register/immediate forms, ignored control bits, low/high/aliased XMM registers,
full-width fields, unchanged vectors/MXCSR/flags and translated flag consumers.
All four forms reject an undefined range explicitly in each mode. Existing SSE4a
CPUID advertisement is retained. No x86 hardware oracle or Rosetta is used.
Only the host diagnostic needs rebuilding; the existing GPU library is unchanged.
A Terminal rerun must establish the next boundary and any actual visible output.

## SSE4a range compatibility and isolated fragment diagnosis

The next Terminal run passes the formerly unsupported EXTRQ instructions and reaches
our strict range guard instead. It enters the main loop at 1.3778 seconds and reports
a 612-draw/2,366-dispatch per-second sample plus 151 compiled pipelines. The queue
overflow and duplicate-location errors remain absent in this run. None of these
counters verifies game pixels or FPS.

AMD leaves fields extending beyond bit 63 undefined. The adapter now follows QEMU
v10.1.0's explicit shift/mask/truncate policy for EXTRQ and INSERTQ instead of stopping
on those controls. All host shifts remain defined, including zero length (64) and
nonzero indices. This is a compatibility decision, not a claim about original Jaguar
hardware behavior. The bit-by-bit scalar regression now covers every six-bit control
combination: 524,288 cases per mode. A private comparison against QEMU's actual helper
functions matches in both modes. Native CPU checks and full contracts pass. No GPU
rebuild is required for this host change.

A separate public synthetic fragment diagnostic uses the installed Sirit/SPIRV-Cross
libraries without a GPU or game. Ordinary fragment input converts; raw per-vertex
input selecting vertex one reproduces the known PerVertexKHR rejection (exit 2).
This isolates a compiler/interface limitation; it does not correct it. Advertised
Vulkan barycentric support cannot be taken as proof that this MSL conversion is
supported. The shader's vertex-selection semantics must be preserved in a future
solution. SRT, texture usage and visible-frame verification remain open.

The user's preferred upstream reference is bloodborne-mac and its runtime/recomp
projects; this policy is recorded in AGENTS.md and the development handoff. Existing
source pins remain unchanged.

## Missing qword widening pairs after upstream comparison

The next Terminal run passes the former EXTRQ range guard and enters the main loop
at 1.3992 seconds. It stops at unsupported VPMOVZXWQ; a cache message records 183
compiled pipelines in the session. Queue overflow and duplicate-location errors are
absent. PerVertexKHR conversion failures, SRT Phi errors, buffer clamps, compressed
image usage and IT_GET_LOD_STATS remain unresolved. No correct game image is verified.

The preferred bloodborne-mac/runtime/recomp reference was checked again. The current
recomp source also lacks PMOVZXBQ, PMOVZXWQ, PMOVSXBQ and PMOVSXWQ and their VEX
mnemonics in both interpreter and ARM64 JIT. The local extension completes those eight
mnemonics using the pinned widening behavior as the reference. Other width pairs
still use their existing pinned native JIT or interpreter paths. Source pins are
unchanged; the missing forms use the JIT's synchronized interpreter fallback.

Both signed and zero extension work for SSE128, VEX128 and VEX256 destinations.
Register aliases are captured before writing; memory reads use precisely the bytes
consumed, even when a protected page starts immediately afterward. Legacy SSE keeps
upper YMM bits; VEX128 clears them and VEX256 fills the full destination. No flags
or MXCSR changes are introduced. This does not establish complete AVX2 coverage.

Native CPU checks and full contracts pass. The new byte-copy/sign-fill oracle covers
64,512 cases per mode across the full twelve-pair signed/unsigned widening family:
all byte values, word/dword boundaries and deterministic patterns, low/high/aliased
registers, unaligned and two-sided guard-page memory, upper lanes and translated flag
consumers. There is no hardware x86 oracle or game input in those tests. A new
Terminal run must verify game continuation; no GPU rebuild is needed for this change.


## SDL3 physical controller adapter (2026-10-21)

The combined graphics host now binds the resident SDL3 gamepad provider alongside
the keyboard provider. It supports DualShock-compatible buttons, analog sticks and
triggers, two touch contacts, hotplug with keyboard fallback, and checked LED/rumble
forwarding. Failed outputs remain explicit errors. No dependency pin or renderer
binary changed. Synthetic guest ABI coverage runs in JIT and interpreter modes;
physical USB/Bluetooth operation still requires hardware verification.

The latest native Terminal log passes the widening batch and stops at
`sceSaveDataMount2` on the save worker. Draw/dispatch activity is present, but correct
pixels, game FPS and a playable application remain unverified. See the
[development handoff](DEVELOPMENT_HANDOFF.md) for the current boundary and
[connection instructions](guide/building.md#dualshock-input).


## Logos/menu confirmed and SaveData failure batch (2026-10-21)

The owner visually confirms the game logos and menu on Apple M1. The latest native
run enters the main loop at about 1.48 seconds and reads a physical DualSense without
disconnections (1,018 samples). SDL accepts 507 LED and 508 rumble operations; the
actual physical effects and connection transport are not established by the log.
A playable 3D scene, gameplay stability, audible sound and save persistence remain
unverified.

The explicit import stop is SaveData Mount2 during a profile-save request. Eight
related mount/cleanup/metadata exports now return the original module's
NOT_INITIALIZED failure before inspecting any guest pointer. Full synthetic runtime
contracts, loader routing and private original-module comparisons pass in both CPU
modes. There is still no save backend or save access. Game continuation requires
another native Terminal run; no renderer rebuild is needed.

A GitHub search found an exact SotC SRT/Phi report and exact MoltenVK per-vertex
converter reports. The AMD-specific workaround reported in the SotC issue was not
applied to M1. See [review and sources](UPSTREAM_REVIEW.md#matching-public-error-reports-and-mount-failure-batch-2026-10-21).


## Save-dialog initialization and Open contracts (2026-10-21)

The latest native log confirms that Mount2 returns the implemented NOT_INITIALIZED
error and execution proceeds to SaveDataDialog Initialize on the save worker. The
main loop starts at about 1.37 seconds; the DualSense remains connected (778 samples,
387 accepted LED requests, 388 accepted rumble requests). No execution fault appears.
The known SRT/Phi, PerVertexKHR and buffer-clamp errors remain.

Initialize and Open now complete the nine scoped save-dialog exports used by this
diagnostic. Initialization reports its missing CommonDialog dependency. Open follows
the original base-header/cookie/reserved/size validation before reporting no instance,
with additional safe unreadable-pointer guards. No dialog UI, completed interaction,
user selection or save access is created. Full contracts and private original-module
comparisons pass in both CPU modes; the initializer comparison uses an injected
platform-dependency failure. The actual game response requires another native run.


## Opening audio confirmed; run reaches diagnostic timeout (2026-10-21)

The owner hears the beginning of the game's introduction, in addition to the
previously confirmed logos and menu. This is direct confirmation of audible game
output; full audio coverage, audiovisual synchronization and correct introduction
video remain unverified.

The latest run starts its main loop at about 1.38 seconds, detects the DualSense,
and continues after the profile-save request. It has no unresolved-import stop or
CPU execution fault before STOP_TIMEOUT at the configured 60-second budget. It
records 78 per-vertex conversion messages, 263 unexpected-Phi offset messages and
186 warnings about partly GPU-mapped regions. Some draws are skipped for missing
pipelines. These are message counts, not distinct shaders, rendered frames or FPS.
Scene entry and renderer correctness are now the immediate investigation; no
playable 3D scene or save persistence is established.


## Five-minute game diagnostic and guarded shader-memory reads (2026-10-21)

**Withdrawn after the subsequent host incident; see the current entry below.**

The combined graphics/game run now defaults to five minutes. The watchdog remains
mandatory and accepts explicit budgets from 1 to 1800 seconds; headless defaults
stay at 30 seconds and renderer-only startup keeps its 60-second limit. Tests check
range/error handling, default/override routing and real one-second guest timeouts
in JIT and interpreter modes.

The host now enables the renderer's existing guarded dynamic-memory path before
loading its library unless BB_DIRECT_MEMORY_ACCESS is explicitly set. The installed
SRT walker cannot flatten offsets depending on Phi or per-lane shader execution;
with DMA disabled, shader-info collection clears the corresponding dynamic resources.
An actual-renderer synthetic fixture checks four uniform/Phi/lane/arithmetic cases
with DMA off/on, preserving uniform data and enabling dynamic flags plus page-table/
fault-buffer resources as appropriate. No GPU or game is used by that fixture.

This is a host configuration correction for the next native experiment, not proof
of a rendering fix. SRT/Phi diagnostics can still appear; PerVertexKHR and partially
GPU-mapped regions remain unresolved. No GPU rebuild is needed. Use the new 300-second
command in the build guide and compare scene progress in native Terminal.

## Host watchdog report and experiment rollback (2026-10-21)

The owner reported a kernel panic because watchdogd did not check in for 90 seconds.
The latest private log selects dynamic shader reads and ends during renderer
initialization, after shared guest-memory imports and worker preparation. It never
records renderer readiness or guest entry. The preceding complete run uses the
former configuration, reaches the main loop, records a GPU submission unfinished
for more than ten seconds, and exits at its 60-second timeout. The panic's triggering
path is unresolved; neither DMA, memory pressure nor the controller is established
as the cause.

The automatic DMA setting is withdrawn. Unset selects 0; all explicit values other
than literal 0 fail before dlopen and before script Vulkan/Metal prechecks. The
unvalidated 300-second default is withdrawn; after the incident the owner requested
a 120-second combined budget. Explicit 1..1800 budgets remain supported. A process
alarm cannot guarantee recovery from a kernel/driver hang. GPU experiments are
paused, with no attempt to reproduce the
panic. CPU/synthetic tests cover configuration rejection, library-constructor
isolation, launch-precheck ordering, timeout routing and the runtime ABI. No real
GPU library is loaded by this verification. Raw logs and panic details stay private.

## Gradual testing resumed with external supervision (2026-10-21)

The owner explicitly authorized gradual GPU tests after the initial pause. A new
native run uses DMA off, completes initialization at about 1.69 seconds, enters the
main loop at about 2.33 seconds and reaches its 120-second timeout without an import
stop, CPU execution fault or GPU-watchdog warning. Metal draws/dispatches continue
in its sampled intervals. The DualSense is detected. Correct introduction video,
scene entry and system stability remain unverified. Five per-vertex conversion
messages, 220 Phi offset errors, 228 SRT-walk failures and 996 buffer clamps remain.
These counts are not distinct shaders or evidence that their causes are fixed.

The combined launcher now uses an independent parent to mirror private output,
apply a wall deadline including renderer startup and stop the process group on a
GPU-watchdog message. Stop reasons and original child exit status are preserved;
there are no retries. Synthetic child-process tests cover fragmented output,
silent children, ignored TERM, worker cleanup, cancellation, status propagation,
invalid durations and preserving existing logs. The standalone Metal check is
also supervised for ten seconds. Neither userspace process can guarantee recovery
from a kernel/driver hang.

The next native comparison is one 30-second run with BB_ASYNC_SUBMIT=0 and DMA off.
Only submission timing changes; the recording workers and renderer pins stay the
same. A short Metal precheck in automation still reports zero devices and exits
24 before shader execution, so the combined comparison needs native Terminal.
Inspect that one log before another experiment; no shader fix or stability result
is claimed by the new supervisor.

## Asynchronous submission off: log reviewed; exit cause unresolved (2026-10-21)

The native comparison header confirms DMA off, asynchronous submission off and JIT
on, with a 30-second supervisor. The main loop starts at about 1.36 seconds and five
Metal activity samples are recorded. There is no import stop, CPU-fault report or
GPU-watchdog warning in the file. However, it ends in ordinary buffer-clamp output
without a timeout/stop/completion marker, so its exit cause cannot be established.
Forty-eight per-vertex errors, 259 Phi offset errors, 267 SRT-walk failures, six
compressed-texture usage errors and 7,788 clamps remain. Counts are not FPS, unique
shaders or a controlled rendering-quality comparison.

The supervisor now writes its stop decision before cleanup and records elapsed
time plus both child and supervisor exit statuses on normal completion paths,
including natural child exit. Synthetic checks exercise a large-output natural
exit and a busy child that prints during timeout cleanup, alongside the existing
signal/status/cancellation tests. This addresses the logging ambiguity; it does
not identify the cause of the preceding exit or repair shaders. No new GPU run is
used to verify the change. Keep the same 30-second settings for the next observation.

## Cleanup denial observed; cooperative cancellation added (2026-10-21)

The newest native log retains DMA off, asynchronous submission off and the
30-second budget. The main loop starts at about 1.38 seconds with a DualSense and
Metal activity. No unresolved import, CPU-fault report or GPU-watchdog warning is
recorded. The footer now shows the wall deadline, cleanup failure with Operation
not permitted, unknown child status and supervisor exit 1 after about 30.56 seconds.
This confirms failed cleanup rather than successful termination. It does not prove
the preceding incomplete log had the same cause. Shader/resource errors remain.

Owned diagnostics now validate an inherited cancellation pipe before starting the
renderer. A detached native host thread waits on that pipe; a parent request or
channel closure makes the child exit itself, preserving the existing immediate
diagnostic-exit policy. The parent waits for status and reports failure if it
cannot confirm exit. Cooperative mode does not send external signals, elevate
privileges or fall back to denied signaling. Errors now identify the cleanup stage.
Process-group mode remains available for synthetic process fixtures.

CPU-only native tests cover a deadline despite ignored TERM, parent cancellation,
parent-channel closure and invalid descriptors. Actual synthetic infinite guests
exit through the new channel in JIT and interpreter before their own longer alarm.
The full runtime contracts pass. No new GPU run is used for validation, and neither
this channel nor an alarm guarantees recovery from a kernel/driver hang. Confirm
that the preceding window has closed before one further identical 30-second
observation; no renderer flag or dependency pin changed.

## Cooperative stop confirmed in native GPU run (2026-10-21)

The newest native run confirms the cancellation channel is ready before renderer
initialization. DMA and asynchronous submission remain off, with JIT on and a
30-second wall budget. Initialization completes at about 1.10 seconds and the main
loop starts at about 1.63 seconds. The log records the wall-time stop request,
the child's cooperative termination message and matching child/supervisor status
21 after about 30.52 seconds. No cleanup-permission error, unresolved import,
CPU-fault report or GPU-watchdog warning is recorded. Status 21 is the intentional
diagnostic limit, not a newly discovered game fault.

The DualSense and Metal draw/dispatch activity are recorded. Twenty-one per-vertex
conversion/skipped-pipeline messages, 220 Phi offset errors, 228 SRT-walk failures,
six compressed-texture usage errors and 910 buffer clamps remain. These counts
are not FPS, unique shaders or evidence that rendering improved. Correct
introduction video, scene entry and system stability remain unverified. No new
runtime configuration or additional GPU test is introduced by this log review.
