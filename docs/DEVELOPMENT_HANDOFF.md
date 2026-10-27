# Development and debugging handoff

## October 27: playable with artifacts — owner-confirmed milestone

The owner confirms that the game is playable on Apple M1 (16 GB), with character
geometry artifacts, intermittent vegetation and low FPS. The latest measured
session lasted 205.045 seconds and ended with a normal window close (exit zero).
SDL detected and connected a DualSense. Recent frame-rate windows were 3.6–5.7
FPS. Whole-device GPU samples averaged 63.2% (maximum 74%); these include other
applications and do not establish a purely GPU-bound workload.

The single-pool import reduced startup Metal allocation from 8002 to 6978 MiB;
the final sample was 9913 MiB. No memory failure occurred in this session, but
previous OOMs are not considered permanently fixed. Geometry at offsets above
4 GiB remains a comparison target; the log does not identify the artifact cause.
Safe math and staging correction remain enabled. Full ARM64 JIT/interpreter CPU
contracts pass, including synthetic gamepad controls. Sustained performance,
visual correctness, audiovisual synchronization and save persistence remain open.

Earlier entries below are historical checkpoints.


## October 26: native Metal RC integration

The active renderer is `a27f0ca477bd8356a015311d1eaa92e6f6a0b007`, the runtime
selected by bloodborne-mac `9c8ac5c` (0.9.0 RC1). Local SotC adaptations were
merged onto this revision, with the existing CPU translator `196ed99` deliberately
retained. New native device/queue initialization, capabilities, 3D texture support,
resource ownership routing and native-resolution classification are included.
The launcher selects `BB_METAL_NATIVE_BOOT=1` without Vulkan discovery. Explicit
`BB_METAL_NATIVE_BOOT=0` retains the previous Metal-on-Vulkan startup comparison;
the dedicated Vulkan check still uses the loader. These modes are not identical.

Four optional button-prompt imports preserve SotC's original icons (no Bloodborne
input-name replacement). Input handling itself stays on our existing SDL adapter.
Old workspace/build `84daa9f13e5c` remains available. Copied dependency install names
were relocated to the new prefix; old libraries were not edited. No game has been
launched as part of this update, and the observed Metal OOM is not claimed fixed.

Next manual trial: `bash prueba_runtime_actualizado.command`. This establishes a
new baseline: native boot, three balanced pool windows, default collector budget,
shim residency enabled, existing exact-shader barrier trial, resource observations,
single inflight command, skipped intro, GPU wait120s and global500s. Other optional
render/indirect/CPU probes are off. This is not a single-variable comparison with
the preceding single-pool/early-GC trial. Native buffer summaries distinguish the
shim's residency switch from the separate renderer address-buffer residency set.
Build/check evidence: `work/analysis/runtime-update-20261026/`.

## Adaptive owner-authorized series

The owner authorized up to 20 serial tests, probe changes between results,
corrections when justified, and removal of redundant probes after a tested fix.
After current failures are fixed, investigate compatible near-colossus startup.
See [controller and stopping behavior](ADAPTIVE_TESTS.md). Agent Metal access is
unavailable and Terminal UI control is blocked; one owner Terminal command must
start the controller. Never claim a game run happened from the failed precheck.
The first reviewed request adds backing-write probes; preserve its ninth journal
and stop on incomplete exit, cleanup uncertainty or GPU stall.

Round 15 is sealed as a SIGSEGV and remains evidence-only. Do not repeat it. The
next checked shader probe covers one separate gap: a writable storage descriptor
can resolve to a cached image with a different actual guest range. Journal layout
10 labels this range `storage_image_resolved` only when it differs from the
requested descriptor. A match is still write intent, not proof of a GPU store,
stable allocation identity or original writer. Addressless resources and direct
guest stores remain outside coverage.

## Pool-offset observations after the first fresh prefix transition

The latest owner control captures the altered prefix already at consumption,
about 362 ms after its recognized pre-enqueue sample. No original writer is
identified. The next renderer retains `shader-mapping-events.bin`, an eighth
journal comparing pool offsets only on changed prefixes. Same offsets are not
allocation identity; different offsets are separate backing observations.
Preserve old binaries for matching older runs. The analyzer's saved-program
fallback recovers missing terminal lookups without inventing timing evidence.

## Fresh consumption shader probes

Keep the new seventh `shader-consume-events.bin` journal when preserving runs.
It contains a fresh checked prefix read, unlike the consume-phase submission
record, which deliberately retains its pre-enqueue sample. See
[stage and watch-state semantics](SHADER_CONSUME_PROBES.md). The latest owner
SIGSEGV repeats an earlier consumer with the XADD correction active; it predates
these renderer probes. No original corruption writer is established.

## Register XADD alias correction

The private scalar preparation now fixes register XADD source/destination
aliasing, independently reproduced in interpreter and JIT fallback. All 256
original fixture cases pass per mode after correction; see [CPU_XADD.md](CPU_XADD.md).
Memory atomic ordering is preserved. Keep the pre-correction binary checkpoint
for matching older captures. The latest compute-shader failure confirms the
snapshot bridge works, but its origin was not watched. Neither this CPU defect
nor the earlier zero-count shift defect is established as the game's cause.

## Shader-prefix imported-copy watches

The follow-up uses 2,048 entries/four ways, cumulative overlap counters and a
failure-time numeric snapshot of the requested watch. Layout v1/256-slot logs
remain readable; they cannot supply cumulative totals. See
[watch semantics and limitations](SHADER_COPY_PROBES.md). Pending native coverage
must be checked with the rebuilt CPU executable and renderer together.

The owner requested improvements while pausing native runs. The launcher now
enables [shader copy probes](SHADER_COPY_PROBES.md), using a dedicated bounded
watch table and existing pointer journal copy channels (actors 6–9). Reports
require explicit matching capture/packet identity and time bounds. Direct
stores, aliases, frees/remaps and GPU writes remain outside coverage. This is
diagnostic evidence, not a shader or lifetime correction. Previous binaries are
preserved privately; new runs form another comparison series.

## Indexed-table SIGBUS probes

The private owner profile now includes [SIGBUS provenance probes](SIGBUS_INDEX_PROBES.md).
The numeric source/scaled indices are observation-only, with no dereference.
Prior profiles and executable are retained; this starts another diagnostic
comparison series. Keep the source-field qword's low32 interpretation and the
frame-local pre-store limitation explicit. Old reader/index and shader bind
probes remain active. No native coverage or correction is established yet.

## Shader binding probes

The owner requested additional shader diagnostics. The launcher now enables
[binding prefix probes](SHADER_BIND_PROBES.md), adding a separate bounded journal
and conservative bind-to-lookup comparison for metadata rejections. Preserve the
new journal with the other capture files. Older captures have no such evidence.
No native shader correction is claimed, and no agent game/GPU launch occurred.

## Fast startup requested by the owner

The owner launcher now defaults to the engine's `+nointro` option, through an
exact-input private startup profile and checked in-memory command-line
redirection. See [fast startup](FAST_START.md). Use `SOTC_SKIP_INTRO=0` for the
previous workload; do not mix those runs in the CPU comparison series. Native
game behavior is pending an owner run. Neither save files nor on-disk guest
instructions are modified.

## October 24 scalar CPU correction

The diagnostic now includes a hash-checked scalar interpreter companion as well
as the JIT companion. Both correct masked-zero 32-bit SHL/SHR/SAR zero-extension;
the original fixture failed in both backends before the correction. See
[the correction note](CPU_ZERO_COUNT_SHIFTS.md). The upstream checkout remains
pinned and unchanged. No game-fault causal link or native game validation is
claimed. Preserve matching pre-correction binaries from the sealed captures
when comparing new runs; changing CPU semantics starts a new comparison series.

Snapshot: 2026-10-23. This is the starting point for the next developer. Read
[AGENTS.md](../AGENTS.md), this page and [Build instructions](guide/building.md)
before changing code. [Status](STATUS.md) and [Graphics bring-up](GRAPHICS_BRINGUP.md)
contain chronological investigations; their earlier “latest run” sections describe
older boundaries, not the present state.

## Continuation checkpoint

### Reader/index capture active on base

The owner also requires [Box64 as a standing JIT comparison reference](CPU_TRANSLATOR_REFERENCES.md#standing-box64-reference-requested-by-the-owner).
For a suspected CPU defect, preserve a minimal original input, compare local
interpreter/JIT and Box64 where executable, then resolve disagreement with native
x86 or ISA semantics. Distinguish source review from actual independent execution.

Start with [Reader/index capture](INDEX_CAPTURE.md) and the current
[checkpoint](SIGSEGV_CHECKPOINT.md). Five caller and five base controls are
preserved. The final base stops on a PM4 assertion, not metadata. The new profile
preserves all original 22 pointer targets, adds two numeric observations and
reassigns eleven scalar channels to reader/index stages; capacity remains 28
scalar/24 pointer targets. Fixed journal mappings total 1204 KiB. The prior
profile/binary is retained privately. Base is now the default.

The CPU journal uses one previously zero payload word for native thread identity;
keep it distinct from its logical thread counter. Offline annotations require
matching pre-launch selection signatures. No automatic causal pairing of nearby
callbacks is claimed. Full contracts and final native synthetic reader controls
pass, including valid results and invalid-index faults in both backends. Native
game coverage/overhead remains pending; do not infer a correction from new timing.
No guest data, guards, protections or command acceptance changed.

### First caller native result preserved

The 21:21:08 UTC owner run has validated actual `caller` interpreter entry and
still exits 139 after 187.376 seconds outside that interval. Its fault instruction
and displacement repeat an earlier pre-variant traversal failure; prepared-input
hashes match. Initial-root versus loop-link source remains ambiguous and no writer
is retained. Read the [checkpoint](SIGSEGV_CHECKPOINT.md); do not infer a new
regression or exclude caller from one changed stop. Keep the active configuration
for repeated caller/base comparisons. No game was launched during this review.

### Controlled function comparisons implemented; native run pending

Read [Controlled CPU function comparisons](CPU_FUNCTION_COMPARISONS.md). Three
variants select one reviewed interval for the existing interpreter: `caller`,
`buffer` or `table`. `base` uses the normal diagnostic JIT; the private default
is `caller`. Exact prepared-input and whole-function hashes, executable-segment
containment and fixed labels are checked before execution. Translation stops
before selected instructions, preventing chained/cache bypass. Configuration is
immutable before execution. Calls outside the interval retain their backend.

Full synthetic contracts pass in JIT and interpreter modes, including nested
calls, conditional flags, preserved registers, cached blocks, concurrent threads,
invalid accesses and launcher/profile/report controls. The prior binary and
profiles are retained privately. The 22 pointer targets, 28 scalar probes and
bounded journal budget are unchanged. No native game/GPU run was launched or
corruption fix demonstrated. Read the validation record and first actual entry
marker in the next owner log; changed timing or a different failure is not proof
of correction. Faults inside the interpreter can lack translated guest context.
The following candidate-count section describes the preceding analysis stage.

### Candidate count before corrective experiments

The latest native fault is associated with a host memcpy call with null
destination; saved registers and a symbolized call continuation support this,
but its guest caller/producer is unknown. Two preceding SIGSEGV captures add an
invalid store with a matching terminal source member and a repeated null-object
read. All are archived independently.

The private inventory selects 20 unique code points in nine routes (15 function
entries, five interior/return sites), with no original writer proven. It is not
an exhaustive list of possible writers. The owner asks for this count before
trying direct changes; no corrective experiment has been applied for this step.
Start with the linked indexed-argument/buffer-owner/table-generator functions,
define the invariant each variant tests, and retain the baseline. Read the
[current checkpoint](SIGSEGV_CHECKPOINT.md) before changing behavior.

### Two subsequent SIGSEGV captures

The newest four-run review contains a repeated pointer-consumer SIGSEGV, a vertex
metadata stop, a distinct null-object SIGSEGV and a fragment metadata stop. The
repeated consumer/candidate return also occur in an older sealed capture. Its
caller loads an indexed argument and subsequently calls the function implicated
in the stack-protector case; only the static relationship is established. Observe
the indexed source before loading. For the null case, observe query/filter inputs
and results before attributing a missing object or type-check rejection to CPU
translation. The actual fault base is RDI; unrelated terminal RCX matches are
not its source. See the [checkpoint](SIGSEGV_CHECKPOINT.md) for evidence limits.

### Latest five completed captures

Read the newest [checkpoint section](SIGSEGV_CHECKPOINT.md): one stack-protector
stop, three metadata rejections and a PM4-parser assertion, with no SIGSEGV.
The previously interrupted preservation of the stack and vertex cases is complete
alongside the three newer captures. Exact-input static slices support a focused
stack-buffer/generator/consumer investigation; runtime count and writer remain
unknown. Guard source and local slot should be compared independently, and the
terminal import RIP must not be reported as the guest function responsible.

One compute failure retains an already unrecognized prefix before enqueue;
do not attribute its appearance solely to queue delay or accept a later shader's
metadata. The PM4 opcode rejection does not establish whether the stream is
unsupported or incorrectly read/modified. All captures use the signed 22-target
profile and leave the earlier five SIGSEGV analysis points unresolved. No code
change, new agent game run or protection change was made for this review.

### Latest native observation coverage

The two owner runs after the probe correction do not reproduce SIGSEGV: one
fragment-metadata exit (170.984 seconds, 23/23) and then a GPU-watchdog exit
(183.596 seconds, child 21 / supervisor 31). Complete captures are archived
separately by classification with matching binary UUIDs and five journals.
The four added probes all have native hits. Bank zero is full; array/member
writer-watch admission is frequently refused, so hits cannot establish writer
coverage. There is no new terminal guest fault to associate with a source word.
The five initial analysis points remain open. Preserve the current selection
for the next SIGSEGV comparison and read the latest
[checkpoint](SIGSEGV_CHECKPOINT.md) before interpreting these runs.

### Current entry capture and JIT assessment

Start with [JIT review](JIT_REVIEW.md) and the current section of the
[SIGSEGV checkpoint](SIGSEGV_CHECKPOINT.md). The latest saved owner run fails
after 141.614 seconds through an invalid incoming argument; its candidate returned
member was outside the old captured window. Preserve that uncertainty and keep
the separately sealed pre-rebuild binaries with this capture.

A confirmed diagnostic JIT omission skipped interior probes. A source-hash-checked
generator now ends translation before them and the private diagnostic archive
replaces its JIT object. All synthetic contracts pass in JIT/interpreter modes,
including loop, flags, cached-decode, array-entry and returned-member controls.
The active profile is now 22 pointer targets plus unchanged 28 scalar probes;
fixed journals total 1172 KiB. Prior profiles and a source/test checkpoint are
preserved privately. Shared bounded watch banks can refuse admission. Probes must
be registered before execution. Faults executing a probed instruction through the
interpreter may lack translated terminal registers; do not fabricate them.

The next owner launch tests observation coverage, not a demonstrated corruption
fix. No game/GPU run was performed for this preparation. Output defaults to
private permissions; memory protections and ordering are unchanged. The external
review found the old CMPXCHG fix already included and no compatible new fix in
orbis-on-macos; its retail support remains on the documented roadmap.

The following eighteen-target and earlier run descriptions are historical.

### SIGSEGV source investigation resumed

The later 136.143-second owner run reaches the following vector load in the same
array consumer. The object holds a readable array address, but its first element
contains the terminal invalid pointer; protected source read and displacement
agree. The prepared-input hash and consumer code match the earlier array case.
Count/destination operands are internally consistent in this run. An earlier
writer or shared lifetime cause is still absent. Full evidence and a comparison
are archived separately. An intervening vertex-metadata rejection is a distinct
exit-23 case, with matching eight-byte pre-enqueue/lookup prefixes but unbound
metadata; it is not another SIGSEGV. The owner is gathering more examples with
the same selection; no runtime/profile change or agent game launch occurs.

The subsequent owner comparison stops after 137.864 seconds in the separate
pointer-array family. The indexed referring entry and the source field are
retained two microseconds apart; the latter already contains the terminal invalid
pointer. Static loads, saved registers and a protected terminal read agree.
Several other operands in that numeric object are inconsistent, but no earlier
source value, writer or allocation lifetime is established. The source/table
targets are observation-only. All eighteen targets are configured; the selected
pre-load branch had zero hits, while caller entry/post-replacement and the atomic
consumer were reached. Preserve this as a separate verified archive and do not
attribute it to the atomic decrement. The next useful step is lifetime/writer
coverage for table insertion/replacement and its referenced storage, with explicit
watch admission rather than enabling every hot source blindly.

The owner resumed this investigation after the latest completed 105.418-second
SIGSEGV. A protected terminal read retains the invalid consumer pointer in a
readable caller slot. Static caller code and a candidate return are compatible
with loading that slot before the failing atomic decrement. No writer, ownership
or allocation lifetime is established. The private index retains three new final
captures, matching UUID binaries, all five journals, input hashes and offline
instruction/caller slices. All six saved capture archives were reverified.

The offline analyzer now retains terminal memory sources independently of earlier
probe history. Four original offline tests validate bounds, malformed/ambiguous
contexts, exact versus displaced accesses and deduplication. Ten original native
CPU cases compare valid/wrong atomic counts and pre-existing/direct/imported-copy
corruption in both JIT and interpreter modes. Existing twenty-one combined
diagnostic and fifteen pointer suites pass.

The active private selection has eighteen pointer targets, retaining the original
fourteen and adding caller entry, pre-load/call, post-replacement and register-only
consumer observations. Two new persistent source selections share bank zero;
check per-target admissions rather than assuming writer coverage. The prior
profile and allocation-failure alternative remain saved separately. Scalar
selection, pins and renderer behavior are unchanged. Journal storage with shader
submit tracing is 1108 KiB. This is preparation for a new owner capture, not a
demonstrated native correction. See the [checkpoint](SIGSEGV_CHECKPOINT.md).

### Bounded pre-enqueue shader-prefix capture prepared

The latest owner fragment-origin rejection still occurs with the corrected
compute packet framing. Its packet/register/lookup/descriptor observations agree;
a later self-describing block references the scanned metadata, while the supplied
origin remains unbound. Offline metadata-only review confirms this distinction
without executing shader/game instructions or relocating the live program.
The separately saved SIGSEGV captures remain deferred; their profiles are unchanged.

A new opt-in `SOTC_SHADER_SUBMIT_TRACE=1` diagnostic observes complete supplied
SET_SH_REG address pairs and eight shader-prefix bytes before enqueue, after the
existing optional command copy. Graphics and compute submission coroutines retain
these observations through decoding, including bounded nested-IB candidates.
Consumed packets publish their retained pre-enqueue observation again, so a long
queue delay or wrapped submission tail need not erase the matching sample.
There is no TLS scope across suspension, guest allocation retention, GPU readback,
shader repair or metadata acceptance change. The owner launcher defaults it on;
`SOTC_SHADER_SUBMIT_TRACE=0` or disabled diagnostic journals performs no new reads.

The fixed pool admits at most 16 live captures, each with 512 prefix samples,
4096 parsed packets, 262144 command words and four nested IB levels. Host pool
storage is below 512 KiB plus at most 16 shared-pointer control blocks. A separate
`shader-submit-events.bin` adds 132 KiB; the fourteen-probe profile now uses
1044 KiB of fixed journals. Existing file sizes, profiles and cache 28/9 stay fixed.
Unavailable reads, partial pairs, other layouts, repeated packets, budget/pool
refusals, failed output initialization and overwritten tails remain explicit.
The scanner observes candidate commands; it does not simulate conditional execution.

Offline review joins the active submission's consumed packet with retained
same-program/thread address-write and lookup observations. It distinguishes an
already unrecognized prefix, a changed sampled prefix at the same numeric origin,
and changed origins without inferring a content transition between different
addresses. Unavailable/repeated/foreign-PID evidence remains unknown. Eight-byte
agreement does not prove whole-program stability, intended identity, ownership,
a responsible writer or an atomic guest observation.

ARM64 compilation, all builder CPU regressions, 19 original submission-recorder
controls with address/undefined sanitizers, eight offline join controls, 26 build
orchestration tests and full JIT/interpreter CPU contracts pass. Guards include
real inaccessible input pages, both compute paths, nested/repeated/partial input,
concurrency, pool/budget saturation, disabled/failed output, retained evidence
after unrelated exit and consumption after submission-tail wrap. No game/GPU run
validates native usefulness or overhead yet. Runtime/CPU/reference pins are unchanged.

### Compute PM4 framing corrected; native shader origin unresolved

Review of the October 23 16:24:04 UTC owner capture found a reproducible defect
in the compute command reader. Type-2 padding was sized as type 3 before the
padding check. A buffered packet copied its entire remaining payload before
checking the supplied extent, lost earlier pieces when split again, and shared
ring continuation state with nested indirect buffers. Split packets could also
exceed the fixed temporary buffer. Original actual-source CPU controls reproduce
these failures, including a guarded out-of-range continuation read.

The checked local overlay sizes padding as one word, validates headers before
copying, appends only available continuation words, checks temporary capacity and
keeps complete indirect buffers independent of ring continuation state. Truncated
indirect and oversized split inputs fail explicitly before copying; supported
complete packets, two/multiple-piece packets, following packets and ring-cursor
wrap retain their original payload/order. The metadata association gate remains.
The pinned sources and private SIGSEGV profile are preserved.

The owner capture exits 23/23 after 148.171 s on an unbound compute origin. A
retained full address packet, its post-copy register, lookup and separate failure
descriptor agree; write-to-lookup observations are 24 us apart. No submission-time
shader-code snapshot or original writer is available. This capture does not prove
that packet framing caused its invalid shader content, and the correction is not
a demonstrated native shader-origin, SIGSEGV, PerVertexKHR or SRT fix.

ARM64 renderer compilation and all builder CPU regressions pass. New sanitizer
controls reproduce six old failures and preserve ordinary old behavior; corrected
framing passes those controls plus every two-piece split, a 1024-piece packet,
following packets, complete large packets and ring wrap. All 26 build-orchestration
tests, actual shader-metadata fixtures and full JIT/interpreter CPU contracts
pass. One initial supervisor-cleanup test failed; its isolated suite and the full
contract rerun pass. No game/GPU run is performed; native validation remains pending. Journal sizes, cache 28/9 and dependency pins
are unchanged.

### Compute shader address diagnostics corrected; native origin unresolved

The October 23 16:07:39 UTC owner capture rejects an unbound fragment program
after 146.057 seconds. Its supplied packet address, post-copy register, lookup
and separate failure descriptor agree; the write-to-lookup interval is 24 us.
The initial head matches the saved scan. Equal failure/trace files are copies
of one lookup observation, not submission-versus-consumption content evidence.
There is no submission-time shader snapshot or proven original writer. Keep the
metadata gate; do not relocate to neighboring programs or accept scanned identity.

Review also exposed a diagnostic layout bug: ComputeProgram starts with dispatch
fields, while its address words begin at byte offset 48. Corrected capture reads
that checked offset and uses register 0x20c for supplied packet coverage. New
compute actors 6/7 distinguish corrected observations from old actors 3/4, which
the offline reader keeps unknown. Failure descriptor context identifies the
compute offset; old compute contexts carrying the graphics label remain unknown.
Graphics capture, guest rendering, journal sizes and dependency pins are unchanged.
This corrects diagnostic evidence, not the native shader origin/content failure,
PerVertexKHR, SRT, or the paused SIGSEGV investigation. Original CPU fixtures cover
both compute paths, deliberate register mismatch, partial and guarded short input.

ARM64 renderer build, actual shader metadata/producer fixtures, 21 offline
diagnostic tests and the full JIT/interpreter CPU contracts pass. No game/GPU
comparison is performed; native validation remains pending.

### Shader address producer capture prepared

After the October 23 15:31:40 UTC owner capture rejected fragment metadata after
155.550 s, the trace now retains the address words observed in the supplied
SET_SH_REG packet, a separate post-copy raw program-address register observation,
and the later lookup address. Graphics/compute and alternate-decoder call sites
supply their bounded packet extent through a checked local overlay. Only the
recognized packet layout and a fully covered address pair are decoded; partial,
short or other-layout packets retain explicit unavailable pair evidence. No new
shader-code reads, renderer repair or register changes are introduced.

Existing write-event payload space holds the observations; journal sizes stay
unchanged. Required metadata failure now also snapshots the raw CPU program
object before the host history callback, with program/thread identity in context.
The offline origin review selects retained same-program/thread observations and
reports packet/register/lookup agreement separately. Older events remain legacy
and absent pairs are unknown. Observations can race; agreement does not prove
correct shader identity, allocation lifetime, unchanged content or writer cause.

Original CPU controls cover a valid pair, deliberately changed register, partial
write, guarded short packet, unrelated layout and evidence surviving exit without
printing. A callback mutating the descriptor confirms the earlier failure snapshot
is retained. Strict metadata association, the eight-case budget and paused SIGSEGV
profile remain unchanged. No game/GPU execution or dependency pin change occurs.

Public heads refreshed before the comparison include runtime
[4f7e727ad5a4](https://github.com/bloodborne-native/bloodborne-runtime/commit/4f7e727ad5a4cc54f7b4999df82de23245433e86)
(October 23 15:41:15 UTC). Checked A6 host-pass object ownership and host-specific
pipeline state; the author clarifies API validation ran with the upscaler off.
No guest shader-address or metadata-association fix is present in that delta.
Other four tracked heads are unchanged. No upstream implementation is imported.

### Shader metadata association gate applied

The owner requested prevention of incorrect cross-program association. Main-program
parameters now require the recognized self-describing prologue, an in-range
literal-derived metadata location, a valid signature at that exact location and
whole-dword code length of at least the prologue size, ending before the metadata.
A signature found by scanning can no longer provide executable code parameters,
length or hash for an unbound origin. Scanning remains bounded diagnostic evidence.
Unrecognized layouts are explicitly unbound rather than treated as valid programs.
This narrows supported lookup layouts; it does not prove stable guest ownership.

Unavailable speculative work still returns no parameters for owning-thread retry;
required unbound work stops with real exit 23 and an explicit association reason.
No draw is silently skipped by this gate, no shader is fabricated, and the origin
is never relocated to a neighbor. Candidate bytes/descriptors and eight-case trace
remain available, now labelled accepted=false. Association status is versioned in
lookup-event actor 2; earlier actor 0/1 journals retain unknown association status.
Journal sizes, private SIGSEGV selection and dependency pins are unchanged.

Original CPU controls cover missing prologue, conflicting/overflowed hints,
neighbor metadata, malformed/overlapping code extents, correct and distant hinted
programs, speculative rejection and required exit 23 with evidence. The privately
saved owner scanner input also passes an old/new CPU metadata-only comparison:
the previous helper accepts the borrowed neighbor identity; the corrected helper
rejects it, while an independently supplied self-bound neighbor remains accepted.
This comparison does not execute shader or game instructions. Native validation
is pending; a bad original program may now stop earlier. The underlying cause of
the missing/corrupt data, PerVertexKHR/SRT, and paused SIGSEGV remain unresolved.

### Bounded fallback shader trace prepared

Fallback shader lookups now retain the first eight distinct observed candidates
before compilation, so an unrelated CPU SIGSEGV no longer necessarily loses this
evidence. Each private case saves the exact existing scan snapshot, its metadata
slice, a separate raw CPU program-descriptor observation, role, PID/thread/time,
origin/hash/length, lookup/scan head comparison and bounded descriptor history.
The trace does not read guest memory again or assert that the descriptor snapshot
is atomic. Repeated keys are suppressed; eight slots are shared between graphics
and compute program types. Refusals, duplicates, output failures, admitted and
completed captures use existing shader-journal header space. No journal grows.

The offline report validates extents, rejects foreign PIDs, fingerprints the saved
bytes, checks metadata against the scan and compares a matching owned decode
window when available. It preserves incomplete-case and exhausted-budget gaps.
Original CPU controls verify survival of unrelated exit/source reuse, eight-slot
saturation across program types, duplicates, disabled tracing and preservation of
existing files. Rendering, lookup acceptance and real error exits are unchanged.
No shader/SRT/PerVertex correctness or SIGSEGV fix is claimed.

The latest owner comparison (October 23 15:07:41 UTC) stopped after 150.378 s on
CPU SIGSEGV, not shader decode. It is retained privately while SIGSEGV work stays
paused. There are 491 pipeline/805 SRT events; 427 pipeline events have adjacent
PerVertex rejection evidence. All 512 retained successful shader lookups use
hints. That bounded tail does not establish that all lookups were valid or that
the independently reproduced metadata-association gap is fixed.

Pre-comparison public heads now include runtime
[c4f72650e63d](https://github.com/bloodborne-native/bloodborne-runtime/commit/c4f72650e63da34c14fc64ac9442088589602d53)
(October 23 15:14:25 UTC). Checked A5 caller/pipeline shader holds and deferred
release, then A6 resource binding/descriptor-heap moves. These do not fix guest
program origin or metadata association. Shader ownership is a separate review
candidate; Metal-shadow session retention remains explicitly documented upstream.
Author frame/timing reports are not SotC ARM64/M1 validation. Other four heads
are unchanged; no pin or upstream implementation is changed for this trace.

### Metadata association gap reproduced; owner comparison unchanged

Original CPU-only fixtures now reproduce a lookup association gap: when a mapped
range starts at unrelated data but contains a later shader, the fallback signature
scan borrows that shader's length/hash while retaining the incorrect code origin.
An expected first token with a wrong metadata hint also permits this association.
Correct hinted origin, missing signature and unavailable-read controls pass with
address/undefined sanitizers; three metadata test groups pass. This confirms a
lookup weakness, not the native writer or a shared cause with SIGSEGV.

In the October 23 15:03:31 UTC capture, the failing vertex-program lookup is the
only fallback scan among 512 retained successful lookup observations. The lookup
head matches the owned decode head; this is two matching sampled words, not an
atomic whole-program stability guarantee. The new private evidence records the
metadata location and descriptor update before rejection. A valid signature does
not prove that it belongs to the supplied origin.

The owner is running another comparison. This check changes only tests and
documentation: the renderer library, private selections and pins remain unchanged.
Do not blindly require a first token, relocate code to a guessed start, skip the
rejected instruction or substitute another shader. Association validation must
handle legitimate fallback layouts explicitly; rejecting an unbound input alone
would not recover the correct descriptor. Native correction remains unresolved.

### Shader decode origin capture prepared; native correction unresolved

The October 23 14:51:15 UTC capture exits 23/23 after 98.845 s during fragment
program decoding. The rejected scalar-immediate word maps to a fork instruction;
its interpreted relative target is outside the caller-declared program length.
This is an input/length coherence clue, not proof of a particular corrupting writer
or permission to remove fork behavior. The previously reviewed vertex rejection
is a different program and format-table case. Both remain unresolved.

Fixed a diagnostic gap: decoder failures now report bounded descriptor write and
lookup history for the exact numeric program origin, and retain it beside the
already owned input before calling the host CPU-history hook. At most eight
matching program slots are copied, each retaining at most eight events; omitted
slots and absent history are explicit. No guest memory is read by this report,
and a numeric match does not establish ownership or stable allocation identity.
Lookup events also retain metadata offset, prologue hint and scan extent in
previously unused payload words with an explicit kind/actor marker. Legacy
journals keep unknown context. Existing journal sizes and dependency pins stay
unchanged; no fake shader, skipped instruction or stage substitution is added.

ARM64 renderer build and original CPU-only decoder/provenance controls pass.
The new controls cover exact-address versus foreign/missing matches, bounded
report overflow, retained metadata location and unchanged unsupported-fork exit.
The next owner run collects the missing origin evidence. PerVertexKHR/SRT and
the native shader stop are not claimed fixed. SIGSEGV investigation stays paused;
its private profile and checkpoint are preserved.

Before this comparison, public default heads were refreshed at 14:58:46 UTC.
Runtime advances through
[64c7c5f714f5](https://github.com/bloodborne-native/bloodborne-runtime/commit/64c7c5f714f597484e9e922a2f5c37dc4cd94d10),
October 23 14:57:13 UTC: adapter A5 moves shaders, vertex input and pipeline
creation behind the backend device. Checked descriptions, handle mapping and
deferred shader release; no decoder fork implementation or invalid guest-program
origin repair is present in this delta. The author's frame/performance reports
are not SotC ARM64/M1 validation. Other four tracked heads are unchanged. No
upstream implementation or new pin is imported for this capture.

### SIGSEGV paused; shader investigation active

On October 23, 2026 the owner paused SIGSEGV work and requested shader work.
The [paused checkpoint](SIGSEGV_CHECKPOINT.md) records the evidence, validated
instrumentation and unimplemented entry-capture proposal. Preserve the private
fourteen-probe profile and its hashed snapshot; do not silently switch profiles.
The invalid-pointer source and writer remain unresolved.

Start shader review with the retained vertex-program decode rejection at word
zero from the 14:34:05 UTC capture. The installed decoder classifies that word
as a scalar-immediate opcode outside its supported format table. Owned input
preserves bytes at decode time; it does not prove valid origin or guest lifetime.
Keep the 14:25:38 UTC graphics Thread ID assertion separate: its offending raw
descriptor and validated shader identity were not retained. Neither rejection
establishes a common cause with SIGSEGV or the PerVertexKHR/SRT failures.
No dependency, guest-data or rendering behavior change is made by this checkpoint.

Earlier checkpoints below are historical.

### Deferred shader assertion; owner continues SIGSEGV comparisons

The owner asks to retain the October 23 14:25:38 UTC shader problem for later
review while continuing attempts to reproduce SIGSEGV. This run exits 23/23
after 170.245 s on CalculateBufferAddress rejecting Thread ID buffer addressing
outside compute. 673 pipeline/805 SRT events, 1444 API presentations, no audio
queue failures. Unsupported legitimate stage use versus invalid/reused descriptor
or stage metadata remains unresolved. No assertion bypass, stage override or
fabricated thread/lane value is applied. Private logs, reconstructed diagnostic,
source/profile context and review questions are saved under ignored analysis.
The original report was missing because of an analyzer indentation error during
an ongoing edit; this is fixed and the report is reconstructed from retained data.

The active owner profile stays at the established fourteen probes/nine scenarios
with independent watch banks. An eighteen-probe allocation-failure profile is
preserved separately as an opt-in alternative; do not silently replace the active
SIGSEGV selection. Its added register-only mode records numeric arguments without
reading guest memory or arming watches; register values never become pointer
focus/reference evidence. Existing journal sizes with fourteen probes stay 912 KiB;
the optional eighteen-probe profile uses 976 KiB.

Instruction-failure diagnostics now retain all sixteen registers and bounded
field/history evidence before delegating to the unchanged upstream fatal body
and exit 30. A weak no-op notification supports standalone CPU fixtures; the
owner executable supplies the strong journal writer. Terminal kind 3 identifies
instruction failure separately from stack-protector and translated signal cases.
No unsupported opcode or guest INT is skipped or emulated. Offline reports now
retain Oodle, reported guest-pool allocation failures and interrupt-vector stops
in log order with displayed units; correlation is not a decoder or host-OOM cause.
The anomalous guest allocation/decompression cause remains unresolved.

### Independent watch banks and new linked-source capture

The latest owner capture (October 23 13:57:38 UTC) stops after 116.513 seconds
on another linked-structure read, through an invalid stored pointer plus eight
bytes. The final node is readable; its source field is captured in the terminal
snapshot, but no earlier observation or responsible writer survives. The previous
allocator-focused selection still filled all 8192 shared slots and refused
278,933 admissions, including every reached original header target. The prior
isolation did not eliminate starvation.

Persistent storage is now four disjoint banks of 2048 slots: original headers,
selected allocator links, following allocator links and the newly failing linked
source. Total capacity remains 8192; a bank can fill or collide independently but
cannot consume another bank. No eviction, fallback borrowing or invented earlier
history is introduced. Existing selections default to bank zero (2048 slots);
optional @0..3 selects a bank after the full pointer-probe entry. Watch records
identify their bank; existing header space retains per-bank admissions/refusals.
Older journals remain readable with unknown bank metadata, not invented zeros.
Small imported copies inspect active-bank buckets; large copies scan the bounded
combined table. Both paths preserve bank attribution and existing write ordering.

The private profile retains twelve earlier probes and adds two source probes,
for fourteen targets/nine scenarios. Array/helper observations remain active;
allocator and new-link fields request their own persistent banks. The pointer
journal now has sixteen channels, adding 32 KiB (all journals 912 KiB). Watch
storage remains 642 KiB plus 32 bytes of bank-usage counters. Guest behavior,
permissions, rendering settings and dependency pins are unchanged.

Fault review now separately reports terminal-register source candidates and a
signed numeric displacement, so a fault at a stored pointer plus eight is not
mistaken for absent pointer evidence. Displacement is not proof of the operand;
ambiguous/incomplete register text is not used. This is an instrumentation change,
not an established fix for native corruption or guest allocation lifetime.

Validation: full ARM64 JIT/interpreter CPU contracts pass. Thirteen pointer
tests cover isolated saturation/collision, concurrent admission, per-bank small/
partial/large copy capture and fourteen pointer plus 28 scalar channels. Sixteen
report tests cover legacy evidence, offset candidates, exact matches, missing
register negatives and foreign PIDs. All fourteen private signatures match the
prepared input. These are diagnostic controls; native effectiveness remains
pending an owner comparison.

Public default heads refreshed 2026-10-23T14:07:31.861451+00:00: runtime advances by
three adapter A4 commits, ending at
[ae84941d2f76](https://github.com/bloodborne-native/bloodborne-runtime/commit/ae84941d2f76c1620e41fbe50196bdaf5e1bcfb8)
(October 23 14:06:51 UTC). Checked dynamic-state/pass conversions and recorded
command captures; no demonstrated guest-link corruption fix was imported.
Author Metal performance/frame reports are not SotC ARM64/M1 validation. Other
four heads are unchanged; prior detailed PR/release limits remain.

### Allocator-link capture and recommended investigation workflow

The October 23 13:39:40 UTC owner run stops after 126.462 seconds on an
allocator traversal through an invalid link. The same numeric source word has
retained earlier and later observations with different values, separated by
404.567 ms. Two words match content observed in another failing object in the
preceding capture. This is a shared-content clue, not a demonstrated writer,
allocation identity, asset origin or shared cause. All terminal registers and
the reconstructed report match the original evidence.

The prepared profile now has twelve signature-checked pointer targets and eight
scenarios. Two added persistent probes watch selected/following allocator links.
The array-source probe temporarily observes only, preserving its source/referrer
samples while giving admission priority to the allocator investigation. Helper
probes remain observe-only. Existing header watches remain persistent. The 8192
slots and bounded hash can still refuse admission; per-probe counters expose it.
No guest data or permissions are repaired, no game is run automatically.

Offline fault_field_transitions reports earlier different and later terminal-
register-matching values for the same readable numeric field. It states the
observation interval without treating the earlier value as validated or claiming
continuous writer coverage. Pointer journal maximum grows from twelve to fourteen
channels; the active twelve-probe profile adds 32 KiB, all four files total 880 KiB.
Host watch/frame budgets and dependency pins stay unchanged.

The [memory-error workflow](MEMORY_ERROR_WORKFLOW.md) is the preferred and most
productive approach used so far in this project: retain evidence before failure,
match the actual fault to its source, narrow a change interval, and validate
instrumentation with original CPU controls. This is project experience, not a
universal ranking or a claim that the native corruption is fixed.

Validation: full native ARM64 JIT/interpreter CPU contracts pass. Twelve pointer
tests cover twelve pointer plus 28 scalar channels and guarded invalid/valid
returned pointers; sixteen report tests cover same-field transition extraction,
refused admission, unrelated focus and foreign-PID negatives. All twelve private
signatures match the prepared input. Reanalysis of the latest native evidence
extracts the 404.567 ms interval automatically. These are diagnostic controls,
not a demonstrated native correction. Public source/link audit passes.
Public default heads refreshed 2026-10-23T13:47:43.033893+00:00: all five match the prior
review; detailed PR/release limitations remain. No dependency update or native
GPU/game run was performed.

### Terminal source watch isolation and fault-specific review

Latest owner capture, October 23 13:23:34 UTC: SIGSEGV after 221.978 seconds
at the earlier array read. A retained referring-table entry points numerically to
the fault object; its source field already contains the actual invalid read
address before the first iteration. Related count and destination arithmetic
agree with terminal registers. No writer, lifetime or common cause is established.
The original combined report matches reconstruction and all 16 terminal registers.

The persistent watch table filled all 8192 slots, refused 15,825,145 admission
attempts, and did not admit the terminal source field. These are attempts, not
unique rejected fields. The first suspicious copy is a different numeric object,
87.020383 seconds earlier, not demonstrated to cause the fault.

The prepared profile now makes the three additional helper/caller watch probes
observe-only, as the fourth already was. All ten probes and seven scenarios
remain; helper observations no longer compete for persistent source/header
slots. This does not guarantee that the remaining original sources cannot fill
the table. The allocation budget and journal sizes remain unchanged.
New journals report persistent-admission refusals per probe in existing header
space. Offline fault_pointer_review separates exact stored-value/fault-address
matches, candidate referring entries and related scalar samples from the earlier
heuristic focus, and explicitly flags refused terminal-source admission. Numeric
matches are not allocation identity, validated ownership or exclusive writers.
Legacy journals remain readable; missing counters are not interpreted as zero.
This corrects capture competition/reporting, not the unresolved native corruption.

CPU-only validation: twelve pointer tests pass in JIT and interpreter, including
hot observe-only helpers preserving source admission and actual copy capture,
exact-address faults versus offset faults, and ten pointer/28 scalar channels.
Fifteen report tests include unrelated first-focus separation, per-probe refusal,
and foreign-PID exclusion. All ten private signatures match the prepared input.
No game/GPU test was launched, no pin or rendering setting changed.

Full ARM64 CPU contracts pass in JIT and interpreter.
Public default heads refreshed 2026-10-23T13:33:52.759844+00:00: all five match the preceding review; prior detailed PR/release limits retained.

### Returned-pointer source capture prepared

Prepared a returned-pointer investigation after the October 23 13:10:01 UTC
owner capture: SIGSEGV after 97.831 seconds at a different consumer. The saved
instruction reads a field through an invalid returned pointer; the preceding
helper returns a stored member. This identifies a source path, not its writer
or object lifetime. The complete diagnostic matches offline reconstruction.

The private signature-checked profile now retains ten pointer targets: all six
previous targets plus caller source, helper entry, stored-member return, and
consumer observations. Independent channels preserve the earlier scenarios.
Protected reads never chase an unreadable result or repair guest data. The
consumer observation reads a bounded eight-byte word; it does not model the
failing instruction's four-byte operand or provide an atomic snapshot.
The pointer journal supports up to twelve channels (ten probes and two imported
copy channels), adding 64 KiB over the earlier profile. All four journals total
848 KiB with this profile; persistent watch and task mapping budgets are unchanged.

A first suspicious imported-copy observation in this capture belongs to a
different numeric field, about 57.57 seconds before the terminal fault. Offline
reports now expose copy overlap, relative offset, candidate source field and
previous-value change explicitly. A persistent watch can outlive its original
allocation; legal initialization can replace a former pointer with non-pointer
data. This evidence does not prove corruption, exclusive writer or a connection
to the terminal fault. Guarded CPU-frame history has no retained matching range;
that does not exclude other lifetime bugs. No native fix is claimed.

Validation: full native ARM64 JIT/interpreter CPU contracts pass, including
11 pointer tests (ten pointer plus 28 scalar channels together, valid/invalid
returned-value controls), 14 report tests and six journal tests. All ten private
probe signatures match the prepared input. Public source/link audit passes.
No game/GPU test was launched and no dependency pin changed.

Pre-test public default-head refresh at 2026-10-23T13:21:42.192827+00:00 finds one runtime commit:
[dda931c5779b](https://github.com/bloodborne-native/bloodborne-runtime/commit/dda931c5779bdd6e437baa224d9de94911426aba),
October 23 12:56:02 UTC, adapter A4 flat binding slots. Checked code converts
bindings directly into deferred recording storage with Vulkan layout assertions.
Author Metal timings/validation are not SotC ARM64/M1 validation; no object-lifetime
repair is established or imported. Other four checked heads are unchanged;
prior detailed PR/release review limits remain.

### Native bounded CPU-frame guard and authoritative protected-reader checks

Added an opt-in native CPU coroutine-frame guard, inspired by the reviewed
SotC quarantine technique but implemented for macOS. The renderer's actual Task
promise allocates independent mappings when SOTC_TASK_GUARD=1. Existing ordered
recording-consumer retirement remains unchanged; deletion makes that frame's
pages PROT_NONE and retains up to 64 frames within an 8 MiB total live/quarantine
mapping budget. This does not retain or change guest objects/pages or GPU
resource ownership. Budget/slot/allocation exhaustion uses normal new/delete,
explicitly recorded as unguarded fallback. Protection/unmap failures preserve
owned mapping accounting and report incomplete coverage; no false release.

A separate task-events.bin retains 256 allocations/fallbacks and 256 retirement/
unmap/failure events, with address, rounded/requested size, generation, thread,
current budget and counters. Offline diagnostics match a fault address to the
latest retained mapping event only as a candidate, excluding foreign producer
PIDs. Reuse/finite retention, fallback and eviction can hide lifetime bugs.
The journal adds 68 KiB (all files 784 KiB when enabled), plus bounded host
metadata and at most 8 MiB of frame mappings; not a total process-memory limit.
No native corruption correction or overhead measurement is established.

Runtime default remains disabled; the owner diagnostic launcher defaults the
guard on, with explicit SOTC_TASK_GUARD=0 opting out. All persistent source/header
and indexed referring-object probes remain active; the guard tests CPU renderer
frames, not whether the failing PS4 object is alive. The new command explicitly
sets SOTC_TASK_GUARD=1. No game/GPU test was launched.

Original CPU controls use the actual production Task/retirement helper: valid
ordered consumers pass, premature and later stale reads trap on real protected
pages. Budget fallback, quarantine rotation, concurrency, injected protection/
unmap failures and stale-PID rejection are checked. Existing delayed-consumer
ASan controls remain. Repeated native protected-reader tests confirm PROT_NONE
reads fail without changing kernel permissions. The renderer builds ARM64 with
all builder CPU regressions; cache remains 28/9.
Full ARM64 JIT/interpreter contracts, nine production-guard cases, ten pointer
tests, four protected-reader tests, fourteen report tests, twenty-six builder
tests and 46/46 host-symbol checks pass. These are CPU-only validations.
 Exact task_guard_previous cohort
migration and idempotence checks pass. Dependencies stay pinned.

Pre-test public refresh October 23 12:36:59 UTC finds one new runtime commit,
e093d2151f216720be61eb7855dcfb52a1407e84 (12:20:19 UTC): access-declaration adapter
refactoring. Its diff is reviewed and not adopted as a lifetime fix. Other checked
default heads unchanged; previous PR/release and object-method limits retained.

### Public diagnostic-method review while native comparison is pending

Reviewed SotC issue #274: an older Linux report reaches the same array-read
instruction with a null cursor/nonzero count; its newer-build failure is a
separate teardown boundary. Open, no comments/fix or deterministic reproducer.
Reviewed guarded coroutine-frame quarantine, stale speculative readback controls
and the October 23 AnyPS5 kernel/logical-permission mismatch against actual code.
Read [the detailed review](UPSTREAM_REVIEW.md#object-corruption-diagnosis-methods-public-review-2026-10-23)
for exact SHAs, author/control distinctions and synthetic follow-ups. No native
shared cause established. The persistent/reference profile and owner command
below remain unchanged; no code/dependency changes or game launch.

### Prepared persistent object-field evidence and indexed reference capture

Prepared persistent numeric-field watches and a referring-table object probe.
Private review of the last native capture finds two retained objects per old
header channel, but 128 distinct objects per source/inner-loop channel. The
failing object has one source observation and four related scalar samples, with
no earlier valid state retained. Checked caller instructions obtain the child
from an indexed table before passing it to the failing consumer. The previous
capture does not contain that table slot/base, so ownership/lifetime is unproved.

The four header probes and array-source field now request persistent admission.
An independent fixed 8192-slot, bounded-hash table never evicts an admitted
numeric field. Rotating watches stay separate. Capacity/hash conflicts and
contention refuse admission explicitly; events distinguish requested policy from
successful admission. Fields reused by later allocations remain the same numeric
key, not a new allocation identity. No guest object is retained, freed, repaired
or authorized for use by this diagnostic. Direct/GPU/alias/pre-admission writes
remain outside the imported memcpy/memset observers.

The first suspicious-pointer snapshot now retains the previous completed value/
time for the same admitted numeric field when available, surviving event-tail
wrap. An initially bad value has no invented earlier good state; concurrent or
missing history is explicit. The sixth pointer probe now captures an indexed
referring-table entry at a reviewed caller block boundary, with checked index
multiply/add and saved index register/value/scale. It replaces hot per-entry
inner-loop observations. Candidate reference reads may race the actual call;
reports list numeric referrers and related scalar fields without claiming live
ownership, object type, free/reuse or exclusive writer evidence.

Small imported copies inspect bounded hash buckets; a conservative page filter
can reject unrelated ranges, while large candidate copies scan the fixed table.
Existing pre-write preparation, actual copy and post-write notification order
is preserved. Fixed journal files remain 716 KiB; the persistent table/filter
reserve an additional 642 KiB of host static storage. Native runtime overhead
and corruption correction remain unvalidated. All six scenarios, 28 scalar and
six pointer targets, 16 fatal fields and other failure-family capture remain.
The updated private profile checks the new caller/source selections and code
signatures against the actual prepared input; same owner command loads it.

Original JIT/interpreter controls preserve the source watch after 64 unrelated
fields evict the rotating control, then capture the actual memcpy transition
and previous value. Partial/large copies, initially bad/direct-store negatives,
indexed references, index overflow, observe-only mode and explicit admission
status are tested. A bounded-hash collision refuses the seventeenth key without
evicting sixteen admitted keys; concurrent lookups/history preserve unique keys.
Ten pointer tests, full ARM64 JIT/interpreter contracts and 46/46 symbol checks
pass. These test diagnostic behavior, not native object validity or deterministic
recurrence. Public default heads checked October 23 12:12:56 UTC unchanged;
prior PR/release review limits retained. No dependency/cache/render changes,
automatic retries or game launch. Native writer and object lifetime unresolved.

### Owner capture: bad array source observed before the failing read

Latest owner capture (October 23 11:55:45 UTC / 13:55:45 Madrid) reproduces the
same pointer-array read SIGSEGV after 163.541 s, exits 139/139. The first suspicious
pointer-field snapshot now captures the actual source field before the cursor
load. Its object matches the fault's object register and its stored value matches
the invalid read address. Four separately sampled source/count/range fields for
that same object survive: count agrees with the unchanged fault counter, and
source-difference/destination-base arithmetic agrees with the destination register.
This localizes bad metadata before the first array read in this invocation; the
following vector copy is not reached. It does not identify a writer, exact object
type, allocation identity, earlier valid state, free/reuse or a common cause with
the other trees/shaders. Source values are individual observations, not atomic.

All six scenarios and all 28 scalar/six pointer targets execute. Profile/input
fingerprints and all 16 terminal registers match. The first suspicious pointer
survives, along with four same-object scalar records; no earlier valid observation
for this object survives. New scalar channels cover only 0.821 ms each. The watch
table rotates 27,597,802 fields, with 482 watch contention drops; zero captured
imported-copy overlaps cannot exclude an earlier overwrite. Pointer journal has
155 busy/delayed drops; CPU has three. No incomplete/stale retained records.
3584 CPU/1024 shader/516 pointer records survive; CPU/shader union spans 18.683 s/
17.042 ms are not full-run coverage. Text CPU history is partial (423 events).
1631 API presentations, 496 pipeline/805 SRT failures; 431 pipeline failures
adjacent to PerVertexKHR, 65 unattributed. Last/peak native allocation samples
10.880/10.882 GiB, no recorded OOM/large copies or memory observation errors;
not controlled savings. Audio 62 windows/no queue-wait failures.

The preceding 11:53:11 UTC capture stops after 129.202 s, exits 23/23, on compute
shader unknown-instruction-format rejection inside an owned 1295-word input.
The 5180-byte saved input contains the reported failing word at offset 886;
invalid code/source lifetime versus unsupported format remains unresolved.
All six scenarios also execute, with 21,985,562 watched-field replacements and
zero copy overlaps; no first out-of-domain pointer captured. 1320 presentations,
433 pipeline/805 SRT failures; last/peak samples 10.198/10.202 GiB. Audio 48 windows/
no queue-wait failures. Both diagnostic reports exactly match reconstruction.
Next: object creation/initialization/reuse and writes to its source/count/range
fields, with persistent selected-object watches isolated from hot per-entry
rotation; retain other failure-family evidence. Avoid claiming no writer from
zero overlaps or replacing invalid metadata. Analysis only: no runtime/config/
dependency changes, native game launch or additional GPU test.

### Prepared simultaneous tree and pointer-array analysis

Prepared six simultaneous private analysis scenarios for allocator insertion,
ordered comparator traversal, allocator source links, allocator search children,
container header lookup/insertion and the newly failing pointer-array traversal.
The native bug remains unresolved: the last fault proves an invalid array read,
not its writer, source iteration, ownership or a common cause with other trees.
No replacement pointer, skipped instruction or fabricated success is introduced.

The joint selection preserves all previous 24 CPU observations and four chain
probes, adding four source/count/range field observations and two pointer-valued
field probes at the new traversal's entry/loop boundary. Pointer-field mode
reads one selected word without following its value, retaining already-invalid
source pointers and watching readable source/current-entry fields for imported
memcpy/memset overlap. It does not interpret vector payload as a pointer.
Fatal fields expand from 13 to 16. Current fixed mappings total 716 KiB (452 CPU,
132 shader, 132 pointer), up 96 KiB; native overhead remains unmeasured.

The generic CPU limit is 28 channels; counters end before terminal context at
header word 128, enforced at compile time. Pointer mode's optional fifth word
is 1 with a zero inner offset; existing four-word chain selections remain valid.
Reports distinguish pointer fields from header chains, and list per-scenario
observed/unreached/unavailable targets. An absent/foreign journal or mismatched
selection is not accepted as observed coverage. The private reviewed profile
now verifies signatures and exact selections for both CPU and pointer targets
before game execution; saved digests bind scenario indices to actual settings.
Game offsets/signatures/configurations stay ignored and are not compiled in.

Original guest controls in JIT/interpreter observe all 28 CPU and six pointer
targets concurrently, distinguish imported-copy mutation from direct stores or
an already-bad initial value, and preserve separate terminal space. Unreached
scenario targets are explicit, and changed field selections/missing signatures
are rejected. Interpreter host faults do not invent translated GPR evidence.
Existing shader/fiber/stack-protector/watchdog capture remains enabled. Writers
outside imported memcpy/memset, earlier writes, aliases, allocation reuse and
GPU writes remain outside the scoped watch. No deterministic native recurrence
is promised, no automatic game sequence is installed, and no game was launched.
Full ARM64 JIT/interpreter contracts, six pointer tests and 46/46 symbol checks
pass; public source/link audit passes. Public default heads checked October 23 11:50:11 UTC unchanged; prior PR/release
review limitations retained. Dependencies, renderer/cache and game settings fixed.

### Owner captures: new array-read fault and saved fragment metadata rejection

Latest owner capture (October 23 11:36:25 UTC / 13:36:25 Madrid) ends in CPU
SIGSEGV after 137.464 s, exits 139/139. Checked prepared-image bytes identify a
pointer-array read through an invalid register, matching the fault address.
The initial array pointer comes from an object field, then advances per loop;
that source field/iteration was not captured. No writer or allocation lifetime
is established. This is a different instruction from the previously traced tree.
All 16 terminal registers match text and the prepared image fingerprint matches.

New pointer capture works natively: all four targets hit (448/448/2/2), 260
records retained, two fields armed, no watch replacements/contention, no imported
copy overlaps and no first out-of-domain-link observation. All retained chain
reads succeed. This does not exclude unobserved direct/GPU/earlier writes or
corruption outside those two fields. CPU/shader files retain 3072/1024 records,
zero busy/incomplete slots; their union spans are 28.558 s/0.865 ms, not whole-run
coverage. CPU text is partial (702 events). 1297 API presentations, 606 pipeline/
805 SRT failures; 525 pipeline failures adjacent to PerVertexKHR, 81 unattributed.
Last/peak native allocation samples 10.301/10.314 GiB, not controlled savings or
fault-time pressure. Audio 52 windows/no queue-wait failures. No OOM reported.

The preceding 11:34:00 UTC capture stops after 126.254 s, exits 23/23, on required
fragment shader metadata rejection. The exact 65564-byte scanner input is saved,
its head matches the initial read, and no metadata signature is present. The
retained descriptor history shows an address-register write followed by the
failed lookup, with earlier different-address lookups successful; this does not
prove a bad packet, overwrite, lifetime or visibility cause. Pointer targets hit
456/456/2/2, likewise no overlap/out-of-domain evidence. 1253 API presentations,
457 pipeline/805 SRT failures; native last/peak sample 10.807 GiB, audio 48 windows/
no queue-wait failures. Both diagnostics exactly match offline reconstruction.
Next: bounded provenance of the new array source/count/range and its writes,
plus the failed shader's address packet and backing lifetime. Retain joint
capture; no demonstrated shared cause. Analysis only: no runtime/config/pin
changes or game launch. Earlier checkpoints below are historical.

### Prepared header-chain, scoped imported-write and first-bad-link evidence

Prepared targeted tree-header tracing, with deterministic original synthetic
controls rather than a promise that the intermittent native crash will recur.
Four private, signature-checked function/call-return entries observe container
header and node-link values before the failing lookup and around insertion.
They use an independent pointer-events.bin with six channels/128 records each,
a first out-of-domain-link slot, and sixteen bounded watched link fields. Current
combined mappings total 620 KiB. Existing 24 CPU observations, terminal registers,
shader/fiber/stack-protector/watchdog coverage remain active.

Optional imported memcpy/memset observers retain destination/source or fill byte,
length, before/after field values, thread and a protected return-address candidate
when available. They run after existing pre-write preparation and before existing
post-write notification; guest copy bytes/returns/order are unchanged. Direct
scalar/vector guest writes, GPU writes, other aliases, earlier writes and evicted
watch fields are not intercepted. Registered container/header relations are not
reread or validated as current ownership during a copy. There is no demonstrated
native writer, allocation lifetime or corruption correction yet.

The first readable link outside the 40-bit guest domain after ignoring an ARM64
top-byte tag is a heuristic, not an actual access failure or object-type test.
It is retained before termination and survives hot-tail wrap or later restoration.
The original program is never stopped/repaired based on this heuristic. The
report exposes per-target hit counts, absent/unreached coverage, watch replacement/
contention and explicit writer limits. New owner command requires the private
selection/profile; missing inputs stop before launch and code-signature mismatch
rejects the prepared image before game execution. No automatic game retry.

Actual original guest controls in JIT/interpreter capture the chain before a
real invalid-link flag-read SIGSEGV, distinguish imported memcpy/memset writers
from a direct-store negative control, capture one-byte overlap, preserve first
bad evidence through 600 later valid copies, and survive discarded stderr.
Valid/zero/nonoverlap/top-byte-tag controls do not invent a failing access;
missing target hits and mutated signatures are reported/rejected. Full CPU
contracts, diagnostics/report tests and symbol checks validate instrumentation,
not native causality or tracing overhead. Public default heads checked October
23 11:22:38 UTC unchanged; earlier PR/release limits retained. Pins/cache 28/9
and rendering settings unchanged; no game launched. Read first handoff checkpoint.

SOTC_CPU_POINTER_TRACE is a private, bounded list of up to six
site:register:signed-offset:signed-inner-offset observations. The first protected
read obtains a header from register+offset, the second a link from header+inner.
All configured PCs must be executable; current signatures are also compared with
the actual newly prepared linked payload using tools/check_pointer_profile.py.
The chosen lookup return is a real basic-block entry before the faulting loads,
not an arbitrary interior PC. Original guest/JIT/interpreter controls exercise
the corresponding call-continuation probe and actual fault, not just a parser.

SOTC_REQUIRE_POINTER_TRACE=1 rejects a missing private selection/profile in the
owner launcher. Explicit empty pointer selection under this required mode fails;
it is not an unnoticed opt-out. Default selections are read as data, never eval'd.
The matched profile/selection and prepared-input fingerprints are saved privately
in pointer-profile-check.json and included in the combined report.
The exact native failure still cannot be forced or guaranteed without changing
the workload or inventing a fault. Read target_probe_hits after the next run;
configuration alone is not proof that a target was reached. Other stop categories
retain their separate evidence if they occur first.

The pointer journal has a first out-of-domain link slot at reserved header words
160..172, independent of rotating probe/copy tails. Interrupted publication is
explicitly incomplete; a producer-PID mismatch excludes the focus snapshot too.
Pointer status bits 1/2 describe source-address/read evidence, 4/8 link-address/
read evidence; bit 256 identifies a protected return-address candidate. Copy
records set no source-read bit: their container/header relation comes from the
watch registration, not a fresh ownership check. Actor/source/size/value/thread
fields are numeric CPU observations. A copy overlap is not exclusive-writer proof;
preexisting bad values are not attributed to a later copy. Low 56-bit domain
selection preserves the original 64-bit value and never changes its execution.

Watch arming begins at observed chain reads and is bounded to sixteen numeric
fields. Replacement, contention, unobserved earlier initialization, address reuse,
GPU guards and non-intercepted stores leave explicit limits. The tracker neither
changes page protections nor replaces the renderer's own write-watch callback.
The scoped copy hooks preserve existing pre/post CPU/GPU notification order.
Private code/profiles, selections, snapshots and any native logs remain ignored.


### Native terminal registers retained; another tree header has an invalid link

Latest owner capture (October 23 11:03:52 UTC / 13:03:52 Madrid) ends in CPU
SIGSEGV after 86.457 s, exits 139/139. It fails in a different tree lookup:
checked linked instructions load a header from a container, load a node pointer
from the header's second word, then read a byte at node plus 25. That stored
word matches the invalid pointer in the fault registers; the computed byte
address matches the fault. Header/container are readable in bounded fault reads,
not validated objects/lifetimes. No writer, exact object type or common cause
with allocator search, shaders or stack protection is established.

The new terminal journal works natively: all 16 saved registers match text;
kind 2 identifies translated-fault evidence. Prepared probe/renderer UUIDs match.
All 24 CPU channels retain 128 records (3072 total), shader channels retain
1024 records, with zero busy drops/incomplete slots and matching producer PID.
CPU union span 24.168 s is not continuous coverage; no retained numeric match
for this header/container/invalid pointer. Current probes target the preceding
allocator/comparator boundaries, not this lookup. Shader tail only 4.018 ms;
all 512 retained lookups find metadata, not validation of ownership/code or the
whole run. Text CPU dump remains partial: 455 events/401.692 ms, no footer.
27,271,510 CPU and 4,113,464 shader reservations show extensive finite-tail wraps.

915 API presentations, 325 pipeline/271 SRT failures; 283 pipeline failures are
adjacent to PerVertexKHR errors, 42 unattributed. Last/peak native samples
9.263/9.264 GiB, no recorded OOM/large copies, memory drops/errors zero; not a
controlled memory reduction. Audio 32 windows/no queue-wait failures. No shader,
fiber, stack-protector or GPU-watchdog stop recorded; prior causes remain open.
Diagnostic matches reconstruction. Next: header initialization/reuse/lifetime
and writes to its node-link field through the newly localized access, retaining
joint failure capture. Do not reset links or skip validation. No runtime/config/
dependency changes or game launch during analysis. Read first handoff checkpoint.

The stored link is observed both in a protected header read and in the captured
source register. Current linked bytes and the fault-address arithmetic are
checked privately. This is evidence of an invalid link at consumption, not the
write that introduced it. Invalid object origin, lifetime/reuse, wrong layout,
bounds/overwrite and synchronization remain alternatives. Different fault sites
and similar-looking packed words do not demonstrate a shared writer or data type.
The current finite per-site tails contain none of the relevant addresses/values;
no-match does not establish an earlier valid state. The next review should target
header provenance and writers, with branch-entry/read context if needed, rather
than treating the preceding allocator probe profile as exhaustive coverage.
No diagnostic/code/selection changes were made during this analysis.


### Prepared search-link evidence and loss-resistant terminal registers

Prepared allocator-search follow-up and corrected terminating-context loss.
Six new private observations cover the actual search root, next-node header/
children and left/right-load sources, at current linked branch entries. The
joint profile now uses 24 observations; earlier insertion/comparator/source
coverage and 13 fatal fields remain. CPU channels retain 128 records each,
shader channels 512 writes/512 changed lookups; combined fixed files are 520 KiB.
No invalid pointer, opcode or failure is skipped; the native writer/cause remains
unknown, so this is not a demonstrated correction of allocator corruption.

A concrete diagnostic defect is corrected: the guest stack-protector handler
previously printed one line then exited 22 without caller/register evidence.
It now snapshots only an existing active TLS CPU, publishes bounded raw import
registers to reserved CPU-journal header space before stderr, and invokes an
optional host context/history reporter. Standalone binaries retain a weak
fallback and exit 22. No CPU creation, decoder access, return-slot edit or assumed
canary layout is used. Translated fatal faults also publish their available
registers before rich mapping/provider diagnostics. The first committed terminal
snapshot is retained; interrupted publication is rejected. Interpreter faults
outside translated code are not promoted to exact fault-register snapshots.

Actual synthetic guest import controls preserve exit 22 and raw context in JIT/
interpreter with drained, discarded, full and closed stderr. Actual translated
fault controls preserve exit 139 and terminal registers with discarded stderr;
CPU per-site evidence remains available in both modes. Full ARM64 CPU contracts,
renderer CPU regressions and symbol checks validate diagnostics, not native
corruption, memory savings or tracing overhead. Public default heads checked
October 23 10:59:12 UTC unchanged; earlier PR/release limits retained. Pins/cache
28/9 and render settings unchanged; no game launched. Same combined owner command
next, with private selections unset in the inherited environment so the updated
profile loads. Read first handoff checkpoint.

The new search observations are generic configured fields, not compiled game
addresses or guest instruction patches. The six additional selected entries
were checked against current linked bytes and their conditional branch targets/
fallthroughs. They observe the initial root, a candidate before its header read,
and parent link fields before left/right descent; they do not identify all writes
or allocation identity. All guest selections/disassembly remain private.

The CPU journal reserves header words 128..150 for a first terminal snapshot:
kind 1 is a raw stack-protector import, kind 2 a translated fault with available
registers. A bounded lock-free publication precedes stderr/context/history work;
no final flush, file open, recorder mutex or pointer dereference is needed to
publish it. Numeric RIP/RSP/RBP, thread, FS base and stack top are observations,
not validated callers/canaries or a fiber identity. Register text and protected
frame candidates are separate best-effort evidence. An interrupted first
publication remains explicitly incomplete rather than being replaced by another
reporter. A PID mismatch excludes terminal context along with journal events.

The diagnostic reporting repair does not repair the observed guest corruption.
Do not infer a cause from a valid root plus an invalid child candidate or from
unchanged dependencies. Existing shader, fiber and GPU-watchdog evidence stays
active if one of those stops occurs before the requested CPU failure.


### Joint journals survive native SIGSEGV and a distinct guest stack-protector stop

Latest owner captures October 23: the 10:48:33 UTC / 12:48:33 Madrid run
ends in CPU SIGSEGV after 118.584 s, exits 139/139, in allocator search traversal.
Current linked bytes confirm a read eight bytes before an invalid node. The
captured root is readable; the same invalid pointer is the first word of another
readable node. Search instructions support an invalid child-link hypothesis,
not a proven writer or atomic memory snapshot. No matching invalid-node/parent
observation survives in the current insertion/comparator/source selection; the
search boundary itself is not instrumented. Next: search root/left/right-load
provenance, initialization/reuse and synchronization, retaining joint coverage.

Both pre-failure journals work natively, with matching producer PID and prepared
binary UUIDs: 2304 CPU events (128/site) and 1024 shader events (512/type), no
busy drops/incomplete slots. CPU per-site spans vary from milliseconds to 18.733
seconds; the total span is not continuous coverage. Shader tail spans 34.120 ms;
all 512 retained lookups find metadata, not proof of valid ownership/code. The
text CPU dump is partial (552 events/3.980 ms, no footer); committed files survive.
26,428,220 CPU reservations and 7,064,195 shader reservations emphasize finite
retention. 1279 API presentations, 402 pipeline/271 SRT failures; 340 pipelines
adjacent to PerVertexKHR errors. Last/peak native samples 8.843/8.848 GiB, no
recorded OOM/large copies, memory drops/errors zero; no controlled saving claim.
Audio 46 windows/no queue-wait failures.

The immediately preceding 10:45:28 UTC capture stops after 126.140 s, exits 22/22,
on guest stack-protector failure. This is the stack-failure import handler, not
the fiber guard mismatch. Its direct _Exit bypasses rich caller/canary context,
but both journals survive (2304 CPU/1024 shader events, no drops/incomplete slots).
1325 presentations, 460 pipeline/805 SRT failures; last/peak native samples
10.603/10.623 GiB, no recorded OOM, audio 48 windows/no queue-wait failures.
Specific stack/caller/writer and shared cause remain unknown. Next also preserve
bounded caller evidence on this existing exit-22 path. Both diagnostic reports
match reconstruction. No runtime/config/dependency changes or game launch during
analysis. Read first handoff checkpoint.

The search-path register snapshot distinguishes a readable initial root, the
sentinel, a readable parent candidate and an invalid current node. Checked
instructions assign the parent when loading a left link; its first word matches
the invalid current node in bounded fault reads. This narrows the source
hypothesis to traversal links but does not identify a writer, object incarnation,
true memory type, CPU/GPU ordering or synchronization error. No matching pointer
in retained per-site history is not proof that earlier observations were valid.
The latest failure differs from the previously traced insertion traversal, so
the current selection must not be described as an exhaustive allocator trace.

The earlier exit-22 handler currently emits only the stack-protector STOP and
exits directly. No guest fault register snapshot, canary values, caller or fiber
identity is available there. Its CPU journal has 14.285 s of union span across
sites and shader journal 26.759 ms, with large prior-history wraps. Do not merge
it with the previous fiber guard STOP or infer actual stack exhaustion. Rich
failure context remains a follow-up; no code change was made in this analysis.
Native joint-file retention is now validated on both paths, but native overhead,
full lifecycle coverage and corruption correction remain unvalidated.


### Prepared joint capture; owner prioritizes reproducing SIGSEGV

Prepared combined pre-failure capture, prioritizing the owner-requested CPU
SIGSEGV investigation: 18 private observations at nine reviewed entries cover
allocator root/traversal links and earlier comparator/source-pointer boundaries;
13 bounded fatal fields retained. Exact current linked bytes checked offline.
Independent MAP_SHARED tails retain 128 observations per configured CPU site and
512 shader writes plus 512 changed lookups, without requiring a final dump.
A hot site/lookup cannot evict another site's/write channel. Interrupted/stale
slots and logged producer-PID mismatches are excluded; wrap, contention and
suppression are explicit. No signal handler queries the shader mutex/loader or
reads this file. Existing fiber guard context, metadata/decode snapshots and GPU
watchdog submission evidence remain enabled; none of those causes is proved
fixed. The owner launcher enables both journals and shader provenance and loads
the private combined selection as data, unless explicitly overridden. Current
journal files total 424 KiB; native tracing overhead is unmeasured. Abrupt signal,
shader/fiber/watchdog exit controls, actual JIT/interpreter CPU fault capture
with discarded stderr, disabled/refused output, per-channel isolation and
incomplete/concurrent publication controls pass. ARM64 renderer/all builder CPU
regressions, full JIT/interpreter contracts and 46/46 imports pass. Public heads
checked October 23 10:39:03 UTC unchanged; earlier PR/release review limits apply.
Pins/cache 28/9 and render settings unchanged; no game launched. Next owner
runs target SIGSEGV using the same combined command, with previous window closed.
Read first handoff checkpoint; this improves evidence, not native correctness.

The current command can be repeated manually until the CPU failure recurs;
other failures still retain their available evidence. Do not use the preceding
command's empty CPU selection. The owner launcher reads the private combined
selection when SOTC_CPU_FIELD_TRACE is unset and the private fatal fields when
SOTC_CPU_FAULT_FIELDS is unset. Explicit empty/zero overrides remain intentional
opt-outs. No automatic game retries or launches are added. Existing CPU and shader
stop reasons and exit codes remain unchanged; no invalid node or instruction is
skipped and the fiber stack is not enlarged.

SOTC_DIAGNOSTIC_JOURNAL=1 creates exclusive mode-0600 cpu-events.bin and
shader-events.bin in the fresh BB_GPU_USER_DIR. Initialization occurs in ordinary
startup/renderer work; files are never overwritten. Observations publish numeric
fields to fixed shared mappings with lock-free 64-bit commit markers. The hot
path performs no new guest reads, file writes, allocations or driver calls.
CPU sites have separate channels so unrelated hot paths cannot erase an entire
class. The original stderr ring remains a separate newest-first terminating
report; its incomplete footer does not erase committed file evidence.

The shader recorder keeps its existing per-descriptor mutex and eight-event
tail; the file is an independent pre-failure write/lookup tail, not a query from
signals or fibers. Shared monotonic timestamps and opaque thread identities are
CPU observations, not a total order of guest writes, allocation lifetime, shader
ownership or GPU execution. The current mapping budget is 424 KiB across both
files; each tail is finite and can omit initial state. MAP_SHARED supports offline
recovery after process termination without stdio/destructors/msync; power loss or
kernel-failure durability is not promised. Use a fresh state directory, as the
launcher does. A PID check excludes another logged child, not all PID-reuse cases.

The combined report adds pre_failure_journals, per-channel retained/wrapped/
incomplete/drop counts, numeric CPU fault-register matches and capture_readiness.
It never replaces missing stderr history with a claim of a complete lifecycle.
Absent requested journals and creation/reachability limitations remain explicit.
Existing metadata exact-scan and owned decoder snapshots remain separate from
the pre-failure journal; no extra copying of guest code into the journal occurs.

Original synthetic controls preserve an infrequent CPU site and a shader write
while hot channels wrap, reject interrupted/older-lap publications, preserve
existing files/symlink targets, and survive signal or immediate exit without final
hooks. The actual diagnostic captures both selected fields in JIT/interpreter
after a real synthetic guest SIGSEGV, even with stderr discarded. The actual
metadata wrapper retains its captured address on required failure. No native
writer, common cause, memory saving or game stability improvement is established.


### Owner capture: allocator tree invalid pointer; new shader evidence not emitted

Latest owner capture (October 23 10:24:27 UTC / 12:24:27 Madrid): CPU SIGSEGV
in allocator tree traversal after 65.553 s, exits 139/139. Current linked bytes
confirm a read at invalid node minus eight; root versus traversed child source,
writer, lifetime and synchronization remain unknown. Full guest/host fault
context and one minimal reporter survive. CPU field selection is explicitly
empty: no history is expected (zero configured sites), not evidence of a lost
dump. Shader provenance is enabled but not emitted from CPU signals; no shader
metadata/decode, register-range, fiber or GPU-watchdog stop is recorded. Matching
prepared renderer/probe UUIDs confirm new build loaded, not correction of native
corruption. 911 API presentations, 344 pipeline/271 SRT errors; 292 pipeline
failures adjacent to PerVertexKHR conversion errors, 52 unattributed. Last/peak
native samples 8.838/8.848 GiB, no recorded OOM/large copies, memory drops/errors
zero; shorter run prevents improvement claims. Audio 24 windows/no queue-wait
failures. Diagnostic matches reconstruction. Next: focused allocator tree
root/child initialization, reuse and synchronization evidence. No runtime/config/
dependency changes or game launch during analysis. Read first handoff checkpoint.

The new code-span/address coherence and SH destination guards are loaded but no
register-range rejection or required metadata/decode failure precedes this CPU
stop. Their synthetic validation remains separate from this native outcome. The
CPU history dumper returns immediately with zero configured sites; the combined
report labels history absent, but the launcher selection and early minimum
explain why. Do not re-enable unrelated tree or shader probes unchanged. Focus
the next diagnostic preparation on the allocator root and selected child reads.
The bounded mutex-based shader journal cannot safely be queried by this signal
handler; its absence says nothing about prior descriptor events. No new shader
scan snapshot/provenance sidecar is available from this run. Prior shader, fiber
and watchdog failures remain unresolved. Private offline review retains the
exact linked-byte comparison and pointer arithmetic; no game addresses or bytes
are copied into public documentation.

### Prepared checked SH copies, bounded program provenance and exact scan evidence

Three legacy SetShReg paths now validate the full destination before memcpy:
shared graphics replay (also used by preparation workers), GPU-owner compute
state and asynchronous compute/graphics writes. The old compute checks only
bounded byte count, accepting a nonempty write at the one-past endpoint or a
partial overrun. Actual preceding source cases overwrite adjacent synthetic owner
canaries for those cases and an end-of-register-bank graphics write. The adapted
cases stop explicitly with exit 23 before copy. Valid bytes/canaries remain intact;
empty writes avoid forming destination references. These are concrete host-write
corrections, not proof that an owner capture triggered them. Invalid SH ranges
now fail fast, including shared replay, rather than silently altering adjacent
state. Other PM4 packet types are outside this range correction.

Opt-in SOTC_SHADER_PROVENANCE=1 adds bounded CPU observations of address-register
writes and metadata lookups. It uses 64 fixed descriptor slots and eight recent
events per slot, with total/omitted/suppressed/slot-eviction counts. Identical
adjacent lookups are suppressed. Observations include numeric descriptor/bank/
packet addresses, SH span, applied program address, actor category and lookup
head/hash/length/result. Shared replay, asynchronous writes and the alternative
CP decoder's observer are instrumented; actor categories do not identify a host
thread. The packet pointer can refer to an owned CPU copy, not the original guest
buffer. Slots and pointer values can be reused; no allocation identity, exhaustive
writer coverage, visibility guarantee or GPU execution is inferred. Other state
initialization/delta copies may have no matching address-write event.

Required metadata failure emits the matching descriptor history and saves it as
private shader-metadata-failure/provenance.txt. No history from a different
numeric descriptor is borrowed. The report retains events/omissions and flags
missing address-write coverage or interleaved program records. Other failure
paths, especially signals, do not query this mutex-based recorder. The next
private command clears SOTC_CPU_FIELD_TRACE and enables shader provenance,
retaining bounded fatal-field context. It no longer repeats the unrelated tree
selection. Relative runtime overhead and native coverage are unmeasured.

A further checked correction binds the returned code span to the same captured
address used for metadata lookup and mapping validation. Previously the wrapper
reread the descriptor when returning, so a descriptor change during a backing
read could combine old metadata with a new code address. An actual-wrapper control
restores that preceding return expression and reproduces the mismatch; the adapted
wrapper keeps the validated address. Lookup observations likewise use the captured
address, not a later descriptor read. This is not an atomic descriptor/user-data/
code snapshot or a lifetime guarantee, and the native owner's cause is unproved.

The metadata finder now retains the bounded bytes it actually scanned. Required
failure writes those bytes instead of rereading backing after CPU history;
context marks snapshot_from_scan and whether initial head matches the scan.
A synthetic history hook mutates backing after the failed scan: saved evidence
still contains exactly the examined bytes. Initial head/fast-hint reads and the
backing copy are not atomic guest observations; this repairs evidence fidelity
and removes the second failure-path backing read, not source lifetime semantics.

Actual production SH cases pass original canary controls, valid writes, no-op
endpoints, explicit adapted rejections, disabled tracing, duplicate suppression
and fixed-ring/slot eviction. Two metadata and fourteen combined-report tests
pass, along with 26 builder tests including exact provenance_previous migration.
The ARM64 renderer and all builder CPU regressions pass; full JIT/interpreter
contracts and 46/46 host-symbol coverage pass. Exact generated source digests are
reviewed, base pins/cache 28/9 and rendering settings remain fixed. No game is
launched. Prior shader/node/fiber/GPU causes remain unresolved.

Public Git HTTPS default heads checked October 23 10:09:50 UTC retain reviewed
SotC/runtime/CPU/MoltenVK/SPIRV-Cross SHAs. This retains earlier PR/release review
limits. The next owner comparison is a targeted diagnostic, not an asserted fix
for the last missing vertex metadata or a controlled memory benchmark.


### Required vertex metadata fails before the guarded decoder

The October 23 09:56:27 UTC owner capture (11:56:27 Madrid) stops after 82.589 s
on required graphics:vs metadata lookup from the GPU command owner thread,
exits 23/23. The new probe and bounds-corrected renderer UUIDs match prepared
binaries. The failure path goes through graphics stage/key refresh, pipeline
selection and an indirect draw. It does not reach opcode format lookup; this
boundary cannot validate or refute the format-bounds correction. It differs
from the preceding fragment first-word rejection and earlier fiber/CPU/watchdog
stops. Their lack of recurrence is not a fix or shared-cause proof.

The program address has 10,159,872 mapped backing bytes; a bounded 65,564-byte
scan reports found zero, hint zero, length zero and metadata offset zero. The
saved 65,564-byte backing slice has matching logged first words and checked hash,
with no OrbShdr signature. Unlike the earlier owned decoder snapshot, this is a
separate backing read after history capture, not an atomic scanner snapshot.
Mapping does not validate program type/content. Bad/stale program register or
descriptor, lifetime/reuse, CPU/GPU write visibility and lookup bounds remain
open. No fabricated length/hash, retry-as-success or skipped work is justified.
Exact address, words, stack symbols and snapshot remain private.

CPU history is complete within its frozen newest-first ring: 2,048 valid events,
zero skips/drops/unavailable samples, but just 131.696 ms across 878 observed
objects and two threads. All four tree-entry sites are exercised. There are
24,546,076 reservations and 9,421,950 suppressed identical consumer samples;
24,544,028 reservations are older than the ring. No exact shader-origin match
appears, and tree probes do not identify shader register/descriptor provenance.
Next capture program register/address changes and PM4 submission source/lifetime
through the first required bind. Do not repeat the same unrelated tracing or
increase scan limits blindly.

Automatic diagnostic.json exactly matches reconstruction. There are 893 API
presentations, last t=81.6594 s, and 256 pipeline failures across 239 pairs
(228 adjacent PerVertexKHR, 28 unattributed), plus 271 SRT errors. Last/peak
retained native presentation samples are 8.827/8.859 GiB, with last sample age
502.999 ms at presentation, not failure-time. Earlier stop/workload prevents
controlled pressure reduction or lower error-count claims. No OOM/large host
copies are recorded; last periodic memory drops/observation-error counters are
zero. Audio has 30 windows, zero queue-wait failures, maximum submit gap 36.485 ms,
without proving continuous playback. Tolerant field CLI succeeds, with zero text
replacement characters. No code/configuration/dependency changes or game launch
follow this analysis; private review remains under work/analysis.


### Prepared instruction-format bounds correction and tolerant field CLI

Inspection of the saved first-word failure maps its encoding-specific opcode to
an in-range undefined format slot in the current table. It is not an outside-array
index, and no operand types or successful instruction semantics are invented for
that slot. Incorrect program origin/content versus unsupported format remains
open. Exact words/opcodes and private review stay under work/analysis.

A separate concrete decoder defect is corrected: all seventeen production
instruction-format lookups previously indexed their arrays without bounds checks.
The adapted lookups return the existing undefined-format value for an outside
index, letting the real decoder produce its scoped error/exit 23 before an
out-of-bounds table read. Checked libc++ array hardening traps on the original
first-outside-index control. All 2,002 existing entries, including undefined holes,
retain all six metadata fields; 34 first-outside/maximum-index controls reject.
Actual installed decoder tests also reject original synthetic outside and in-table
hole instructions explicitly. No GPU or game-derived instruction fixture is used.
This corrects unsafe indexing; the owner's captured in-range rejection still
needs source/format review and is not claimed fixed.

A new digest-checked local overlay and exact format_previous cohort migrate the
preceding source without reapplying its other overlays. ARM64 renderer build and
all builder CPU regressions pass, including the new format control; host import
coverage remains 46/46. Valid shader metadata is unchanged, so existing cache
28/9 is retained. No base pin, dependency, game data or renderer setting changes.
Full CPU contracts pass in JIT/interpreter. The same private seven-field tree selection
and fatal fields are retained for the next owner capture, with prior fiber/fault
reporting intact. No game is launched here.

The standalone field CLI now decodes logs tolerantly and records replacement
character count. An original invalid-UTF8 text control preserves its valid field
record, and reprocessing the latest owner log returns all 2,048 events instead
of failing UnicodeDecodeError. Six field-analysis tests pass; byte replacements
do not identify guest corruption. Original log/diagnostic remain unchanged.

Public Git HTTPS default heads checked October 23 09:52:37 UTC retain reviewed
SotC/runtime/CPU/MoltenVK/SPIRV-Cross SHAs; earlier PR/release review limits remain.
Prior invalid pointers, fiber marker damage, pipeline/SRT errors and GPU stalls
are unresolved. Native benefit and the next failure boundary remain unvalidated.


### Fragment first-word decode rejection; owned snapshot and full bounded history

The October 23 09:43:06 UTC owner capture (11:43:06 Madrid) stops after 115.810 s
on main-program unknown_instruction_format, exits 23/23. Checked enum value zero
is HwStage::Fragment. The first-prologue warning precedes a rejection at word
zero in an owned 282-word supplied program. The decoder finds undefined operand
format metadata. Unsupported decode-table/instruction versus invalid program
origin/content/type/lifetime remain open; a mapped and owned source is not a
validated shader. No relation to earlier node/pointer/fiber corruption is proved.
Exact shader identity, origin, opcode, raw words and evidence stay private.

The saved owned shader snapshot now exists: all 282 words / 1,128 bytes, with
saved-start zero and complete range metadata. Its SHA-256 and first word match
the combined report and rejection. This preserves decoder input without rereading
the guest origin; it cannot validate the initial source or changes during capture.
Next review the saved encoding/opcode against the actual format table and trace
program descriptor/address/length and write/lifetime provenance. Do not synthesize
a successful opcode or skip rejected instructions. No GPU command execution is
established by this CPU-side compilation failure.

The frozen newest-first CPU dump is complete within its ring budget: 2,048 valid
records, zero skips/busy drops/unavailable samples, 4.171667 s across 658 observed
objects and five threads. All four selected tree entries are exercised. There
are 35,675,310 reservations and 12,355,930 suppressed identical consumer samples;
35,673,262 records are older than the ring. Complete output is not complete object
history or a controlled tracing-overhead measurement. The tree selection does not
target shader provenance; no exact shader-origin address match appears. No fatal
register snapshot exists, as this is not a signal stop. Fiber-specific reporting
is unexercised and its underlying failure remains unresolved.

Automatic diagnostic.json exactly matches reconstruction, including the saved
shader context. There are 1,296 API presentations, last t=114.5629 s, and 413
pipeline failures across 378 pairs (368 adjacent PerVertexKHR, 45 unattributed),
plus 805 SRT errors. Last/peak retained native presentation samples are
10.469/10.480 GiB, with last sample age 895.318 ms at presentation, not fault-time.
No OOM or large copies are recorded; last periodic drops/observation-error counts
are zero. Audio has 44 windows, no queue-wait failures and maximum submit gap
54.853 ms, without proving continuous playback. No GPU watchdog/CPU SIGSEGV/fiber
stop appears; lack of recurrence does not establish a fix.

The raw log contains non-UTF8 bytes (three replacement characters on tolerant
read). Combined reporting handles them, while the standalone field CLI's strict
read fails. Direct analysis with replacement decoding succeeds; invalid text bytes
alone do not prove guest corruption. Retain this small CLI issue for follow-up.
No runtime/configuration/dependency changes or game launch follow this analysis.


### Prepared fiber marker evidence and history before terminating

A stack-marker mismatch in save_running now captures bounded current fiber/
stack metadata, stack size, numeric end/range validity, current guest RSP,
expected/observed marker, state/flags, entry/current CPU RIP, encoded bounded
name, and the sixteen architectural registers. The same real check still exits
23: no marker removal, fiber skip, fake success or stack enlargement. A marker
mismatch while RSP is within numeric bounds is possible and must not be called
proof of stack exhaustion. Metadata itself and host-call RIP are observations,
not validated allocation identity or exact guest instruction attribution.

The diagnostic entry's exported strong callback freezes new observations, attempts
small context writes first, then existing newest-first CPU history. Standalone
contract binaries have a weak metadata-only fallback rather than a new mandatory
unresolved import. Each write is at most PIPE_BUF, pipe/socket output becomes
nonblocking, descriptor-scoped Darwin SIGPIPE suppression is applied, and failed/
short writes are not retried. Unsupported output/full/closed pipes can still lose
context or history; frozen history does not suspend in-flight guest threads.

The report recognizes all STOP categories, including the prior unrecognized
fiber stop, and retains numeric fiber/stack evidence plus encoded identity.
Reprocessing the last owner capture recovers its reason while explicitly retaining
its missing stack/RSP/marker evidence; the original diagnostic is preserved.
Numeric register/history matches remain bounded and do not infer causality.

Seven native history tests pass, including actual shared context/history output,
frozen newest-first records, inside/outside RSP, invalid-bound arithmetic, and
full/closed output with real exit 23. A deliberate one-bit guard overwrite in the
actual original synthetic guest fiber fixture exits 23 in JIT/interpreter, with
RSP still inside bounds, matching encoded identity and parseable metadata. Existing
normal switching/migration tests remain required. Twelve report/five field-analysis
tests pass; full contracts pass in JIT/interpreter. The rebuilt probe exports both hooks
and passes 46/46 renderer host-symbol coverage. GPU code is unchanged.

The next owner command retains the seven tree-origin fields and matching fatal
windows, now with fiber failure reporting active. Actual native corruption/fiber
cause remains unresolved; these changes repair evidence loss, not the source of
the overwrite. No game is launched. Pins/cache/settings are unchanged. Public
Git HTTPS default heads checked October 23 09:38:41 UTC retain reviewed SotC/
runtime/CPU/MoltenVK/SPIRV-Cross SHAs; earlier PR/release limits remain.


### Fiber stack-marker rejection; direct exit bypasses source capture

The October 23 09:30:39 UTC owner capture (11:30:39 Madrid) stops after 109.422 s
with `STOP: fiber guest stack overflow`, child/supervisor exits 23/23. Checked
local `save_running` code in bootstrap_fibers.c emits this when the current fiber
has a stack whose first word differs from its marker, then directly exits. The
check does not test guest RSP against bounds. Thus marker mismatch is established,
while actual stack exhaustion, unrelated overwrite, reused backing, bad metadata
or lifetime remain unresolved. Fiber name, stack bounds, guest RSP and observed
marker value are not logged. This is a distinct boundary from previous translated
CPU faults, shader rejection and GPU watchdog; no common cause is demonstrated.

The seven tree-origin probes are configured and prepared probe/renderer UUIDs
match. No history, fatal minimum or guest register context is emitted: this direct
fiber exit bypasses the signal/assertion history hooks. Actual probe activity and
tree-root provenance remain unvalidated; absent output is not an empty ring.
Automatic diagnostic.json exactly matches reconstruction but its stops array is
empty because the matcher omits this fiber STOP category. The raw log supplies
the real reason; absence from the structured report must not imply normal exit.
No parser/runtime correction is applied during this analysis.

There are 1,181 API presentations, last t=108.3639 s, and 416 pipeline failures
across 379 pairs (364 adjacent PerVertexKHR, 52 unattributed), plus 805 SRT errors.
Last/peak retained native presentation allocation is 10.190 GiB, with last sample
359.054 ms old at presentation, not failure-time. No OOM or large host copies are
recorded; last periodic drops/observation-error counters are zero. Audio has 42
windows, no queue-wait failures, maximum submit gap 51.102 ms, without establishing
continuous playback. No GPU watchdog or shader rejection occurs; earlier failures
are not thereby fixed. Counts/memory at different stop boundaries are not a
controlled improvement comparison.

Next preserve bounded current-fiber identity/stack bounds/current guest RSP and
expected/observed guard evidence, then freeze/dump CPU history before the real
exit 23. Include this STOP category in the combined report. Synthetic guard
corruption versus out-of-bounds RSP controls and valid switching should precede
another owner capture. Do not remove the guard, skip a fiber or enlarge all stacks
without evidence. No code/configuration/dependency change or game launch follows
this analysis. Private capture review remains under work/analysis.


### Prepared tree-origin selection and failed-read register correlations

The next ignored private command replaces allocator-source probes with seven
field observations at four reviewed function/call-return boundaries. It records
the container root on entry and after the helper, boundary links after the helper,
both current-node child fields on return from the comparator (before the branch),
and a node-source candidate at comparator entry. Entry/call-return probes avoid
relying on arbitrary interior fallthrough sites that the pinned JIT might omit.
Actual linked instruction bytes and executable segment bounds match reviewed
code. The actual prepared binary accepts trace/fatal-field configuration before
a deliberately missing-input exit, without loading a game or GPU. Fatal fields
are retargeted to container/root/node candidates rather than allocator headers.
Exact sites, register IDs, field offsets and metadata remain private under work/.

Root observations are unconditional, preserving repeated helper/lifecycle visits
within the finite ring. Repeated identical consumer samples may be suppressed;
callbacks and protected reads still execute, and source changes between probes,
address reuse and ownership remain unproved. No game memory is changed, pointers
are not followed recursively, and no invalid node is skipped. Native capture and
traffic/retention behavior of this selection remain unvalidated.

Offline CPU analysis now preserves individual protected-read failures and rejected
address-arithmetic samples, which previously survived only as counts. Combined
reports expose a bounded recent unavailable tail and numeric matches between the
single guest register snapshot and retained source values/object/field addresses.
Unavailable values never become fabricated zero-valued reads. Multiple snapshots
remain ambiguous rather than being combined. Per-register matches are limited to
eight latest records with explicit total/omitted counts. These are numerical
associations, not validated objects, actual writes, lifetime or causal ordering.
Timestamp span now includes timed failed reads as well as readable observations.

Eleven combined-report, five field-analysis and six native probe tests pass,
including unreadable/overflow samples, bounded matches, ambiguous snapshots and
JIT/interpreter protected-read/backpressure controls. Reprocessing the last owner
capture preserves its original stop and partial history; no connection between
that bad node and the old allocator selection appears. The original diagnostic
is preserved. No runtime rebuild or renderer/settings change is needed for these
offline improvements and private selection. No game is launched here.

Public Git HTTPS default-head refs checked October 23 09:28:46 UTC retain reviewed
SotC/runtime/CPU/MoltenVK/SPIRV-Cross SHAs. Earlier PR/release review limits remain;
pins/cache are unchanged. The current node corruption and prior GPU/shader/pointer
failures remain unresolved; this preparation improves localization, not gameplay.


### Earlier CPU node comparator failure recurs; source selection misses it

The October 23 09:22:03 UTC owner capture (11:22:03 Madrid) stops after 67.702 s
on translated CPU SIGSEGV, exits 139/139. This repeats the exact comparator and
caller boundary of the October 23 07:57 capture, rather than the later NaN-pattern
allocator pointer read or the preceding GPU watchdog. Current linked instruction
bytes match the reviewed static disassembly: the caller forms a key address from
node plus 32, then the comparator reads it. Captured node/key arithmetic agrees;
the node pointer is already invalid. Root versus left/right link, writer, reuse,
object type and CPU/GPU/translator cause remain unproved. Private offsets, raw
addresses and verified bytes stay under work/.

The early fatal minimum, native registers, exact guest instruction and bounded
guest memory/frame context are present, with one reporter and no nested notice.
The frozen newest-first dump retains 532 valid events across 3.030205 s, 33 objects
and six observed threads, starting with the minimum's latest sequence. It has no
footer and is partial; the requested 2,048-record budget does not mean all were
written. Nonblocking best-effort output can terminate early; its precise failure
reason is absent. The seven active probes cover the prior allocator-source path,
not this tree root/link traversal, and overlap no selected fault fields. Retained
values do not show the prior repeated NaN pointer; that does not establish a fix.
Retarget root/link initialization and reuse before another owner comparison;
do not repeat this source selection unchanged or skip invalid nodes.

Automatic diagnostic.json exists and exactly matches reconstruction, correctly
marking partial CPU history. No GPU watchdog, shader metadata/decode rejection or
OOM is recorded, so those paths and the new GPU report section are unexercised.
There are 908 API presentations, last t=66.7991 s, and 326 pipeline failures across
294 pairs (277 adjacent PerVertexKHR, 49 unattributed), plus 271 SRT errors.
The earlier stop/workload prevents interpreting lower counts as improvement.
Last/peak retained native presentation samples are 9.050/9.065 GiB, not fault-time;
last native sample age at presentation is 367.827 ms. Three 187.012 MiB host copies
complete, reusing the same 192 MiB staging buffer; no multi-GiB staging copy is
observed. Last periodic counters have zero dropped records/observation errors.
Audio has 24 windows, zero queue-wait failures, maximum submit gap 32.973 ms,
without proving continuous playback. No runtime/settings/dependency changes or
game launch follow this analysis.


### Prepared GPU-stall report with explicit evidence omissions

The combined private report now keeps each submission report separate, with
LAST_COMPLETED versus AWAITED batches, recorded wait/signal dependencies, buffer
recording generations, native chunks, commit/scheduling/completion callbacks and
fresh native status/memory observations. Callback status zero before completion
is not interpreted as native command status zero. Unavailable memory probes are
unknown rather than zero allocation. CPU note counts and retained/omitted guest
candidate counts remain separate from GPU progress; no exact shader or deadlock
is inferred. Missing native callbacks/buffers and candidate omissions are gaps.
The explicit STOP_GPU_STALL marker is now retained, including a gap if its
submission trace is absent. Multiple report contexts never borrow prior chunks.

A complete CPU output footer now still raises an evidence gap when its output
budget omits other ring records. Reprocessing the latest capture preserves its
three scheduled/uncompleted chunks and reports the 1,920 omitted CPU records plus
one omitted guest work candidate. The owner's original diagnostic is preserved;
the reviewed reconstruction remains private under the capture state directory.

Nine combined-report and five CPU field-analysis tests pass, with additional
native producer-format coverage in the submission suite. Existing production
CPU controls also pass ordered task-frame retirement, staging held/pending/tick/
type reuse and native admission callback lifetime/cancellation. These validate
those contracts, not all barriers, resource lifetimes, game pixels or M1 GPU
behavior. No synchronization change is justified by these controls alone.

The next owner command retains the same source selection and fatal windows to
capture a potential recurrence without adding GPU tracing work or changing
runtime/settings. The updated offline report will run automatically afterward;
no renderer rebuild is required for this report-only change. Never run a second
instance while the previous window remains open. The original CPU pointer and
GPU-stall causes remain unresolved; no game is launched here.

Public Git HTTPS default branches rechecked October 23 09:18:09 UTC retain the
reviewed SotC/runtime/CPU/MoltenVK/SPIRV-Cross heads. This is a ref comparison,
not a new PR-state/release audit; earlier review limits remain. Pins/cache stay
unchanged. Exact capture evidence and identifiers remain private.


### Source probes exercised; GPU watchdog with three scheduled buffers

The October 23 09:11:07 UTC owner capture (11:11:07 Madrid) stops after
132.309 s on the renderer's ten-second GPU watchdog, not the preceding CPU
pointer fault or shader rejection. Awaited batch 101847 follows completed batch
101846; recording has reached 101853. Three awaited native buffers have returned
from commit, received scheduling callbacks and report native status 3/error 0,
but no completion callback. The previous batch completed. This brackets the
unfinished submission without identifying an executing draw/dispatch, a driver
bug or a CPU/GPU dependency cycle.

The awaited journal contains 710 CPU notes, including 19 guest draw and 110 guest
dispatch candidates. The separate candidate tail retains 128 of 129, alongside
the original 96-note tail and first/last descriptions. These are CPU recording
candidates, not per-command GPU progress; neither first nor last identifies the
culprit. Fresh native probes report 11.067 GiB allocated during the stall, against
11.841 GiB recommended. No OOM is recorded and memory remains high; this is not
evidence of a pressure reduction or an allocation-caused hang.

The new seven-observation source selection is active. Its frozen newest-first
cancellation dump contains 128 valid records, complete footer and no in-window
skips, spanning 1.613296 s across four observed objects/four threads. It attempts
260,084 records; 258,036 are older than the ring and another 1,920 still-in-ring
records are omitted by the cancellation output budget. Complete budgeted output
is not complete object history. The preceding repeated NaN pointer does not occur
in retained values; neither that absence nor the lack of CPU SIGSEGV proves the
source corruption fixed. Writer, type, lifetime and CPU/GPU relation remain open.

Cooperative cancellation is observed 2.384 ms after the request, child exit 21 /
supervisor 31. Automatic diagnostic.json matches reconstruction. There are 1,214
API presentations, last t=121.5886 s, 474 pipeline failures across 432 pairs
(417 adjacent PerVertexKHR, 57 unattributed) and 805 SRT errors. Last/peak native
presentation sample is 11.069 GiB; the fresh stall sample above is separate.
Last periodic memory summary has zero dropped records, observation errors and
large host copies. Audio has 50 windows, no queue-wait failure and maximum submit
gap 35.633 ms, without proving continuous playback. No shader rejection occurs.

Next prioritize dependency/barrier/resource-retirement review of the unfinished
submission and synthetic reproductions, while preserving source observations for
a future CPU failure. Do not skip work, fake completion or extend the watchdog as
a remedy. This analysis changes no runtime, settings, dependencies or private
probe configuration and starts no game.



### Prepared source-boundary capture, newest-first output and Darwin SIGPIPE repair

CPU ring output now emits newest reservations first and declares `newest_first`.
If nonblocking output fills, its surviving prefix retains recent observations
instead of the oldest part of the selected window. Offline analysis still sorts
reservation IDs for comparisons; this is not a guest write-order guarantee. Tests
exercise a changing 300-value source, require the latest 128 values/sequences first,
and verify correctly ordered transitions from a partial reverse-order dump.

Closed-output synthetic stress exposed a separate diagnostic bug: 6/100 cooperative
stops died by SIGPIPE despite worker-local masking. Darwin F_SETNOSIGPIPE is now
set on diagnostic stderr before cancellation startup/ready output, and on early
fatal output. Descriptor-scoped suppression covers other writers without changing
the process-wide signal disposition; worker masking remains. The same 100-run
control passes 100/100 after the fix. Native tests assert the descriptor setting,
full/closed output behavior and real exits. This repairs diagnostic termination,
not the game's SIGSEGV or its NaN-pattern pointer source.

The next ignored private command replaces tree probes with seven unconditional
source/header observations at three reviewed entries: caller before flag update,
helper entry and the captured block before loading the failing pointer. It reads
only configured source fields, not the values as pointers, and can distinguish
observed bad-on-entry from a later observed state. It does not establish object
identity, ownership, an exhaustive writer trace or intended memory type. Exact
entry bytes/executable bounds match the actual private linked input. Fatal windows
now cover the relevant source/owner candidates instead of the old tree selection.
Actual rebuilt parser acceptance and ignore checks pass; native exercise is pending.

Six CPU tests, five field-analysis tests, full JIT/interpreter contracts and 46/46
host imports pass. Previous 22 renderer CPU checks remain applicable with GPU
code unchanged. Existing early minima, rich failure handling and real exits remain.
Public HTTPS refs checked October 23 09:06:26 UTC retain reviewed SotC/runtime/CPU/
MoltenVK/SPIRV-Cross heads; earlier PR-state limits, pins and cache 28/9 stay fixed.
No game is launched and the original invalid pointer's writer/cause remains open.


### Early fatal minimum succeeds; source pointer contains repeated NaN bits

The October 23 08:59:55 UTC owner capture (10:59:55 Madrid) exercises the focused
five-probe selection and matching prepared binaries. It stops after 91.851 s on
translated CPU SIGSEGV, exits 139/139. The early minimum is present with a recent
atomic record and one reporter identity. No concurrent/nested notice occurs;
this does not prove the prior multi-reporter cause fixed. Full native registers,
exact guest instruction and bounded guest memory/frame context are now available.

Private static inspection matches the actual linked input: a preceding load
reads a pointer from a source object, then the faulting instruction reads pointer
plus eight. The source word and loaded register agree on two repeated binary32
quiet-NaN patterns. This is evidence that code uses that value as a pointer, not
that a particular component wrote NaNs, that an expected allocator/object type
is validated, or that GPU/CPU translation caused corruption. Wrong object/type,
stale/reused storage or a writer remain open. Exact offsets/addresses/raw bytes
and checked disassembly stay private.

The full CPU ring begins frozen but emits only 596 events, no footer, hence
partial rather than complete/empty. It spans 26.904 ms, ending 1.089446 s before
the minimum's most recent observation. The retained first part of a ring dump
must not be described as its newest tail. Nonblocking reporting stops at its
first failed/short write; the exact failure reason is not logged. The minimum
still preserves the latest sampled record, but it belongs to the old tree
selection and is not this source field. Narrower tracing attempts 10,642,207
records; different timing/boundaries prevent an overhead benchmark. New source
initialization/reuse probes are needed before repeating that selection unchanged.

Automatic diagnostic.json exists and matches reconstruction, correctly flagging
partial history. There are 1,109 API presentations, last t=90.9091 s, 418 pipeline
failures across 381 pairs (369 adjacent PerVertex, 49 unattributed), and 805 SRT
errors. Last/peak native presentation sample is 10.370 GiB, 535.054 ms old at
presentation, not fault-time. No OOM is recorded; last periodic memory counters
have zero large copies, drops or observation errors. Audio has 34 windows, no
queue-wait failures and maximum submit gap 32.609 ms; continuous playback remains
unverified. No shader rejection occurs; owned shader evidence is unexercised in
this run. No executable/config/dependency change or game launch follows analysis.


### Prepared early fatal minimum and explicit report truncation

Every unrecovered fatal entry now freezes new observations and attempts one
compact minimum before claiming the detailed reporter or querying provider/CPU
mapping metadata. This includes a second/nested entry before its real immediate
exit. The minimum contains signal/address, native PC/LR, a POSIX-thread identity,
ring end and at most one committed recent observation from an eight-slot search.
That observation may belong to another thread; neither it nor PC/LR identifies
a writer or a validated guest fault instruction. Distinct reporter identities
can distinguish participating threads; an absent minimum cannot supply identity.

The fixed buffer fits PIPE_BUF, is emitted with one best-effort nonblocking pipe
write, and never allocates, locks, reads guest bytes, waits for a damaged reporter
or retries output. Closed-reader SIGPIPE is blocked in the terminating thread;
full/closed/unsupported outputs can omit evidence. The minimum supports pipes
and sockets; regular files/terminals skip it rather than promise bounded I/O.
Existing rich context/full history follows for the first reporter only; real
exits 128+signal and recovered GPU/speculative/alignment faults are preserved.
Stderr becomes nonblocking only during fatal termination on supported outputs,
so later rich output may also be incomplete. This is evidence retention, not a
repair of the original invalid pointer or a guaranteed full crash report.

Combined diagnostic.json now retains minimal snapshots and reporter identities,
counts concurrent/nested truncation notices and independently states whether
guest context finished. A complete native register block cannot conceal a
truncated guest report. Reanalysis of the prior owner log exposes its two notices
but cannot reconstruct missing minima/history. Existing decode-specific and
post-run progress evidence remains separate.

Six native CPU suites include deterministic nested host-signal controls with
available/full/closed output, matching reentered-thread identities, unchanged
protected-reader counts and preserved exit 139, plus actual JIT/interpreter fatal
entry ordering and observed synthetic field values. Six combined-report tests,
full JIT/interpreter contracts and 46/46 host imports pass. The prior 22 renderer
CPU checks remain applicable with GPU code unchanged. Native minimum capture
and the actual SIGSEGV cause remain pending. The same focused five-observation
private owner command is next; no game is launched. Public HTTPS refs checked
October 23 08:57:14 UTC retain the reviewed SotC/runtime/CPU/MoltenVK/SPIRV-Cross
heads; PR-state limits, dependency pins and cache 28/9 remain unchanged.


### Focused capture returns to CPU SIGSEGV with truncated fatal reporting

The October 23 08:49:05 UTC owner capture (10:49:05 Madrid) enables the exact
five-observation focused tree selection and matching prepared binaries. It stops
after 95.328 s with confirmed exits 139/139 on CPU SIGSEGV through an invalid
address. Two concurrent-or-nested fatal notices occur before the first report
finishes. The native ARM64 register block is complete, but exact translated guest
instruction, guest register/memory context and CPU ring dump never appear. The
fail-fast second-reporter path can terminate before the main reporter reaches
them. Multiple failing threads versus recursion inside reporting remains unknown;
no precise guest instruction, pointer origin, writer or CPU/GPU cause is proved.
Do not use an unvalidated saved register/LR as that missing instruction/call stack.

Automatic diagnostic.json is present and matches reconstruction. Its complete
host-register block does not imply a complete guest fatal report; history is
explicitly absent. The narrower selection's native retention/overhead cannot be
measured from this missing dump. No shader decode/metadata rejection occurs, so
missing shader bytes do not show a snapshot regression. Neither prior shader
failures nor the silent freeze is proven fixed.

There are 1,149 API presentations, last at journal t=94.3423 s, 451 pipeline
failures across 409 pairs (394 adjacent PerVertex, 57 unattributed), and 805 SRT
failures. Last/peak presentation-sampled native allocation is 9.580/9.602 GiB,
not fault-time memory. No OOM is recorded. The last periodic summary counts zero
large copies and observer errors, but drops 4,908 buffer-journal records (zero
image/summary drops): complete buffer-lifetime absence claims are unavailable.
Audio has 36 windows, zero queue-wait failures and maximum submit gap 56.715 ms;
continuous playback remains unvalidated.

Next prioritize minimal bounded history/register evidence before richer fatal
reporting and expose concurrent/nested truncation in the combined report. Preserve
real exit behavior and never wait for a damaged reporter. Distinguish recursion
from another failing thread using original synthetic controls before attributing
a cause. Raw registers/addresses and reviews remain private. No executable,
configuration or dependency changes or automatic game launch follow this analysis.


### Prepared owned decode evidence, explicit stop reasons and narrower CPU selection

Shader-decode failure now preserves the active owned main-program bytes before
its existing assertion/exit 23. The host history hook runs first. The capture
uses only a scope-owned immutable vector, never the original guest pointer or
adjacent storage, and is bounded to 16,384 words around the failed offset/end.
Context identifies total/supplied/saved words and the saved start. Borrowed
fetch/copy paths have no invented snapshot. Empty input is explicit; first
failure evidence is preserved. I/O exceptions report unavailable evidence and
leave the original rejection intact. This is not an opcode/metadata repair or
proof that the initial capture was atomic or valid.

The combined report now retains decode-specific reasons/provenance even when
the terminal marker is a generic renderer assertion. Saved owned bytes have a
fingerprint and range context; older missing captures remain explicit gaps.
Post-run progress review flags long no-presentation intervals before exit and
owner-requested window closure, even with exit zero. It is not a live watchdog,
a driver-hang diagnosis or proof against legitimate pauses/minimization; clock
origins may differ. Offline checks recover the earlier apparent-freeze clue
and the latest compute-decoder reason without new game execution.

A separate ignored focused tree selection retains five reviewed observations
at three existing entries: container root/header links and the two source child
links. It removes redundant entry/key/per-node observations from the 21-field
selection. Actual parser acceptance and the previously verified executable-site
subset are checked. It retains less detail and cannot prove writer/lifetime;
actual traffic/retention improvements require the owner's comparison. Fatal
container/node windows stay selected. Previous config files remain available.

ARM64/all 22 renderer CPU checks and 46/46 host imports pass. Installed-decoder
controls additionally verify exact owned bytes, a bounded offset window after
source mutation, no borrowed/oversize capture, preserved first evidence and real
exit 23. Five combined-report tests and full JIT/interpreter contracts pass.
The original unknown-format cause and prior silent freeze remain unresolved.
No dependency/cache changes or game launch. Public HTTPS refs at October 23
08:47:22 UTC retain the reviewed SotC/runtime/CPU/MoltenVK/SPIRV-Cross heads;
this does not revalidate PR states. Next owner command uses the focused private
selection with the existing fatal windows.


### Owned compute-shader decode rejection with a frozen complete CPU tail

The October 23 08:39:07 UTC owner capture (10:39:07 Madrid) stops after 151.065 s
with confirmed exits 23/23. The matching renderer records unknown instruction
format in the main shader decoder, then asserts. The logged stage is verified
against the selected HwStage enum as Compute. Rejection is at word offset 2 in
an owned 523-word supplied program, with 520 remaining: not a recorded out-of-
slice read. The first-instruction prologue warning precedes it. Unsupported
instruction-table coverage, a wrong/stale program source and invalid initial
capture remain unresolved; owning bytes prevents later reuse from changing that
copy, not a non-atomic/incorrect initial capture. Exact words/IDs/addresses stay
private. This is not the previous fragment metadata stop, CPU SIGSEGV or silent
freeze, and does not prove those resolved or causally related.

The host assertion history bridge/freeze is now exercised natively: 2,048 valid
records, frozen flag set, matching footer, no busy drops, skipped-in-window,
malformed or duplicate records. However 95,116,576 attempted records and
10,166,583 suppressed observations leave just 12.879 ms across three threads and
201 readable object addresses. The tree probes are exercised, but no writer or
connection to the failed compute program is demonstrated. No sampled nonzero-
to-zero transition is observed; absence in this short tail is not lifetime proof.

Automatic diagnostic.json exists and exactly matches offline reconstruction.
It labels the renderer assertion but currently omits the specific decode reason
from structured stop fields; that reason remains explicit in the raw log. There
are 1,473 API presentations, last t=150.1557 s, 422 pipeline failures across 386
pairs (374 adjacent PerVertex errors, 48 unattributed), and 805 SRT failures.
Last/peak presentation-sampled native allocation is 10.195 GiB, with last poll
830.774 ms old at that presentation, not failure-time. No OOM or large copies are
recorded; the last periodic memory summary has zero drops/errors. Audio has 58
windows, no queue-wait failures and maximum submit gap 90.130 ms; continuous
playback remains unverified.

Next preserve decode-specific provenance in the combined report, distinguish
invalid source from unsupported decoding using bounded original controls, and
reduce/filter CPU trace traffic. Do not invent an opcode, skip the shader or
claim PerVertex/SRT caused this assertion. No executable/config/dependency change
or automatic game launch follows this analysis.


### Owner-observed freeze with live audio, then window-close exit zero

The October 23 08:25:33 UTC owner capture (10:25:33 Madrid) loads the matching
prepared renderer/host and tree selection. The owner reports a frozen application
and closes it. The log ends at 547.666 s with `Window: closed (quit requested)`
and child/supervisor exits 0/0. This is a window-close result, not validated gameplay
or a recorded fatal crash. There is no CPU fault, shader-metadata rejection, native
command failure or GPU-watchdog marker. The automatic private `diagnostic.json`
is present and exactly matches an offline reconstruction from the saved evidence.

There are 1,525 retained API presentations, last at journal t=156.8915 s: about
6.5 minutes without new presentations before closing. Audio continues for more
than six minutes after the last resource-journal event, so at least those workers
remain active; this does not disprove the owner's apparent UI/system freeze or
identify the blocked thread. Both presentation rotations are included. The final
pipeline count is 414 across 378 pairs (366 immediately adjacent PerVertex errors,
48 unattributed) and SRT remains 805. Correct pixels or a shader fix is unproved.

Last/peak presentation-sampled native allocation is 9.114/9.145 GiB, not at close
or freeze detection. No OOM is recorded, but there are 38 completed bounded copies
of approximately 78 MiB each. The last periodic summary's 2/2 copies precedes
36 later buffer-journal copies and must not be called the final total. Its dropped
record and observation-error counters are zero. Audio has 216 windows and zero
queue-wait failures, but a maximum submit gap of 2.154 s occurs earlier; continuous
playback remains unvalidated.

No history is dumped because no fatal/cooperative stop path runs on normal window
closure. Thus the new shader/Metal history bridge and tree observations are still
unvalidated natively, not shown empty. The supervisor listens for the renderer's
watchdog message; it is not an independent monitor of missing presentations with
live audio. Existing fence-wait/committed-work watchdogs do not cover every CPU/
queue/UI deadlock. Capture thread stacks and independent CPU/render/submission
progress during the freeze, and preserve history on an owner-requested close,
before another identical comparison. The final unimplemented LOD-stats warning
also appears in progressing earlier runs and is not a demonstrated cause. Raw
addresses/journals/review remain private. No executable/config/dependency changes
or game launch follow this analysis.


### Prepared combined stop evidence and metadata-history correction

Required shader-metadata rejection and native Metal-command failure now invoke
the host's existing once-only CPU history hook before further file/report work.
On macOS the ordinary renderer error path resolves that optional host symbol
through the loader; standalone hosts can omit it. Signal handlers still use the
local lock-free snapshot directly. The hook freezes newly entering observations
and retains the bounded ring; it does not reread guest fields, change the real
exit 23, fabricate metadata or make a failed command successful. Valid and
speculative shader paths never invoke it. The existing assertion hook remains.

Metadata evidence now retains the full bounded scanner slice, including the
previously omitted trailing 28 bytes for the current BinaryInfo layout. Stop
records distinguish found metadata, declared length and metadata offset. The
saved slice remains a separate best-effort backing read, not an atomic snapshot
or proof that mapping implies shader validity. The actual invalid program's
origin/lifetime/write visibility remains unresolved; no semantic shader fix is
claimed and no search bound is relaxed.

The graphics entry now writes private `diagnostic.json` in each fresh run's state
directory after the child stops. It combines stop markers and real exits, CPU
history completeness/time span, startup host candidates, pipeline/SRT counts,
both rotating presentation journals, last periodic memory evidence, audio metadata
and a bounded shader snapshot fingerprint. Up to eight preceding graphics logs
are listed separately, retaining distinct guest fault sites and missing evidence.
Counts/co-occurrence do not establish a common cause. Missing memory/audio/history
stays unavailable, not zero; report failure preserves the original diagnostic
exit. This offline report uses no driver, game execution or network access.

Two metadata tests exercise the real wrapper with the host hook present/absent,
valid/speculative controls, unchanged exit 23, first-evidence preservation and the
scanner-sized snapshot. Three submission-journal tests include Metal hook/no-hook
controls; three combined-report tests cover distinct stops, rotated journals,
malformed tails and absent-versus-empty history. Full JIT/interpreter contracts,
ARM64/all 22 renderer CPU checks and 46/46 imports pass. Owner-run evidence is
still required for this new renderer; the private tree command stays selected
so a shader stop can now retain its currently missing CPU context.

Public HTTPS refs checked October 23 08:21:07 UTC retain the reviewed SotC,
runtime, CPU, MoltenVK and SPIRV-Cross default heads. This does not refresh PR
states. Pins and shader cache 28/9 remain unchanged. No game is launched.


### Required fragment-shader metadata stop bypasses CPU history

The October 23 08:11:06 UTC owner capture (10:11:06 Madrid) enables the exact
21-observation tree selection and matching prepared binaries. It stops after
165.141 s with confirmed child/supervisor exits 23/23: required fragment-shader
metadata lookup fails on the owning GPU command thread during indirect drawing.
There is no repeated translated CPU SIGSEGV or GPU watchdog stop; neither is a
proven fix. The reported stage role determines classification, not a templated
cold-symbol name in the native backtrace.

The lookup reports mapped backing, no recognized offset hint and a bounded
65,564-byte scan without accepted metadata. Private evidence saves a 65,536-byte
snapshot and context. No expected metadata signature occurs in that saved slice;
the slice is shorter than the scan and is not an atomic memory snapshot. Mapping
does not prove shader validity. A stale/wrong program address, overwritten/reused
backing, CPU/GPU visibility or lookup limitation remain candidates, not findings.
Do not fabricate a shader hash/length, skip required rejection or expand scanning
without a validated bound. Raw bytes, addresses and reports remain private.

No CPU history is dumped. Source inspection of the matching renderer confirms
that required metadata failure writes its evidence/backtrace and calls `_Exit(23)`
directly, bypassing the optional assertion hook. The tree probes are configured,
but this stop cannot validate their observations or the new freeze behavior;
zero parsed events does not establish an empty ring. Before another comparison,
preserve history on this stop and review program-register/command provenance,
backing lifetime and write visibility. No common cause with earlier CPU faults
or the previous watchdog is established.

Both rotating visual files together contain 1,588 API presentations, with last
journal t=164.3542 s. There are 462 pipeline failures across 419 pairs: 403
immediately adjacent PerVertex conversion errors and 59 unattributed, plus 805
SRT failures. Last native presentation sample is 10.507 GiB and sampled peak
10.522 GiB, not failure-time memory. No OOM or large copy is recorded. The last
periodic memory summary has zero image/buffer/summary drops and observation
errors. Audio has 64 windows, zero queue-wait failures and maximum submit gap
36.319 ms; uninterrupted audio and correct pixels remain unvalidated. This
analysis changes no executable, configuration or dependencies and launches no game.


### Prepared linked-node capture and termination freeze

The follow-up freezes new CPU observations as soon as an unrecovered fatal
signal is selected, before printing register/memory context. Renderer assertions
and cooperative history dumping also freeze first. The atomic flag prevents new
callbacks from reading guest fields or reserving slots; already-entered callbacks
may finish, with no wait or thread suspension. Recovered faults return before the
freeze. Metadata reports the flag explicitly. This addresses avoidable diagnostic
overwrite during reporting, not the invalid guest node or its writer/lifetime.

A separate ignored private selection replaces the previous broad array command:
21 observations at six reviewed function/call-continuation/branch entries follow
container/header/root candidates, lookup-key fields and both node-link branches.
Identical values are suppressed at these read sites. It follows no field as a
pointer and retains no game-specific addresses in public source. Adjacent block
fallthrough and finite history still limit coverage; reduced traffic and longer
native retention must be measured, not assumed. Custom fatal windows cover the
container/root/sentinel and node candidates instead of the old array offsets.

Instruction starts and executable bounds match the actual private linked input.
The actual rebuilt ARM64 diagnostic accepts both configs and deliberately stops
on missing input before CPU/GPU initialization. The five native CPU tests include
eight-producer freeze controls, unchanged errno/registers, no newly entering
post-freeze field reads/reservations, actual fatal exit 139 and cooperative exit
21 in both modes. Full synthetic contracts and 46/46 host imports pass; the prior
22 renderer CPU checks remain applicable because no GPU code changes. Native
node capture and actual crash correction remain pending the owner's execution.

Public HTTPS default-branch refs checked October 23 08:05:04 UTC retain the
previously reviewed SotC/runtime/CPU/MoltenVK/SPIRV-Cross heads. Earlier PR-state
verification limits remain separate. No dependency/cache update or game launch.


### New owner fault in linked-node key comparison

The October 23 07:57:24 UTC owner capture (09:57:24 Madrid) loads the matching
prepared binaries and array selection, then stops after 160.533 s on translated
CPU SIGSEGV, confirmed child/supervisor exits 139/139. It does not reach a GPU
watchdog stop. Static instruction/caller bytes match the private linked input:
a key comparator reads through an invalid pointer supplied by a linked-node
traversal. The candidate node is already invalid before that read. Root versus
left/right-link origin, writer, lifetime, concurrency and CPU translation remain
unproved. This differs from the earlier array read and original-libc string fault.
Exact offsets, disassembly, addresses and reports remain private.

Fatal history now emits 1,441 valid records and a matching footer. A complete
dump does not imply complete execution history: 61,086,239 attempted records,
147,448,114 identical observations suppressed, and 607 unavailable/replaced slots
in the selected 2,048-slot window leave only 4.366 ms of readable history across
three threads. These probes still target the previous array, not the current
container root/node links. Custom fatal windows based on the invalid argument
are unreadable. Do not repeat this broad selection unchanged or infer a writer
from unrelated retained objects. Retarget to reviewed traversal entry and link
sources with bounded/filterable traffic before another origin comparison.

There are 1,470 API presentations; the last is at journal t=159.8288 s. Recorded
pipeline failures are 444 across 401 pairs: 385 immediately adjacent PerVertex
conversion errors and 59 unattributed, with 805 SRT failures. Different workload
and failure boundaries prevent treating lower pipeline counts as a correction.
The last/peak presentation-sampled native allocation is 9.905 GiB, sampled about
810 ms before that presentation, not at fault time. No Metal OOM or large copy is
recorded. There are 26,893 binding-size clamp messages and eight unsupported image
format/type/usage combinations; neither establishes the CPU fault's cause.

The image journal reaches 10,851,461 bytes, exceeding the old 8 MiB cap, and the
last periodic summary reports zero image/buffer/summary drops and zero observation
errors. The larger evidence cap is exercised; it is not a GPU memory saving.
Audio has 62 windows, no queue-wait failures and a 39.550 ms maximum submit gap;
continuous audio is unverified. Cooperative 128-record stopping and the separate
GPU candidate tail are not exercised by this CPU failure, nor is the previous
GPU stall resolved. No runtime/config/dependency change or game launch follows
this read-only analysis.

### Prepared cooperative history and awaited-batch evidence

The latest watchdog exposed a diagnostic defect: cooperative cancellation bypassed
the CPU ring dumper. Its terminating worker now attempts the latest 128 atomic
observations after a successful nonblocking stop notice. It never reads guest
memory, allocates, acquires a lock, retries writes or waits for output. Unsupported
outputs and the first failed/short write skip the remainder. SIGPIPE is blocked
only in this worker so a closed reader preserves the intended exit 21. Full
signal/assertion snapshots retain their 2048-record budget. Snapshot metadata and
the analyzer distinguish missing, partial and complete dumps; truncation does not
establish an empty ring. Startup publication of the image base is atomic.

The GPU journal now keeps a separate bounded tail of 128 guest draw/dispatch
candidates, independent of the 96-note state/native tail. This can retain all
118 candidates from a batch shaped like the last capture, but these remain CPU
recording notes, not GPU progress or proof of the stalled shader. The image event
file cap increases from 8 to 32 MiB to delay observed evidence loss; it remains
finite, cannot restore old drops and does not reduce guest/Metal allocation.

Five native CPU tests cover both execution modes, 300-observation/128-record
budget controls and drained/full/closed output. Real cooperative exits remain 21
without an external signal; full output does not delay termination. Four offline
analysis tests, two GPU-journal tests (state-note saturation, candidate overflow,
merging/recycled buffers), 22 supervisor tests and full JIT/interpreter contracts
pass. The ARM64 GPU build and all 22 renderer CPU checks pass; all 46 host imports
are present. No native game is launched and no hang/corruption/shader correction
is established. The existing ignored array-state command is the next owner run.

Public Git HTTPS refs checked October 23 07:50:10 UTC retain the previously
reviewed SotC, runtime, CPU, MoltenVK and SPIRV-Cross heads. This ref check does
not revalidate PR state. Dependency pins and cache 28/9 remain unchanged.


### Array-targeted owner run stops on scheduled GPU work

The October 23 07:39:10 UTC owner capture (09:39:10 Madrid) enables the exact
private array-state selection and matching binaries. It stops after 159.147 s
on GPU progress: awaited submission 103707 does not finish within 10 s, while
103706 is complete and recording reaches 103713. The supervisor requests
cooperative termination; the child notice appears 0.909 ms after that request,
and child/supervisor exits 21/31 are confirmed. This is an exercised stop, not
an unknown child exit or the prior translated CPU SIGSEGV. No unknown-format,
sample-count or Metal OOM stop is recorded; their nonrecurrence is not a fix.

The awaited batch contains two command buffers. Both Metal commit calls return
and both scheduled callbacks occur; completed callbacks remain absent. The
report-time weak probes are available, status 3/error 0, with retained references,
and observe native allocation 10.023 GiB at the stall report. Status/commit/
scheduling is not evidence that any particular kernel made progress. The preceding
batch has a completed callback and native status 4; its released weak probe is
unavailable, not a zero-memory observation.

The full awaited CPU note summary contains 653 notes: 17 guest draw and 101 guest
dispatch candidates (plus overlapping native/state/barrier/copy descriptions).
Only 96 individual notes are retained. There are no missing batch notes or Metal
events reported, but the journal's bounded old-batch eviction is explicit.
These are CPU candidates, not a GPU trace; do not blame the final dispatch or a
PerVertex pipeline just because its hash occurs nearby.

There are 1,336 API presentations, the last at journal t=148.3402 s; 477 pipeline
failures across 426 pairs (407 immediately adjacent PerVertex errors, 70
unattributed) and 805 SRT errors. Last native presentation sample is 10.003 GiB
and its presentation-sampled peak 10.003 GiB; the separate stall-time native
probe above is newer. No large copies are counted. The final periodic memory
summary reports 7,229 dropped image records, zero buffer/summary drops and zero
observation errors: full image-lifetime absence claims are unavailable. Audio has
62 windows, zero queue-wait failures and maximum delivery gap 30.796 ms; this
does not establish uninterrupted playback. Pixel capture remains off.

No CPU field-history events or summary are dumped. Read-only source inspection
confirms the cooperative cancellation worker writes a best-effort nonblocking
notice then `_Exit(21)`, bypassing the current signal/assertion history paths.
Thus this log cannot show the array's pre-read values or whether observations
existed in memory. Do not interpret the offline analyzer's zero events as an
empty/unexercised ring. Native array-state capture remains unresolved.

The next diagnostic gap is retaining bounded CPU history on cooperative stop
without blocking cancellation, together with isolating the awaited batch's
shader/resource/synchronization candidates. Preserve watchdog timing, real
completion, memory guards and real exit behavior; do not extend the timeout or
skip dispatches. This analysis makes no code/config/dependency changes or game
launch. Exact shader IDs, native addresses and reports remain private.



### Prepared owner array-state command

After the 09:33:36 Madrid capture returned to the earlier array-pointer fault,
the owner requested a targeted command. Separate ignored private files select
18 observations at three reviewed executable entries: consumer function entry,
the block after count checks, and the array-read loop. Each observes the apparent
object header, range begin/end, array pointer, destination base and packed count.
Loop observations use identical-value suppression; source observations remain
unconditional. Custom fatal fields now cover the relevant array source windows,
replacing the formatting windows for this command only. Exact offsets remain
private; no pointed-to array/string is followed, guest fields changed or read
skipped. Bounded site history does not identify a writer/allocation/lifetime or
make the fields an atomic snapshot.

The actual rebuilt diagnostic accepts both configs and intentionally stops on a
missing input before CPU/GPU initialization. Read-only executable-range checks
and ignore checks pass. No new runtime code, dependency, cache or game launch
is involved. Public Git HTTPS refs at October 23 07:38:25 UTC reconfirm unchanged
SotC/runtime/CPU/MoltenVK/SPIRV-Cross heads; earlier PR-state verification limits
remain separate. Native array history is pending the owner's execution.



### Filtered capture returns to the earlier array-pointer CPU fault

The October 23 07:33:36 UTC owner capture (09:33:36 Madrid) enables the exact
filtered 18-observation selection and loads matching prepared probe/renderer
UUIDs. It ends after 96.320 seconds on translated CPU SIGSEGV, confirmed exits
139/139. Private static inspection and the register snapshot identify the same
array-pointer read instruction as the October 23 06:24:04 UTC capture (08:24:04
Madrid), through a different invalid address. Before the loop, the pointer is
loaded from an object-relative source field; it and neighboring count/destination
fields lie outside the current default snapshot. The apparent object's first
words also look unlike the expected object header, but this does not prove a
freed allocation, writer, GPU corruption or translator defect. Current custom
fatal fields target formatting and do not cover this array state.

The filter explicitly rejects 1,581,784 unrelated observations. Ring attempted,
retained, overwritten, busy drops, skipped slots and identical suppression are
all zero; no malformed/duplicate records appear. No unconditional original-warning
source-block record is observed, and no formatter matches are retained. Thus
there is no current string-argument chain to compare, not evidence that the
invalid string was repaired or that the warning could never execute. Native
negative filtering is exercised; native positive matching for that target is
still absent. The earlier 3.3-million-record overwrite problem does not recur.
Callback overhead remains, and run lengths are not a benchmark.

There are 1,145 API presentations, 439 pipeline-failure events across 402 shader
pairs (388 immediately adjacent PerVertex errors, 51 unattributed) and 805 SRT
errors. Last and maximum presentation-sampled native allocation is 9.557 GiB,
194.251 ms old at the last presentation, not measured at failure. No Metal OOM
or large copies are recorded; memory-journal drops/errors are zero. Audio has
36 windows, zero queue-wait failures and maximum delivery gap 54.052 ms. Scene
requests remain 1920x1080, output 1280x720 and visual capture off; pixels and
synchronization remain unverified. Shader-format and sample-count failures do
not recur in this capture, without establishing their resolution.

The next source question is this array object's pointer/count state before its
consumer and its ownership/writes, rather than broader formatting traffic. Do
not skip the read or substitute a fabricated pointer. This analysis changes no
code, private selection, dependency or cache and launches no game. Exact fields,
addresses, disassembly and the comparison report remain private.



### Prepared selective format tracing and reaffirmed runtime review

The owner requested correcting the saturated trace and a new command, then
reviewing bloodborne-runtime as a permanent direct source. The generic optional
trace syntax now accepts `:on-change:match-register:match-image-offset` after
its first three fields. It compares a saved register to image-base plus the
configured offset, validates bounds/overflow at startup, and rejects unrelated
calls before protected guest reads, timestamps, cache updates or ring reservation.
A separate `filtered` counter distinguishes selection from overwritten/dropped
records. Existing three/four-field syntax, real fatal exits and the 2048-record
ring remain unchanged. No hardcoded game filter enters public source.

The new private command selection has 18 observations at five reviewed entries:
source fields remain unconditional; logger/game formatter/length wrapper/guest
vsnprintf entries match only the original warning's format address. The broad
parser entry is removed. Exact executable sites and match bounds were checked
against the actual prepared image; the rebuilt diagnostic accepts the private
config then stops on a deliberately missing input before CPU/GPU initialization.
No game is launched. This repairs diagnostic selection, not the source pointer,
variadic ABI or unknown shader instruction. Probe callback overhead remains.

Four native CPU tests pass in JIT and interpreter, including actual loader
matching/rejection histories and fatal exit 139. Original fixtures check vector,
byte/errno preservation in both matching and filtered real probe callbacks,
100,000 unrelated observations that perform no guest-field read or ring/cache
mutation, changed selected values, overflow and malformed/out-of-image matches.
Two offline analysis tests preserve the filtered counter; full synthetic
JIT/interpreter contracts and 46/46 host-symbol audit pass. No renderer or
shader semantic change is made; cache 28/9 and dependencies stay fixed. Native
validation of the narrowed history and the shader/string failure remain pending.

[The direct-runtime review](UPSTREAM_REVIEW.md#direct-runtime-review-2026-10-23)
inspects the public 30-commit / 149-path compare against the selected base.
Instanced tessellation is already adapted; FSR queue/shared-event ordering and
adapter changes are future separate integration candidates. Critical-pressure
content-losing eviction is not adopted. No direct decoder/CPU implementation
repair is found. The source remains explicit in the permanent watch list; no
duplicate or reactivated scheduled task is created by this review.

Before proposing the command, the public API was rate limited. Public Git HTTPS
refs reconfirmed the same SotC/runtime/CPU/MoltenVK/SPIRV-Cross heads as the
preceding review; new commits/diffs against last-reviewed heads are absent.
PR states retain the earlier noted verification limits. This does not silently
advance a pin or claim new native stability.



### Format follow-up stops on owned shader decoding, not strlen

The October 23 07:19:39 UTC owner capture (09:19:39 Madrid) exercises the exact
23-observation format selection and matching current binaries. It stops after
94.992 seconds, confirmed exits 23/23, on `unknown_instruction_format` in the
main shader decoder. The supplied owned input contains 101 words; rejection is
at word offset 26 with 74 words remaining. This is not a recorded supplied-slice
bounds failure. The rejected SMRD slot has no recognized format in the installed
tables. Unsupported code, stale/wrong shader source or already-invalid capture
are not distinguished. Owned input protects subsequent source reuse, not atomic
initial capture or correctness of supplied bytes. No shader is repaired/skipped.
Neither the preceding strlen SIGSEGV nor sample-count failure recurs; this does
not validate their resolution or establish a common corruption cause.

There are 1,150 API presentations, 453 pipeline failures across 414 shader pairs
(399 immediately adjacent PerVertex errors, 54 unattributed), and 805 SRT errors.
No Metal OOM or large copy occurs in the recorded evidence; memory journal
errors/drops remain zero. Last native presentation sample is 9.991 GiB and the
sampled maximum 10.025 GiB; the last poll is 489.347 ms old at presentation, not
at assertion time. Audio has 36 windows, zero queue-wait failures and maximum
delivery gap 37.108 ms. Visual capture remains off; neither correct pixels nor
smooth/synchronized audio is established.

The renderer assertion now successfully dumps CPU history before exit 23,
validating the optional assertion hook on the native owner path. However the
broad formatting selection attempts 3,318,674 observations, overwrites 3,316,626
and retains only 2,048 observations spanning 0.332 seconds. No busy drops,
skipped slots, unreadable fields or malformed/duplicate records are observed.
All retained samples are general length/vsnprintf/parser sites; none is the
original source-field block. Thus this capture cannot compare the original
warning's fields with the failing string arguments. Do not interpret absence
of an observed clear as evidence against invalidation. This high-frequency
configuration can change timing; the longer run is not a performance result.
Do not recommend repeating it unchanged: narrow source-specific coverage or add
qualified filtering before another origin-focused comparison.

The next review priorities are preserving the exact original warning's source
arguments and distinguishing invalid/unstable shader input from a genuinely
missing decoder format. No safe semantic correction is established by this log.
This analysis changes no executable, dependency, cache or runtime selection and
launches no game. Exact shader identifiers, bytes and addresses stay private.



### Latest owner CPU fault: invalid string during formatting

The October 23 07:12:03 UTC owner capture (09:12:03 Madrid) loads matching newly
prepared probe/renderer UUIDs and ends after 83.739 seconds on translated CPU
SIGSEGV, confirmed exits 139/139. Read-only module metadata, export identity and
private disassembly identify the first word read in the supplied guest libc's
`strlen`; its incoming string pointer is invalid before the read. Candidate
stack returns and their static call sites match a `vsnprintf` string-formatting
path from a game warning. This does not prove who supplied/corrupted that pointer,
its lifetime, a variadic ABI defect or CPU mistranslation. It is another stopping
boundary, distinct from the earlier null object and sample-count assertion.
No sample-count rejection or renderer assertion occurs in this run; their absence
does not validate a fix for the preceding native assertion.

There are 1,075 API presentations, 473 pipeline failures across 421 shader pairs
(402 immediately adjacent PerVertex errors, 71 unattributed), and 805 SRT errors.
No OOM/large copy is recorded; memory journal drops/errors remain zero. Last and
maximum presentation-sampled native allocation is 9.678 GiB, 524.259 ms old at
the last presentation, not a failure-time measurement. Audio has 32 windows,
zero queue-wait failures and maximum delivery gap 45.471 ms. Visual capture is
off; no pixel, smooth-audio or controlled performance claim follows.

CPU history parses without malformed/duplicate events: 83,510 attempted,
2,048 retained, 81,462 outside the ring, 117,376 identical hot observations
suppressed; no busy drops/skipped slots. It spans 6.423 seconds and observes no
nonzero-to-zero transition in that retained window. The prior private object
selection does not cover this string's source; missing history cannot establish
that it was never initialized or cleared.

For the owner's requested next command, a separate ignored format-only selection
captures 23 fields at six reviewed function/conditional-block entries: original
source pointer fields before the warning, the two actual stack string arguments,
and format/va_list metadata before the nested formatting calls. Repeated general
object consumers are excluded from this run's selection so they cannot overwrite
this evidence. No pointer chasing, guest bytes or fields are changed. Exact game
offsets, strings and selection files remain private. Read-only validation against
the actual prepared executable segments, the real diagnostic's config parser
(stopped on a missing input before CPU/GPU initialization), and all three native
CPU trace tests pass. The command uses explicit private config overrides; no
new code/dependency/cache change or game run is performed here.

The pre-test public-source check at October 23 07:16:35 UTC resolves the same
reviewed SotC/runtime/CPU/MoltenVK/SPIRV-Cross heads; no new commit diff is present
against the preceding review. SPIRV-Cross proposals 2699 (draft) and 2697 remain open/unmerged at their
reviewed heads. GitHub rate limiting prevented fresh MoltenVK 2858 and SotC 219
PR-state checks; their last reviewed state is retained, not reconfirmed.
No dependency is updated. The next question is whether the invalid string is
already present in the original field/stack argument or appears later while
formatting; a later changed failure may still leave that question open.



### Owner assertion boundary and checked sample-count rejection

The latest October 23 06:59:22 UTC owner capture (08:59:22 Madrid) stops after
84.545 seconds with the renderer's `RawNumSamples` assertion, confirmed
child/supervisor exits 23/23. The old converter accepts only guest counts
1/2/4/8/16; its log omits the rejected value and caller. This is a different
boundary from the preceding CPU faults. It does not prove an invalid descriptor,
use-after-free, device limitation or shared corruption cause.

There are 1,063 API presentations, 422 pipeline-failure events across 386 shader
pairs (373 immediately adjacent PerVertex errors, 49 unattributed), and 805 SRT
errors. No Metal OOM or large copy is recorded; memory summary drops/errors are
zero. Last and maximum presentation-sampled native allocation is 9.837 GiB,
288.180 ms old at the last presentation, not measured at assertion time. Audio
has 32 windows, zero queue-wait failures and maximum delivery gap 98.639 ms.
Scene requests remain 1920x1080, output window 1280x720, visual capture off;
API counts do not establish pixels. No controlled shader/memory improvement is
inferred from differing run lengths or counts.

The preceding 06:57:48 UTC capture (08:57:48 Madrid) ends after 69.558 seconds
on another translated invalid-object-pointer SIGSEGV, exits 139/139. Private
static inspection confirms the argument comes from a previously loaded field;
no writer/lifetime cause is proven. There are 974 presentations, 285 pipeline
failures (253 adjacent PerVertex), 271 SRT errors, zero large copies/OOM,
8.859 GiB last native sample / 8.868 GiB sampled maximum, and zero audio queue
failures with maximum gap 50.982 ms. Its field trace works: 32,715 attempted,
2,048 retained, 30,667 older-than-ring and 54,864 suppressed identical hot-site
samples; no busy drops, skipped slots or malformed/duplicate events. The retained
window spans 21.835 seconds. Its sampled nonzero-to-zero transitions end at an
initialization observation and can include reused addresses; they do not prove
a corrupting clear. Fatal selections have no matching retained history for this
different boundary. Exact offsets, fields and offline reports remain private.
Both captures load the expected prepared probe/renderer UUIDs.

Owner-authorized follow-up adds a checked `sotc-sample-count-validation-v1`
overlay. Actual pinned-method controls reproduce returning e1 even when absent
from the supported mask. Selection now rejects an empty/no-matching downward
fallback instead of inventing an unsupported device state. The accepted guest
count domain and every valid downward conversion remain unchanged. Failure
provenance records requested count, supported mask and all five caller roles:
image creation, backing swap, pipeline raster/color/depth. Invalid requests still
stop with real exit 23; no unsupported sample, fence, pipeline or object is
manufactured. The latest native failure's raw count remains unknown until this
new provenance is exercised.

The assertion shim calls an optional weak host hook before `_Exit(23)` so the
existing bounded atomic CPU history is retained on renderer assertions too.
Absent-hook standalone hosts preserve exit behavior; no guest reads, registers,
write attribution or CPU/GPU completion proof are invented. Actual installed
CPU controls reproduce the pinned defect, compare 1,032 valid conversions,
reject 248 unsupported masks and exercise invalid-count/fatal-origin controls
with the hook present and absent. ARM64/all 22 renderer CPU checks and 26 builder
tests, full JIT/interpreter contracts and 46/46 host-symbol audit pass. The optional
hook is exported by the prepared host and weak-imported by the renderer.
Cache remains 28/9 and all dependency/base pins and prior overlays
remain fixed. Native crash resolution is unverified; no game/GPU is launched.


### Prepared field history and Vulkan-Headers 1.4.366 integration

The owner requested tracing the repeated null field and reviewing/updating
dependencies. Static private inspection found creation/loading/destruction/use
paths, including an initialization that writes zero legitimately. This does not
show which path the failing native object took. The new generic opt-in
`SOTC_CPU_FIELD_TRACE` uses the selected CPU's existing probes at reviewed function
and call-continuation block entries. A private ignored selection installs 17
site/field observations; no game offsets or instructions enter public source.
The shared graphics entry loads the private selection and the supervisor records
it. Both command files/direct packaging routes honor explicit overrides/empty
disable. Nine real-entry stub controls stop before any driver/GPU action.

One protected 64-bit field is observed at each configured site, without pointer
chasing, guest writes, return-slot edits or resource changes. A fixed atomic
2048-record ring preserves bounded history for fatal dumps in JIT and interpreter.
Hot consumer sites suppress identical samples in a bounded per-thread cache;
lifecycle sites retain all observations. Drops/overwrites/readability/suppression
are explicit. These are sampled site observations, not exhaustive writes,
allocation identities, writer attribution or CPU/GPU ordering proof. See
[CPU diagnostics](CPU_DIAGNOSTICS.md) for syntax, status bits and offline analysis.

Three original native CPU tests pass, including actual loader fatal histories
and rejection controls; direct probe/ring fixtures run with UBSan/bounds in both
modes and preserve vector state, bytes and errno. Two offline-parser tests and
22 supervisor tests pass. Full synthetic JIT/interpreter contracts pass; no game
is launched. Native history and a corruption correction remain unverified.

The dependency review found Vulkan-Headers **1.4.366**, commit
`73814d006638aa00bd553215b054f0f02e457e22`, published as a source update October 23
01:14:01 UTC. Its registry/generated C/C++ diff was inspected and integrated.
The recipe now selects loader **1.4.365** independently, avoiding an unsupported
assumption that each header tag has a matching loader release. ARM64 library,
all 21 renderer CPU checks, 26 builder tests and 46/46 host-symbol audit pass.
Installed version markers and the build report identify headers 366/loader 365.
Cache remains 28/9, with all existing memory/shader/audio overlays retained.
The runtime/CPU/reference base pins do not change.

Other selected stable library/tool channels were refreshed: MoltenVK 1.4.2,
SPIRV-Cross SDK 1.4.363.0, SDL 3.4.18, glslang 16.6.0, Dear ImGui 1.93.0 and
Zycore 1.5.2 remain current in their reviewed channels. GitHub's release endpoint
is not a valid latest-tag oracle for every repository; loader/headers tags and
Zycore tags were checked separately. Runtime head `0d2ffb3470da7e56dd82cc8ef08f58b5e777abbd`
and app head `f761502eb4d34e92a5446751f3d8f1d2140339ad` add Metal FSR 4.1.1/adapter
and launcher work; this is a separate unqualified architecture migration, not a
source-field fix. CPU head `ef153b8d35768aff86fa1643c12e7d0340b16501` still adds
CI/link changes rather than a newer CPU implementation. No unmerged PerVertex
proposal, alternative driver, system software or game module is installed.

### Targeted capture exercised: source field is zero

The October 23 06:29:14 UTC owner capture (08:29:14 Madrid) explicitly enables
the prepared field selection and emits the configured-field records. It ends
after 69.639 s on translated CPU SIGSEGV, confirmed exits 139/139, at the same
null-object field-read instruction as the 06:09:33 capture. The earlier caller's
source field and the neighboring word are both zero in the protected snapshot,
consistent with the null argument. This closes the missing-field evidence gap;
it is not an atomic execution history and does not identify who initialized,
cleared, freed or otherwise invalidated the object. Other fields selected for
the intervening array-pointer fault stop safely because their base is null in
this different boundary. They do not demonstrate a reader failure.

Balanced-v2 and the expected probe/renderer UUIDs are loaded. There are 984 API
presentation results, 327 pipeline failures across 302 shader pairs (289 with
adjacent PerVertex conversion errors, 38 unattributed), and 271 SRT errors.
Fewer events before an earlier stop do not establish shader improvement. Visual
capture remains off. No Metal OOM or large copy is recorded, with no memory
journal drops/errors. The last and largest presentation sample is 8.937 GiB,
633.819 ms old at that presentation; it does not measure failure-time pressure.
Audio records 26 windows, zero queue-wait failures and a 94.839 ms maximum
delivery gap; absence of queue errors does not establish smooth playback.

The next causal question is the source field's initialization/clearing and object
ownership history. Do not skip the read, manufacture an object, attribute this
to PerVertex/SRT, or declare CPU/GPU corruption proven from the zero alone.
Exact field selection, addresses and reports remain private. No code, runtime
setting, dependency or cache changes accompany this analysis; no game is launched.

### Latest capture and corrected diagnostic activation path

The October 23 06:24:04 UTC owner capture (08:24:04 Madrid) ends after 86.294 s
on a different translated CPU SIGSEGV, exits 139/139. Private disassembly shows
an invalid array-pointer read; preceding source fields are outside the default
register snapshot. The writer, lifetime and shared cause with prior faults are
unknown. It records 1050 API presentations, 432 pipeline failures across 396
shader pairs (386 adjacent PerVertex, 46 unattributed), and 805 SRT errors.
There are zero recorded large copies, no Metal OOM, and no journal drops/errors.
Last native allocation is 9.777 GiB, with a 777.057 ms-old sample at the last
presentation, not at failure. Observed presentation-sample peak is 9.810 GiB.
Audio has zero queue-wait failures and a maximum delivery gap of 32.439 ms.
Neither pixels nor synchronization is established by these API/timing records.

The settings header explicitly records `SOTC_CPU_FAULT_FIELDS=null`, so this
capture did not exercise the selected source fields. Earlier activation only
covered the dedicated native command. The private-file reader now lives at the
common `packaging/test-arm64.sh --graphics-game-test` entry: both command files
and direct packaging invocation share it. Explicit environment values still
override the private file, including empty to disable. Nine real-entry synthetic
controls cover all three routes and override/disable; a Python stub stops before
any Vulkan/driver/GPU action. The private selection retains the earlier null
source and adds bounded fields for this newer array-pointer boundary. No game
was launched and no semantic crash correction is claimed. Exact fields, raw
disassembly and reports remain private. Earlier activation scope below is history.

### Owner-activated source-field capture

The owner explicitly requested activating the prepared diagnostic. The native
command now reads optional private `work/cpu-fault-fields.txt` into
`SOTC_CPU_FAULT_FIELDS`. It reads data, not shell code; an explicitly supplied
environment value takes precedence, including empty to disable the selection.
The exact source-field selection is installed privately and ignored by Git.
The existing bounded fatal reader and supervisor settings record remain unchanged.
Actual-launcher stub controls verify absent/private files, newline handling,
override and empty disable without starting a game/GPU. This activates additional
evidence only when the owner next launches the native command and a terminating
translated fault occurs. It does not trace earlier writers or establish a crash
fix. No new native comparison accompanied activation. Older inactive wording
below records the preceding preparation state.

### Latest owner result: null object in translated CPU code

The October 23 06:09:33 UTC capture (08:09:33 Madrid) loads balanced-v2 and
matching probe/renderer UUIDs, then ends after 80.855 s on translated CPU SIGSEGV,
confirmed child/supervisor exits 139/139. Private offline disassembly and register
context identify a field read through a null object argument. A preceding caller
loads that argument from a different object's field. Its source lies beyond the
default four-word register snapshot: the actual source-field value, writer,
initialization and lifetime are not captured. Earlier invalid-pointer faults
must not be treated as the identical instruction or an established common cause.

There are 1136 API presentation results, 377 pipeline-creation failures across
337 shader pairs and 271 SRT offset errors. Exactly 318 pipeline failures have
adjacent PerVertexKHR conversion messages; 59 remain unattributed (57 share a
fragment hash with conversion evidence elsewhere, not exact attribution).
General PerVertex and dynamic SRT remain unresolved. Visual capture is off:
presentations alone cannot establish visible logos or correct pixels. The last
four frame-stat intervals range from 9.6 to 14.7 FPS, not a controlled benchmark.

No Metal OOM or 3/3.5 GiB staging allocation is recorded. Last presented native
allocation is 9.557 GiB, with a 684.802 ms-old sample at that presentation; this
is not a failure-time pressure sample. Two completed requests of about 566 and
564 MiB miss a complete runtime mapping, not an import-window boundary, and use
one 640 MiB upload allocation. That staging inventory is 38.760 s old at the last
presentation; completed requests are not independent concurrent allocations.
All 246 recorded pool-window hits also fit balanced-v1 in actual-planner replay.
The run exercises v2 without isolating its extra coverage or proving a causal
memory/stability improvement over v1. Audio has no queue-wait failures across
30 windows, but a maximum delivery gap of 34.274 ms; synchronization is unproven.

An original private synthetic control exercises 1920 object-field/nested-call
cases: all non-stack base registers, unaligned/high-offset fields, a native call
boundary and deliberate null propagation. It passes in JIT and interpreter,
both with the pinned archive and with the current local CPU context/vector bridge.
This does not exclude other CPU defects, but gives no reproduced scalar-load or
call-preservation defect to repair. The known public texture-pool exhaustion
report concerns a different fault mechanism and does not identify this writer.

The owner's request to try correcting the crash does not justify skipping a
load, manufacturing an object or claiming success from unrelated tests. No
semantic runtime correction is established in this follow-up. A private inactive
`SOTC_CPU_FAULT_FIELDS` template targets the missing source field through the
existing bounded protected reader; exact offsets/disassembly stay under `work/`.
It changes no public game-specific code, active setting, dependency or cache.
Refresh the tracked public sources before proposing any native comparison.
No game or GPU was launched during this investigation.

### Prepared early-OOM correction: balanced-v2 import placement

The owner reports that the latest run did not show logos. The failure reviewed
below is Metal OOM, not a recorded CPU signal: the slightly-over-3-GiB binding
starts below the previous middle-window origin and allocates 3.5 GiB staging.
Twenty API presentation results cannot establish visible logos or valid pixels.

The checked `sotc-pool-window-placement` overlay now selects balanced-v2. It moves
the middle origin from 2 GiB to 1.75 GiB, shortens the prefix from 2.5 to 2.25 GiB,
and preserves the 3.5 GiB individual limit, 3 GiB stride and 512 MiB overlaps.
For the selected 6592 MiB pool the windows are [0, 2.25), [1.75, 5.25) and
[4.75, 6.4375) GiB: three imports, 7616 MiB imported aliases, exactly the previous
total. Aliases are not additional guest storage. Complete binding and BDA
selection still use each window's actual recorded origin; no partial binding,
early resource release, fake completion or shader approximation is introduced.

Replay compiles the maintained C++ planner and compares the actual recorded
policy rather than assuming every journal used legacy geometry. Across 44 private
retained journals and 10,390 observations, v1 predicts 28 crossing ranges and v2
predicts none, with zero observed direct-hit regressions. All three morning
failure ranges become direct bindings. Recorded events contain 73 crossings;
45 were already addressed by v1, so 73 is not the new improvement count. There
are no observed-policy mismatches or ranges outside the supplied 6592 MiB pool.
Repeated events are observations, not independent allocation savings; replay
cannot establish GPU alias correctness, residency, timing or native pressure.

Synthetic ASan/UBSan checks exercise 15 movable long ranges and every BDA table
entry, exact boundaries, overflow, missing/short mappings, full fallback and
legacy/v1/single controls. Crucially, the earlier 3 GiB range starting just above
2 GiB remains direct; the rejected 1.5 GiB proposal stays withdrawn. V2 is not
universally superior: a 3 GiB range starting at 2.5 GiB fits v1 but crosses v2.
The fixture requires its complete fallback, and no retained observation shows
this regression. A pool-size sweep checks coverage, individual limits and overlap;
equal import totals are asserted only for the selected pool, not all pool sizes.

Validation passes: ARM64 library, all 21 renderer CPU checks, 26 builder tests,
three replay tests and full JIT/interpreter contracts. Exact predecessor sources
migrate as `pool_v2_previous`; cache remains 28/9 and dependency pins unchanged.
`BB_POOL_LAYOUT=balanced-v1` restores the exact previous policy,
`BB_POOL_LAYOUT=legacy` restores the original policy, and `BB_POOL_SPLIT=0`
retains single-import behavior. No game or GPU was launched. Logos, lower native
pressure, sustained stability, general PerVertex and dynamic SRT remain unproven.

Private replay commands (outputs must stay under `work/`):

```sh
python3 tools/replay_pool_windows.py --all-retained --pool-mib 6592 \
  --output work/analysis/pool-v2-all-retained.json
python3 tools/check_pool_windows.py
python3 -m unittest discover -s tests -p test_pool_replay.py
```


### Latest owner result: early Metal OOM with a 3.5 GiB allocation

The October 23 05:46:41 UTC capture (07:46:41 Madrid) ends after 4.033 s,
confirmed child/supervisor exits 23/23, on Metal command-buffer error 8:
Insufficient Memory. The failure-time native property sample is 11.905 GiB,
0.008 ms old. The earlier presentation sample is only 7.900 GiB but 744 ms old;
these are different observation times, not contradictory failure-time pressures.

A contiguous read-only binding requests slightly more than 3 GiB and begins
below the 2 GiB middle-window origin. It misses complete import coverage and
reserves a 3.5 GiB dedicated upload. The forced memory journal records one
large-copy request and zero completions, without dropped records or observation
errors. Therefore allocation is observed, but completed copying is not. The
last dedicated-upload snapshot is 1.878 ms old at the native error. The recorded
92 live images occupy 353.38 MiB; that inventory alone cannot explain all Metal
allocation. There is one reserved/inflight native command at failure, limit and
peak eight; simply attributing OOM to a deep unfinished queue is unsupported.
The failed CPU-recording tail contains clear/blit/barrier candidates, not evidence
that an identified shader instruction caused the memory failure.

There are 20 presentation results, zero logged pipeline failures and 50 SRT
offset errors. No audio timing window is available in this short capture. No
SIGSEGV, CFG or newly added SRT-validation rejection is recorded. The early stop
does not validate resolution of those older boundaries or general PerVertex
support. Requested scene/output remain 1920x1080 / 1280x720, with zero scene-
reduction decisions; that request alone does not establish actual scene coverage.
Startup UUIDs match the current probe and cache-28/9 renderer, so current binaries
are loaded; individual correction paths are not thereby proven exercised.

The preceding 05:42:06 and 05:42:24 UTC captures also stop on Metal OOM 8, after
5.729 and 21.430 s, exits 23/23, at 11.907 and 11.704 GiB respectively. They
record completed large snapshots allocated at 3.5 and 3 GiB. Pipeline/SRT counts
are 0/50 and 27/228; all 27 adjacent conversion failures are PerVertex rejections.
These shorter captures must not be reported as reduced error rates versus the
80.900-second 423/805 capture. Their shared import-coverage failure remains the
next memory review priority. The prior failed 1.5 GiB-origin experiment stays
withdrawn. This review changes no runtime, dependency or settings and launches
no game; preserve the raw private logs and existing lifetime/failure contracts.


### Applied follow-up: libc write coherence and portable SRT safety

The owner requested investigating SIGSEGV, pipelines and SRT, including public
GitHub sources. The requested 306/271 counts describe an older capture; the latest
completed capture still has 423 pipeline failures and 805 SRT offset errors.
The source-field diagnosis above remains unresolved. No new game run was made.

Two guest libc imports previously resolved directly to host memcpy/memset. They
now follow the selected runtime's existing write contract: notify the GPU memory
tracker before writing protected pages, perform the exact libc operation, record
an opt-in existing CPU watch, then announce the completed CPU write. Return values,
fill-byte truncation, copy extent and ABI are unchanged. This closes a demonstrated
notification omission, not a demonstrated cause of the game's corrupted field.
Original CPU controls exercise the actual memory backend and imported wrappers:
a raw host-alias control produces no watch event; adapted operations notify before
clearing real read-only traps and after watched writes, with original/new data,
unaligned destinations, canaries, zero-sized and unobserved-range controls. Full
synthetic JIT/interpreter contracts pass. No GPU or game is initialized by them.

The portable SRT bytecode VM has fixed 32-value and 64-pointer stacks, but installed
programs previously had only size/alignment checks. An original-source ASan/UBSan
control reproduces a 33-value stack overflow. The checked `sotc-srt-program-safety`
overlay validates the owned program before installation/execution: payload extent,
opcode/zeroed-load flags, stack effects and balanced termination. Invalid programs
stop explicitly with shader provenance and exit 23; no opcode, table value or
branch is fabricated. It is structural validation, not a guest-pointer/flat-buffer
bounds proof. Existing read-fault behavior and resource lifetimes are unchanged.

Full-width unsigned bitfield extraction was independently incorrect: the SRT VM
masked width 32 to zero, while constant folding shifted a 32-bit one by 32 (UB).
Zero-width extraction at offset 32 also needed to avoid a shift by 32. Both paths
now preserve the defined valid range. Original controls reproduce the wrong SRT
read and constant-fold UBSan failure. 2,244 valid base/offset/width cases compare
actual installed SRT reads and constant propagation with a 64-bit arithmetic
oracle. Twelve malformed/invalid controls stop explicitly, and exact stack limits
remain accepted. These defects are not established triggers of the 805 native
errors, whose sampled shapes contain dynamic/Phi dependencies. DMA stays off;
unknown/cyclic/divergent dependencies are not resolved by choosing an edge.

ARM64 build, all 21 renderer CPU checks, 26 builder tests and full JIT/interpreter
contracts pass. Cache is **28/9**, fixed dependency pins unchanged. The exact
previous cohort migrates as `srt_program_previous`. The builder now derives its
reported cache version from checked serialization source and rejects a lock
mismatch, replacing an obsolete hardcoded report value. No cache/capture is purged.
Native visual, corruption, synchronization and memory-pressure effects remain
unverified. Read the new [public-source review](GITHUB_PROBLEM_REVIEW.md) for
PerVertex proposals and why unrelated upstream changes were not copied wholesale.


### Latest owner result: translated CPU fault with a visible source field

The October 22 22:26:02 UTC capture (October 23 00:26:02 Madrid) stops after
80.900 s, confirmed exits 139/139, on SIGSEGV in translated guest code. Offline
inspection of the private executable identifies a byte comparison through an
invalid pointer. The preceding instruction loads that pointer from a structure
field, and the bounded fault snapshot records the same invalid value in that
field. This narrows the source-field investigation; it does not identify the
writer, prove object lifetime/type, or distinguish guest corruption from an
incorrect earlier CPU/runtime operation. Do not skip the access or replace it
with a fabricated value. Unlike the earlier unmapped host-instruction failure,
this report supplies a translated guest instruction and source-field evidence.

The new native ARM64 register context is exercised and complete. The native PC
is in generated translated code; lack of a startup Mach-O range match is expected
for that code and is not evidence of a new invalid host branch. The recorded LR
matches the diagnostic executable's startup text interval, but remains an
unvalidated candidate rather than a reconstructed call stack.

There are 881 presentations, 423 pipeline failures / 388 shader pairs (376
adjacent PerVertex rejections, 47 unattributed), 805 SRT errors and no finite-SRT
expansion messages. Last presentation-observed Metal allocation is 9.497 GiB,
523 ms old; it is also the sampled peak, not exact fault-time pressure. Cumulative
large-copy requests/completions remain zero with no journal drops/tracker errors.
Last dedicated upload is 600 MiB, 353 ms old. Audio's maximum submit gap is
102.701 ms with zero queue-wait failures. Scene/output stay 1920x1080 / 1280x720,
windowed, with no reduced decisions. No Metal OOM, CFG or shader-decode stop is
recorded; their absence does not establish a fix or sustained stability.

The intervening 22:23:46 UTC capture instead stops on Metal OOM 8 after 19.143 s,
exits 23/23, at 11.665 GiB actual failure-time allocation, with a 3 GiB staging
snapshot for a contiguous binding beginning below the 2 GiB middle-window origin.
These outcomes demonstrate allocation-placement sensitivity of the current
geometry. The uncompiled 1.5 GiB-origin proposal left by the disconnected session
failed the existing 3 GiB-range regression and has been withdrawn; the source is
restored to the compiled 2 GiB-origin geometry. A new synthetic band control
preserves both successful coverage and the known full-copy fallback below that
origin. Original UnifiedRange/BDA ASan/UBSan checks pass after restoration. No
native benefit is claimed for the withdrawn proposal; no game was launched here.
The main-shader owned-input change remains built and CPU-tested, cache 27/9;
this capture does not by itself validate its effect on the previous decode stop.


### Latest owner result and owned main-shader input

The October 22 22:16:18 UTC capture (October 23 00:16:18 Madrid) ends after
116.574 s, exits 23/23, on `unknown_instruction_format` in main shader decoding.
An earlier unhandled DPP message does not establish that implementing DPP would
resolve the later failure. Unsupported input, incorrect shader extent and
corrupted/reused bytes remain alternatives. No CFG rejection, Metal OOM or native
fatal signal is recorded; absence in this run does not resolve previous faults.

The windowed startup is exercised: BB_FULLSCREEN=0, 1280x720 output, original
1920x1080 scene, upscaler none and zero reduced decisions despite inherited
preset 4. There are 943 presentation results, 423 pipeline failures / 388 pairs
(377 adjacent PerVertex rejections; 46 unattributed), 805 SRT errors and no
finite-choice expansion messages. Last native sample is 9.596 GiB, 337 ms old;
presentation-sampled peak is 9.601 GiB. Cumulative large-copy requests/completions
remain zero with no journal drops/tracker errors. Last dedicated upload sample
is 140 MiB, 13.917 s old, so it is not failure-time pressure. Maximum audio submit
gap is 30.340 ms, with zero queue-wait failures; this does not prove synchronization.

Review found that main-program translation borrowed caller storage for every
instruction read. The checked `sotc-shader-snapshot` overlay now copies exactly
the supplied dwords into compilation-local owned storage before decoding. It
keeps that storage through translation, then releases it; there is no global
snapshot cache or extra per-draw copy on cached-module hits. The original guest
base remains in Info and failure provenance. A 24-bit metadata byte-length cap
rejects oversized supplied programs before reading/copying them. Cache stays
27/9 because output semantics and serialization are unchanged; pins are fixed.

This protects against source reuse after capture. It does not make the initial
copy atomic, validate metadata/hash freshness, restore already-corrupt input,
protect separately parsed fetch code, or prove the latest failure's cause. No
unknown instruction is skipped or synthesized. Failure provenance now reports
supplied word count and owned-input status; unknown-format failures report the
actual remaining supplied words instead of an unavailable sentinel. No extra
raw-code dump or guest reread is added to the failure path.

Synthetic installed-decoder controls compare the same source overwrite: the
borrowed path fails, while owned input continues decoding its original end
instruction. Owned decoding also survives unmapping the original guarded source.
Actual TranslateProgram rejection controls verify owned input/provenance and
oversize rejection before capture. Native effect awaits an owner-run comparison;
no game was launched during this work.

### Shader CFG follow-up: defined SetPC arithmetic and explicit target validation

The `sotc-shader-cfg` overlay corrects signed C++ overflow in the local static
S_SETPC resolver. The old code negates INT32_MIN and can overflow signed
base-plus-offset addition; the original-source UBSan control reproduces the
failure. The adapted resolver uses modulo-2^32 unsigned add/subtract before the
existing alignment mask, preserving valid target arithmetic. It also rejects
out-of-range resolver indices/PC-map accesses before reading them. This is a
demonstrated source defect, not an established cause of the owner's CFG assertion.

Before creating blocks, CFG construction now rejects empty input, zero/non-dword/
overflowing instruction lengths, targets outside decoded code and targets inside
multiword instructions. Empty block graphs cannot enter the old next(empty)
path. Missing branch/fallthrough blocks and unresolved initial SetPC patterns
produce explicit fatal provenance with phase, shader, stage, target, instruction
count and available source/size fields. No end block is fabricated, instruction
skipped, target clamped or guest memory reread. Exit 23 remains fatal.

Sixty original SetPC cases cover positive/negative immediates, INT32_MIN and
wrapping bases under UBSan. Eight valid actual installed CFGs cover forward/
backward/conditional/empty-conditional/multiword and SetPC graphs; nine malformed
or unsupported controls stop explicitly. ARM64 build, all 20 renderer CPU checks
and 25 builder tests pass. Native shader behavior and the last assertion's cause
remain unverified. Shader cache is now **27/9**, dependency pins unchanged;
the exact preceding cohort migrates as `cfg_previous` without changing captures.
No game was launched. Windowed default and removed Game Mode integration remain.

### Latest owner result: shader CFG assertion, no large snapshots

The October 22 22:02:30 UTC capture (October 23 00:02:30 Madrid) ends after
118.513 s, exits 23/23, on a shader `CFG::LinkBlocks` assertion: the requested
destination does not match a known block. This does not identify the shader's
source or distinguish unsupported control flow, malformed/stale input and a
compiler bug. Do not fabricate the missing block or skip the dispatch. No Metal
OOM, SIGSEGV or metadata stop is recorded in this capture.

Balanced imports remain active. The cumulative memory recorder still reports
zero large-copy requests/completions with no journal drops/tracker errors.
Last presentation-sampled native allocation is 10.379 GiB, 656 ms old, versus
11.840 GiB recommended; this is not a failure-time measurement or sustained
stability proof. Dedicated upload sample is 560 MiB, 3.358 s old; remaining
small/medium uploads must not be mistaken for a revived 3 GiB snapshot. The run
records 1,119 presentations, 467 pipeline failures / 429 shader pairs (412 adjacent
PerVertex rejections), 805 SRT errors and an audio submit gap of 4.260 s with zero
queue-wait failures. Scene/output remain 1920x1080 / predominantly 2560x1440.

The startup image diagnostic is exercised: 512 of 1,158 loaded images are
recorded, including the renderer. No native fatal register context is expected
for this explicit assertion exit. It still uses BB_FULLSCREEN=1 and the previous
fullscreen-start settings; it does not validate the newly removed Game Mode
integration/windowed default. No game was launched during this analysis.

### Development resumed: bounded native register and binary provenance

The owner explicitly resumed development after native evidence that balanced
imports avoid the large staging snapshot. No stable/playable result is claimed.
The latest unmapped host-instruction SIGSEGV remains unexplained; do not skip
the fault or presume that memory-import or coroutine changes caused it.

`host_fault_diagnostics.h` now prints x0..x28, FP, LR, SP and PC from the supplied
ARM64 signal context before guest-specific lookup. It works even when guest
register reconstruction is unavailable. It uses bounded local formatting and
write calls, without dereferencing the registers, reading host/guest stacks,
unwinding, symbol lookup, allocation or recovery. Existing speculative/GPU fault
recovery still returns before the fatal path; real fatal exits remain 128+signal.
The existing concurrent/nested-report guard remains unchanged.

Outside the signal handler, startup records at most 512 loaded image names,
UUIDs, load addresses and text ranges, after graphics-provider initialization
when relevant. Later library loads may be absent. This is private evidence for
matching candidate addresses to actual binaries, not validated callers or objects.
`tools/analyze_host_fault.py` performs that range lookup offline, handles old/
truncated logs, and never reads process memory or guesses a stack. Old captures
cannot retroactively supply missing LR/register information.

Original CPU tests check bounded output with guarded FP/SP values, an actual
native jump into unmapped memory, unchanged fatal exit, real signal registers,
startup UUID/ranges, and offline boundary/truncation/unknown-address controls.
The launcher-budget fixture checks the direct executable path, windowed default
and unchanged budgets without GPU execution. The owner-requested Game Mode
integration has been removed, including its plist and app-bundle startup.
The full synthetic contract suite passes in JIT and interpreter, including the
four native fault tests and three offline-analysis tests. No game was launched
by the assistant. The owner's new capture exercises startup image inventory;
fatal native register output still awaits a matching fault.

### Latest owner result: large snapshot absent; host instruction fault

The October 22 21:54:46 UTC run confirms `pool layout=balanced`, three windows,
7,616 MiB imported aliases and 1,024 MiB overlap. It records zero large-copy
requests/completions, zero cross-window events and zero dedicated upload bytes
in the last staging sample. There are no dropped journal records/tracker errors.
This exercises the targeted allocation-avoidance mechanism in a native run.

The last presentation's native memory sample is **8.917 GiB**, approximately
one second old; presentation-observed peak is **8.928 GiB**. The preceding run's
failure-time allocation was 11.697 GiB: 2.780 GiB more, but the captures end at
different workload boundaries. Do not treat that difference as a controlled
benchmark or the presentation sample as memory at the instant of the new fault.
The staging sample is 22.474 s old; cumulative forced memory snapshots separately
confirm zero large-copy events through their latest observation.

This run stops after **82.902 s**, exits **139/139**, on `SIGSEGV` with an unmapped
host instruction address, outside translated code. The log supplies no translated
CPU context or original writer/branch source. Neither the new pool layout nor
the task-retirement change is established as its cause. No Metal OOM or shader
metadata stop is recorded before this different boundary; sustained OOM avoidance
and correct pixels remain unproven. There are **905 presentation results**,
scene/output 1920x1080 / predominantly 2560x1440, **306 pipeline failures / 281
shader pairs** (268 adjacent PerVertex rejections), **271 SRT errors**, and zero
finite-choice expansion messages. Audio has a maximum 2.084 s submit gap and
zero queue-wait failures. Next diagnosis should preserve the native instruction
fault and identify its thread/call source rather than infer corruption causality
from timing. No game was launched during this analysis.

### Memory follow-up: avoid the cross-window multi-GiB staging copy

The owner requested a change targeting actual graphics-memory pressure after
the 21:42:49 UTC capture still stopped on Metal OOM 8 after 51.599 s, exits 23/23,
at 11.697 GiB native allocation. There were 667 presentations, 152 pipeline
failures and 271 SRT errors. Lower counters do not establish shader improvements.
Scene/output remain 1920x1080 / predominantly 2560x1440. Audio's maximum gap is
36.048 ms with no queue-wait failures; no CPU fatal or shader-metadata stop occurs.

That capture's contiguous 2.880 GiB binding starts within the original import's
first 3 GiB region but extends beyond its 3.5 GiB buffer. It therefore requests
a complete 3 GiB staging snapshot. The new checked `sotc-pool-windows` overlay
uses balanced import geometry: prefix [0, 2.5 GiB), middle [2, 5.5 GiB), then
origins 5, 8, ... GiB with at most 3.5 GiB per window. Each neighboring pair
overlaps by 512 MiB. Direct bindings and BDA table entries both use recorded
window origins; no remaining index-times-3-GiB calculation is used. Imports
remain alive, no buffer is released early, and complete out-of-window/noncontiguous
ranges still use the original fallback. Mapping/range arithmetic rejects overflow.

The actual C++ planner replay of all **219 retained large-range observations** in
that capture agrees with the legacy decisions, avoids its one giant snapshot,
and introduces zero new cross-window fallbacks among those observations.
For the recorded 6,592 MiB pool, imported aliases increase by **512 MiB** (7,104
to 7,616 MiB). Avoiding the recorded 3 GiB snapshot therefore gives **2.5 GiB of
potential allocation-budget reduction before other allocation/reuse changes**,
not measured native savings. Do not add repeated copy events as independent
allocations. Journal limits mean this is not a proof for every binding or scene.
An independent negative control demonstrates that some long prefix ranges can
newly require a full fallback; the balanced layout is not universally better.

`BB_POOL_LAYOUT=legacy` restores exact old origins and sizes for comparison.
`BB_POOL_SPLIT=0` preserves the existing single-import opt-out. The startup log
prints actual layout, import count/bytes and overlap; the supervisor records both
settings. Use the guarded native launcher for any owner-run comparison. Shader
cache remains 26/9 and dependency pins remain fixed. Source migration is exact
`pool_previous`, preserving prior captures/build cohorts. The planner/range/BDA
CPU tests pass with ASan/UBSan. The ARM64 build, all 19 renderer CPU checks,
25 builder tests and dependency-inventory check pass.
Native residency accounting, alias synchronization, pixels, performance and OOM
avoidance remain unverified. No game was launched for this preparation.

### Owner-authorized follow-up: ordered CPU coroutine-frame retirement

After pausing general development, the owner requested an attempt to apply the
reference project's memory findings. The immediate nested-task leak correction
was already installed locally. Its later lifetime warning requires a different
consumer boundary, not another copy of the same immediate `destroy()` calls.

The checked `sotc-task-retirement` overlay now retires all five completed nested
tasks, the CE owner and top-level submissions through `Rasterizer::RunInOrder`.
That existing API copies the frame address into its ordered packet; the recording
thread destroys it after consuming all earlier packets. With pipelining disabled,
the API drains preceding packets before the synchronous callback. With no
renderer, completed tasks are destroyed immediately. Suspended tasks are never
retired. This preserves frame parameters, including submitted-copy ownership,
through queued CPU consumers; it does not extend locals already destroyed at
coroutine return, preserve arbitrary guest storage, wait for actual GPU completion
or change Metal resource retirement. Existing ring capacity provides backpressure;
this adds no separate unbounded retirement queue.

The independent CPU control exercises 300 delayed consumers borrowing storage
owned by a completed coroutine parameter. Immediate reclamation invalidates all
300; ordered reclamation preserves them and releases each frame exactly once.
Additional controls cover disabled/null pipelines and repeated draining, under
AddressSanitizer/UndefinedBehaviorSanitizer. The ARM64 library build and all normal
renderer CPU regressions pass, alongside the preceding five-site suspended/
completed regression and 25 builder tests. Exact prior sources migrate as
`task_previous`; shader cache stays 26/9 and dependency pins stay fixed. Native
memory savings, pixels, performance and OOM avoidance remain unverified. No game
was launched. Overall playable/native validation remains paused.

### Development paused: memory remains the immediate blocker

The owner has paused development because native runs still exhaust Metal's
memory budget. The latest 21:16 UTC comparison ends after 185.769 s on error 8,
exits 23/23, at 12.170 GiB. It records 1,095 presentations, 417 pipeline failures
and 805 SRT errors. Scene work remains 1920x1080, fullscreen output mostly
2560x1440. No finite-choice SRT expansion or actual Game Mode activation is
established. The [pause report](DEVELOPMENT_PAUSE.md) consolidates evidence,
remaining uncertainties and conditions for resuming. Do not automatically launch
further game/device comparisons. Historical checkpoints below remain evidence,
not a declaration that development is active or those failures are fixed.

### Historical Game Mode experiment (removed)

The owner subsequently requested removal of the Game Mode integration. The
launcher now runs the diagnostic directly, without the generated app bundle,
Game Mode plist or forced native fullscreen. Its default is windowed; the native
command explicitly uses BB_FULLSCREEN=0. Historical fullscreen captures retain
their original settings and output sizes. No Game Mode benefit was established.

### Prepared finite-choice SRT data reads; real-module PerVertex experiment

The checked `sotc-srt-finite` overlay expands a small, exhaustive set of scalar
ReadConst offsets into separate constant reads and a GPU-side selection. It runs
after resource discovery and before the existing protected SRT flattening pass.
Unlike selecting a Phi incoming edge on CPU, both runtime choices remain correct.
Only direct scalar-user-data pointer bases are admitted. Descriptor dependencies,
sampler LOD post-op dependencies and values used as table pointers are protected.
Unknown/lane-derived sources, cycles, induction recurrences and excessive sets
remain unsupported. Limits: eight choices, 16 levels/256 analysis nodes, offsets
below 4,096 dwords and at most 64 expansions per shader. No shader DMA is enabled.

Compiler cache is now **26/9**; pins and installed dependencies stay fixed. Exact
prior sources migrate as `finite_previous`. ARM64 build, normal renderer CPU
regressions, 25 builder tests and dependency-inventory checks pass. Fifteen
original finite-choice controls include nested Phi/ReadFirstLane/arithmetic,
uint32 wraparound, resource/pointer exclusions, depth limits and a separate
64-expansion cap. Runtime selections are checked against actual flattened data,
alongside the preceding 24-case suite. New native effects remain unmeasured.

For PerVertexKHR, private cached modules show 406 modules selecting vertices
0/1/2 and six selecting only vertex zero. The pinned compiler proposal converts
16 unambiguous failed vertex/fragment pairs through capture, replay and portable
fragment inputs; all **48 generated MSL sources pass Apple Metal 3.1 syntax**.
Six ambiguous/missing permutation pairs are skipped rather than guessed.
`tools/replay_pervertex_proposal.py` preserves private reports/sources under work.
Its standalone options and experimental binding slots do not reconstruct actual
runtime bindings, specialization, topology, ordering or ownership. This is not
an installed PerVertex fix or a native pipeline/pixel test; full renderer
capture/replay integration remains open. Default library pins are unchanged.

### Latest owner comparison: zero-filled compute-shader metadata

The October 22 **20:57 UTC** owner run, before the new finite-choice adaptation,
stops after **78.854 seconds**, child/supervisor exits **23/23**, on
`STOP_GPU_SHADER_METADATA` during an indirect compute dispatch. The source is
mapped, but the protected lookup finds no valid metadata and its entire captured
**65,536-byte** window contains zeros. This does not identify a writer, prove
freed storage, or distinguish wrong register/address, uninitialized content and
later overwrite. Preserve the explicit stop; do not skip the dispatch or treat
zeros as executable shader code. Exact owner addresses/code remain private.

Scene work remains 1920x1080 with **944 presentation results**. The last native
sample is **11.438 GiB**, not a failure-time OOM measurement. No Metal OOM,
CPU fatal signal or watchdog stop is recorded. There are **275 pipeline failures
/ 258 shader pairs** (248 have adjacent PerVertex rejection evidence) and **805
SRT errors**. Audio has zero queue-wait failures and a recorded maximum submit
gap of 127.707 ms. Different stopping boundaries and workload prevent interpreting
these counters as an effect of the prepared shader changes. The metadata failure
is separate from the unresolved PerVertex/SRT paths. No game/device run is
performed during this follow-up.

### Latest owner comparison: original-resolution scene, OOM persists

The October 22 **20:53 UTC** owner capture confirms **1920x1080 scene work**,
1280x720 presentation output, zero reduced decisions and zero created scene
proxies. Its environment retains preset 4 but sets `BB_UPSCALER=none`; actual
scene metadata confirms the reduction was disabled. The dedicated native launcher
also forces preset 0 to avoid that misleading inherited setting.

The run ends on Metal status 5/error 8 after **123.887 seconds**, with confirmed
child/supervisor exits **23/23**. Fresh native allocation is **11.966 GiB**, against
11.840 GiB recommended. Admission is active (limit/peak 8, five reserved/inflight
at failure). There are **1,325 presentation results**, not 3,976 frames. The failed
batch has 18,356 CPU notes, not proof of a particular failing GPU instruction.

Failure memory inventory records **359 live images / 954.19 MiB** and zero scene
proxies. The 3 GiB upload remains. Staging counters are 4.777 seconds old and GC
state 7.263 seconds old. The image event journal dropped **8,861 records**; full
event replay is incomplete even though the failure inventory supplies live totals.
Do not infer absent allocation/destruction events or a complete lifetime history.

There are **476 pipeline failures / 423 shader pairs**, 403 with adjacent
PerVertexKHR rejection evidence; the strict association leaves 73 unattributed.
SRT still records **271 failures**, with 52 bounded samples/eight shaders. The
uniform ReadFirstLane correction has no demonstrated reduction of these remaining
errors. Presentation counters include 1,136,113 missing-pipeline draw requests;
more/faster presentations do not establish complete rendering or correct pixels.
Audio has no queue-wait failures and a recorded maximum submit gap of 32.027 ms.
No CPU fatal signal or watchdog stop is recorded.

The longer run versus the 65.352-second reduced-resolution capture is an owner
observation, not an isolated performance/memory comparison: duration, progress,
shader/cache state and missing work differ. Original resolution is retained as
requested; OOM, PerVertex compatibility and remaining SRT problems stay open.
This checkpoint analyzes existing evidence only; no new game/device run occurs.

### Owner-selected original scene resolution

The owner now requests removing the 640x360 reduction. The graphics launcher
defaults to `BB_UPSCALE_PRESET=0` and `BB_UPSCALER=none`. The new
`prueba_grafica_nativa.command` also clears inherited render/output overrides
and forces that original-resolution mode, with DMA/asynchronous submission off
and JIT/statistics on. This means the game's original scene resolution, not the
monitor's physical resolution. Existing explicit low-resolution comparisons remain
available through the generic launcher. Prior 640x360 captures remain historical.
Shell syntax/configuration were checked without a game/device run; native memory,
FPS and correctness effects of this selection remain unmeasured.

### Prepared uniform SRT lane-read correction and reuse observations

The checked `sotc-srt-uniform` overlay resolves `ReadFirstLane` only when its
operand can already be expressed by the portable uniform scalar SRT program.
Constants, scalar user data, branch-independent Phi and previously flattened
constant reads can therefore retain their actual values. Lane-dependent values,
different incoming Phi programs, induction recurrences, cycles and excessive
expansion remain dynamic/rejected. No Phi edge, initial loop value or lane-zero
value is guessed, and direct guest-memory shader access remains disabled.

The strengthened 24-case installed-pass fixture first fails against the preceding
library and passes with the correction in both CPU-only synthetic DMA modes.
It includes original arithmetic induction and nested Phi/load controls inspired
by the bounded diagnostic shapes, without copying owner shader code. This is not
proof that all 271 owner errors can be resolved, nor native pixel validation.

Shader cache becomes **25/9**, invalidating previous compiler outputs through
the existing versioned migration; no manual deletion, dependency pin or library
upgrade is introduced. Exact previous sources migrate as `uniform_previous`.
The ARM64 rebuild, normal renderer CPU regression suite, 25 builder tests and
dependency-inventory checks pass. No new owner/game/device comparison was run.

Staging memory statistics now refresh every 256 large-buffer reuses when memory
tracing is enabled, in addition to allocation/frame-boundary updates. This improves
the stale diagnostic boundary without changing allocation sizes, completion,
ownership or release rules. The original/best-fit repeated-reuse CPU fixture
checks fresh publication and preserves held/pending/type guards. The 3 GiB upload
is not freed or shrunk: existing code documents late Metal descriptor references,
and original size-class controls reproduce growth from naive finer rounding.
OOM avoidance, title progress and native SRT effects remain unverified.

### Latest owner comparison: OOM and first native SRT provenance

The October 22 **20:40 UTC** owner capture ends after **65.352 seconds** with
confirmed child/supervisor exits **23/23**, Metal status 5/error 8 (insufficient
memory). Fresh native allocation is **11.573 GiB**, against 11.840 GiB recommended;
the recommendation is not a guaranteed safe allocation ceiling. Admission has
limit/peak 8 and eight reserved/inflight commands at failure. The failed batch
has 16 CPU notes including three dispatch candidates, not proof of a particular
failing shader or GPU execution order. Completion is observed after 12.368 ms;
commit returned after 0.009 ms. There is no CPU fatal signal or watchdog stop.

There are **680 presentation results**, not 2,041 frames (three lifecycle events
per presentation plus one failure). Final requested scene work remains 640x360.
Failure memory inventory has **255 live images / 683.43 MiB**, including **21
proxies / 19.60 MiB** after 559 proxy destructions. No image tracking overflow,
unmatched frees or journal drops are reported. A roughly 2.88 GiB fallback copy
still requires the rounded 3 GiB upload; another roughly 85.63 MiB request is
also recorded. Staging counters at failure are 19.398 seconds old; do not treat
their reusable-byte figures as permission to release current resources.

Offline grouping finds **199 pipeline failures / 188 distinct shader pairs**:
181 have adjacent PerVertexKHR conversion errors and 18 remain unattributed by
that strict rule. There are 184 conversion messages and **271 SRT errors**.
Lower counts than the 122-second run do not establish improvements. PerVertex
capture/replay remains an isolated compiler experiment, not an installed fix.

The checked SRT diagnostic is now exercised natively: **52 samples across eight
shader identifiers and six bounded IR shapes**, all at the copy-offset site.
Thirty-six samples include a Phi recurrence through IAdd, consistent with a loop
induction value; others include nested Phi or ReadConst/ReadFirstLane shapes.
Samples omit constants and can truncate traversal. They neither prove identical
expressions nor establish how to resolve the remaining offsets. Do not choose a
Phi edge, substitute zero or enable direct guest-memory access. Next construct
original synthetic recurrence/nested-load controls before changing semantics.

Audio has zero queue-wait failures, a maximum recorded submit gap of **29.597 ms**
and approximately 47,965 input frames/s/port over its windows. Its final window
does not cover the exact failure instant. Prior CPU corruption/audio failures
remain unresolved. This is an offline review of owner evidence: no new game run,
runtime/configuration change or dependency update is performed.

### Extended isolated PerVertex compiler experiment

`tools/check_pervertex_proposal.py` now exports six MSL sources and the captured
layout, strengthens the two-corner consumer to a real sum, checks 24 invalid
layout/binding configurations and exercises a CPU addressing oracle with 24
scalar reads and four range controls. The pinned isolated compiler converts the
four portable cases and rejects the default cases; four driver controls pass.
Renderer dependencies, cache 24/9 and exact-only compatibility policy stay fixed.

Apple MSL 2.4 syntax validation now passes for all six sources using the direct
installed Metal Toolchain 27A266a compiler (Apple metal 32023.921), without
diagnostics. Xcode's `xcrun` wrapper still reports missing tooling in this
environment; its report stays blocked, with the successful direct invocation
recorded separately as `metal-direct-validation.json` beside the sources. The driver
uses subprocess-local Xcode selection and returns 2 for missing tools, 1 for real
syntax failures, 0 only when all six compile. Generated sources/reports remain
private in `work/pervertex-research/validation-*`. Do not label compiler or CPU
ABI checks as GPU bounds/ordering validation.
No game/device execution or PerVertex renderer integration occurred here.

### Prepared pipeline/SRT failure classification

`tools/analyze_shader_failures.py` groups private logs by fragment/vertex pair
and conversion errors immediately preceding pipeline failures. The 20:08 UTC
capture has 431 failures across 386 distinct pairs: 375 events/375 pairs have
adjacent PerVertexKHR errors; 56 events/54 pairs remain unattributed. Categories
overlap by pair; a pair is not a complete pipeline key and adjacency is not
causal proof. All 805 SRT errors come from the portable copy-offset failure site.
The old log does not preserve the underlying expressions.
Of the 56 unattributed events, 55 reuse a fragment identifier with adjacent
PerVertex rejection evidence elsewhere. The remaining event has a PerVertex
message three lines earlier, interrupted by unrelated runtime output. Preserve
these as weaker associations, not another 56 established independent defects.

The new checked `sotc-srt-diagnostic` overlay adds shader/stage/pointer-or-copy
context and a bounded IR shape on offset failure, preserving original errors,
resolution, cache 24/9, dependencies and disabled direct guest-memory access.
At most eight records per pass / 128 per process are emitted; shapes omit values
and have fixed node/depth/input/text limits. They are diagnostic samples, not
semantically equivalent-expression keys. They appear only on compilation, not
when a cached pipeline bypasses the pass. No cache is automatically cleared.

ARM64 build and renderer CPU regression checks pass, including twelve actual
SRT/shader-info cases in both synthetic direct-access modes and the 128-record
cap. The latter mode only uses original CPU arrays, never a game/GPU. Offline
parser controls and 25 builder tests pass. Native provenance is pending; no
game/device run, shader support fix or OOM remedy is claimed. Next inspect new
private failure shapes and construct original synthetic expressions before
changing offset semantics or attempting per-vertex capture/replay.


### Latest owner comparison: Metal OOM despite active admission cap

The **2026-10-22 20:08 UTC** owner capture ends after **122.026 seconds** on
Metal insufficient-memory error **8**, with confirmed child/supervisor exits
**23/23**. The fresh native probe and completion callback agree on status 5/error
8, retained references, **13,388,939,264 bytes (12.469 GiB)** allocated against
**12,713,115,648 bytes (11.840 GiB)** recommended. No SIGSEGV/SIGBUS, decoder
assertion or GPU-progress watchdog is recorded. Their earlier causes remain open.

Admission is active with limit/peak **8**. At the failure report it counts
**4 reserved/inflight commands**, 97,316 acquisitions, 97,312 completions, zero
abandoned tickets and 3,243 waits / **1,053.599 ms** accumulated admission wait.
The failing command takes **4,222.347 ms** to report scheduled and **4,226.833 ms**
to report completed after callback registration; commit returned in **0.019 ms**.
These are host observations, not GPU execution time or proof of scheduling cause.
The batch contains **45,662 CPU notes**, not independent GPU instructions. The cap
does not bound a single command's resources or the total working set, and another
arbitrary cap reduction is not established as a remedy.

There are **1,190 `present_result` events** (also 1,190 `present_recorded` and
1,190 `queue_submitted` events). The 3,571 JSON rows include these three lifecycle
events plus the failure. Last presentation is at recorder time **114.1262 s**,
scene work **640x360**, output **1280x720**; the failure observation is **120.8699 s**.
These clocks do not share the supervisor's origin, and observations are not
correct-pixel or gameplay validation. **Count correction:** the preceding 19:53
capture had **1,115 presentations**, not 3,345; 3,345 was its three lifecycle rows
per presentation. Its sampled memory and SIGSEGV diagnosis are unchanged.

Fresh failure inventory/full image replay agree on **485 live images / 1,163.47
MiB**, after 6,623 allocations and 6,138 destructions. There are **23 live proxies /
22.18 MiB**, with **793 destroyed** and none pending. No journal drops, overflow
or unmatched frees are reported. The large upload is still **3 GiB**; a separate
**96 MiB** allocation is also recorded. Seven large copy completions are traffic,
not seven simultaneous 3 GiB buffers. No shared-binding/staging-tail event occurs.
The staging snapshot is **4.211 s** old and GC sample **7.635 s** old: neither grants
permission to release live storage. Image accounting is not full Metal accounting.

There are **376 PerVertexKHR messages**, **431 pipeline-creation failures**, 805
SRT-offset errors, 21,720 buffer clamps, 282 incomplete GPU-mapping warnings and
589 unimplemented LOD-stat command messages. Counts are messages, not distinct
shader/resource failures. Audio reports no queue-wait failures but a **15.012 s**
delivery gap; averaged input is about **41,262 frames/s/port** including it.
Its cause is unknown; do not call audio continuous. No additional guest resource
FileNotFound messages occur in this capture. `SOTC_CPU_FAULT_FIELDS` is unset,
and no fatal CPU context exercises its optional windows. The supervisor's new
settings header is exercised; absent admission override correctly uses default 8.

This analysis changes documentation only; no runtime/configuration/dependency
change or new game/GPU test is performed. Continue investigation of the retained
working set and invalid shader buffer bounds independently of the CPU-pointer
corruption, without discarding unresolved GPU contents or weakening lifetimes.


### Latest owner comparisons: bounded admission exercised; CPU pointer fault remains

The **2026-10-22 19:53 UTC** owner capture ends after **118.281 seconds** on
translated **SIGSEGV**, with confirmed child/supervisor exits **139**. It records
**1,115 presentation observations**, requested scene work at 640x360 and a sampled
native allocation peak of **11.646 GiB**. There is no recorded Metal OOM, decode
failure or GPU-progress watchdog in this capture. These observations do not prove
correct images, stable performance or resolution of previous memory failures.

The native admission limit is exercised: **limit/peak 8**, with the last logged
admission checkpoint reporting 2,048 waits and 643.091 ms cumulative admission
wait. These are checkpoint values, not final totals. The preceding **19:49 UTC**
capture also exercised the cap but still ended on **Metal OOM 8 after 121.100 s**,
confirmed exits 23/23 and fresh native allocation **12.404 GiB**. Thus the cap is
working as a bound, not an established OOM fix. Different run lengths, workloads
and stopping reasons prevent attributing presentation-count changes to it.

The newest fatal context identifies an invalid pointer load. Private offline
disassembly relates that pointer to an earlier field load from a readable object,
outside the first four words captured at register bases. The fault address agrees
with the saved pointer plus the instruction displacement. The originating field's
value was not captured and cannot be reconstructed after exit. No writer,
allocation lifetime, race or translation defect is established. Do not substitute
a null pointer, skip the load, patch game bytes or treat neighboring values as
validated object metadata. Exact instructions, addresses and analysis remain in
ignored private `work/analysis` output.

Full image-event replay at that CPU stop has **379 live images / 877.01 MiB**;
there is no synchronized failure-time Metal/image snapshot from the CPU handler.
The bounded buffer journal records a **3 GiB** allocation, no adjacent shared-copy
or staging-tail event, and no dropped records in the last summary. PerVertexKHR
appears **380** times. Audio windows contain **zero queue-wait failures**, but
one delivery gap reaches **28.474 seconds** on both ports; its cause is unknown.
The roughly 35,913 input frames/s average includes that gap and is not a hardware
underrun counter or proof of continuous playback.

**Prepared improvement:** `SOTC_CPU_FAULT_FIELDS` selects up to sixteen explicit
register-relative field windows, one to eight 64-bit words each, only at a fatal
translated stop. Syntax is `decimal-register:signed-byte-offset[:word-count]`,
comma-separated; offsets accept decimal or hex. Configuration is parsed before
guest threads or GPU initialization; malformed input stops with exit 24. Empty
or absent configuration performs no additional reads. Checked address addition
rejects underflow/overflow before the protected reader. Low/user guest-range and
GPU-tracking exclusions remain active, short/unreadable fields are explicit,
and no pointer is followed automatically. The original fatal exit is preserved.
The supervisor's private settings header now retains the selected fields and
native admission override without logging unrelated environment variables.

Original synthetic tests cover a field beyond the old snapshot, unaligned reads,
guarded page edges, both signed offset limits, register/count/configuration bounds,
invalid startup configuration and the actual JIT/interpreter terminating paths.
A UBSan reader run, the ARM64 diagnostic build and full JIT/interpreter contracts
pass. Selected candidate windows for this specific owner
capture are prepared privately, not hardcoded in public source or enabled by
default. This is better evidence collection, not a correction to the corrupted
pointer or a completed coroutine/queued-consumer ownership review. No game/GPU
test or production dependency/renderer-setting change is performed here.


### Prepared native admission limit after the latest Metal OOM

The **2026-10-22 18:22 UTC** owner comparison ends after **100.149 seconds**
with confirmed child/supervisor exits **23**, on Metal insufficient-memory error
**8**. It does not exercise the new decoder failure path: no decode assertion,
SIGBUS/SIGSEGV or GPU-progress watchdog is recorded. The new decoder guards are
not proven to resolve the previous slot-15 input; that input simply did not
recur. The native probe observes status 5/error 8, retained references and
**13,351,354,368 bytes (12.434 GiB)** allocated against a recommendation of
**12,713,115,648 bytes (11.840 GiB)**. This is shared M1 memory, not additional VRAM.

The failed batch has **45,774 CPU notes**, including 2,785 guest draw candidates,
13 guest dispatch candidates and native observations that overlap those counts.
Commit returns after 0.017 ms; scheduled/completed callbacks occur about
**8,858.529 / 8,881.566 ms** after registration. This establishes a long native
scheduling interval, not its cause or a specific failing shader. No retrospective
inflight high-water measurement exists; queue accumulation is a review hypothesis.
There are **1,074 presentation results** and scene work at 640x360; correct pixels
are unverified. PerVertexKHR appears **385** times; SRT/pipeline/clamp errors remain.

The fresh failure image inventory and full replay agree: **6,351 allocations /
5,876 destructions / 475 live / 1,161.42 MiB**, including **23 proxies / 22.18 MiB**
after **993** proxy destructions. Registered images are 450 / 1,134.34 MiB;
two non-proxy images / 4.91 MiB are queued for deletion. No journal drops or
unmatched events are reported. The last GC observation is **13.619 s** old;
the staging snapshot is **8.451 s** old. They are not current release permissions.
The early buffer journal records one **3 GiB** upload allocation and one 96 MiB
allocation, with five large completed copies. No shared-binding/tail event is
recorded. Audio has 19 windows per port, about 47,950 input frames/s and no queue
failures, with a 65.114 ms maximum gap; audible continuity is unverified.

The checked **sotc-metal-inflight-v1** overlay introduces application admission
control in the native `CommitCommandBuffer` wrapper, covering ordinary and split
chunks through that common method. Default **SOTC_METAL_INFLIGHT=8** permits at
most eight reserved-or-submitted unfinished native commands in this path. A
full budget waits for actual Metal completion, after existing error handling;
commit return, scheduled callback and Vulkan timeline advancement do not release
slots. No command is submitted early, reordered within a producer, skipped or
reported complete artificially. Resource retention, staging reuse, shader code,
resolution, pins/cache 24/9, timeout and watchdog policies remain unchanged.
The independent cancellation thread still exits while admission is blocked.

**Eight is a proposed bounded admission policy, not a measured M1 optimum.**
This can reduce accumulation of unfinished native batches but does not cap
unsubmitted recording objects, MoltenVK-internal work, a single huge batch or
the total shared working set. The 3 GiB reservation and unsupported rendering
remain. Admission can slow CPU submission and affect title progress/performance;
native validation is pending, not an established OOM fix. The control **0** removes
the admission cap; valid explicit values are 0..64, invalid values stop with
configuration exit 24. Existing guarded/supervised comparison rules and the
nested-coroutine queued-consumer review remain prerequisites for another test.

CPU evidence: an unlimited model retains 32 outstanding tickets; the bounded
model admits eight and blocks the ninth until completion. Duplicate/out-of-order
callbacks, uncommitted cleanup, concurrent producers and state lifetime pass.
The actual production wrapper runs against Foundation-only command doubles,
proving ARC callback capture, unchanged fatal Metal exit 23, and no successful
completion counted for an error. The real cooperative cancellation channel exits
21 with admission already blocked. No GPU/device/game is initialized. Private
`Metal inflight` logs report active/peak/waits/completed/abandoned counts; abandoned
unsubmitted work is not GPU progress. The exact preceding source cohort migrates
as `inflight_previous`, preserving digest and unknown-source rejection checks.
ARM64 build, seventeen renderer regressions, 25 builder tests, full
JIT/interpreter contracts and the 46/46 host-symbol audit pass. No new game
run has been started. Native savings/progress/performance remain unverified.


### Prepared decoder correction: bounded reads and failure provenance

The latest owner assertion has been narrowed against the selected source:
`OpcodeMap::OP_MAP_SMRD` is **1586**, so reported opcode **1601** is SMRD slot
**15**. That slot has no defined format in the current decoder tables. This
mapping is reproduced with original synthetic fields; it does not identify the
owner shader or establish that its bytes were valid, corrupt or stale.

The checked `sotc-shader-decode-v1` overlay protects `GcnCodeSlice::at`, `readu32`
and `readu64` against reading beyond the supplied end. The 64-bit read uses
`memcpy`, avoiding an unaligned/aliased u64 dereference. Main/copy translation
also checks empty input before the first word, instead of indexing `code[0]`.
Valid decode semantics and explicit assertion termination remain; the patch
does not implement slot 15, skip shaders or turn errors into successful draws.

Failures now print bounded private provenance: main/fetch/copy phase, parent
shader hash when supplied, hardware-stage enum, code origin and word offset,
requested/remaining words for truncation, and the already-read token/opcode/
encoding for unknown formats. Unknown counts are **-1**, not an assertion of an
empty shader. Failure reporting reads no additional guest bytes. Nested scopes
restore prior context and threads have independent context. These are CPU
compiler observations, not executed GPU commands. No code dump is enabled.

This correction is limited to the **supplied slice boundary**. Fetch parsing
still obtains its end from the existing synthetic maximum, not a measured guest
mapping; cache-hit `memcmp` and guest-pointer lifetime are not repaired here.
The overlay does not prove a live input span cannot be modified concurrently,
validate every mapped address, or resolve the nested-coroutine ownership lead.
Neither the latest assertion nor the 16 GB memory-pressure problem is declared
fixed. Preserve the existing pre-comparison ownership review and GPU guards.

The installed-decoder CPU fixture covers thirteen modes: valid scalar/control/
literal words, aligned-to-u32 64-bit reads, guarded reads/peeks, truncated 64-bit,
literal, SDWA and DPP encodings, illegal encoding, synthetic SMRD slot 15,
fetch provenance and empty main/copy input. It checks nested/thread-local scopes.
The old production `readu64` crosses the protected page and faults; adapted
malformed cases stop explicitly through the real exit-23 assertion handler.
The actual-library tests initialize no renderer or GPU and use no game input.
ARM64 rebuild, sixteen renderer regressions, 25 builder tests, full
JIT/interpreter contracts and the 46/46 host-symbol audit pass. Native owner
validation is pending.
Selected source/library pins, cache 24/9, resolution, resource ownership and
synchronization are unchanged. Exact preceding sources migrate through the
`decode_previous` cohort; unknown or mixed source modifications stay rejected.


### Earlier owner comparison: shader decoder assertion, not a recorded Metal OOM

The **2026-10-22 18:06 UTC** owner comparison (20:06 local time) ends after
**95.239 seconds**, with confirmed child/supervisor exits **23**, on a GPU-library
assertion in `GcnDecodeContext::updateInstructionMeta`, `frontend/decode.cpp`.
The instruction format has an undefined source or destination type: the decoder
prints opcode **1601**, with an empty symbolic name and encoding class
**0xc0000000 (SMRD)**, then stops. The encoding value identifies a family; it is
not a captured raw instruction word. Exit 23 is shared with other GPU failures
and must not be interpreted as OOM by itself. No Metal OOM callback, watchdog,
SIGBUS or SIGSEGV is recorded in this capture.

This is a new concrete shader-decoding boundary. It does not distinguish a
valid unsupported instruction from stale/corrupted code, a wrong shader range,
or decode desynchronization. No shader identifier/raw words are attached to the
assertion, and preceding asynchronous pipeline messages cannot identify the
faulting shader. Next inspect the opcode mapping/format tables, then prepare
bounded decoder provenance with synthetic invalid/truncated/unsupported inputs.
Preserve explicit failure; do not bypass the assertion or invent an instruction.
The existing nested-coroutine queued-consumer ownership review remains relevant
but is not a demonstrated cause.

There are **905 presentation results**, last recorder time **91.8833 s**, with
scene work requested at **640x360** and output **1280x720**. Peak sampled Metal
allocation is **11.447 GiB**; no failure-time native memory probe is triggered
by this assertion, so final pressure is unknown. The owner's M1 has 16 GB unified
memory; lower scene work does not eliminate original-size resources or the
large upload reservation. The early buffer journal records one **3 GiB** staging
allocation and one 96 MiB allocation, with six large completed copies reusing
storage. Cumulative copy traffic is not simultaneous capacity. No large sharing
or tail event is retained, but the bounded buffer journal has **3,457 dropped
records** at the last summary: absence cannot establish those cases never ran.
The last staging-pool snapshot is **40.102 s** old at the final presentation.

The image journal has no reported dropped image records or unmatched destroys.
Lifecycle replay records **5,326 allocations / 5,018 destructions / 308 live /
769.83 MiB**. Owner/role updates identify **19 live proxies / 19.25 MiB**.
The last periodic snapshot is 2.183 s older than the last image event: its
303 live images / 784.53 MiB and 777 destroyed / 18 live proxies are historical
snapshot values, not the final replay. These are application/VMA lifetimes,
not complete Metal/driver accounting or safe-eviction proof.

Audio has 18 windows per port, about **47,948 input frames/s**, maximum delivery
gap **38.151 ms**, and no queue-wait/backend failures. Audible continuity remains
unverified. PerVertexKHR appears **337** times; SRT/clamp and pipeline failures
persist, without recurrence of the integer-blit type mismatch. No new pixel
validation is available. Two intervening short owner runs, at 18:01 and 18:02
UTC, still ended on explicit Metal OOM after 14.161 and 9.868 s; the newest
assertion does not resolve that memory problem. Do not attribute the variation
to a fix or compare these runs as controlled performance measurements.

This analysis starts no new game/GPU run and changes no runtime, selected pin,
cache, memory ownership, graphics configuration or stopping guard.


### Preceding owner comparison: native probe confirms Metal OOM on the 16 GB M1

The **2026-10-22 17:57 UTC** owner comparison ends after **77.670 seconds**
with confirmed child/supervisor exit **23**, on Metal insufficient-memory error
**8**. It is not a duration timer, GPU-progress watchdog or recorded CPU fault.
The weak native observer is now exercised: it reports an available command with
status **5**, error **8**, retained references enabled and current device
allocation **12,458,835,968 bytes (11.603 GiB)** against a recommended working set
of **12,713,115,648 bytes (11.840 GiB)**. Callback history agrees: scheduled and
completed callbacks are present, with completion about 14.147 ms after commit
registration. The failed chunk's summary contains 19 guest compute candidates
and 19 native dispatch observations, not 38 independent dispatches and not proof
of a specific failing instruction. Private shader/resource identifiers stay local.

The owner clarifies that this M1 has **16 GB of unified memory** and that reduced
graphics are intended to relieve GPU pressure. CPU and GPU share physical memory;
Metal's allocation figure is not extra dedicated VRAM to add to the process's
full memory footprint. The comparison uses preset 4 and records requested scene
work at **640x360**, but output remains **1280x720**, with original-resolution
resources still present. The failure inventory includes 1920x1080 images and a
large fallback upload; reducing scene resolution alone does not shrink them all.
Prioritize lowering the total retained working set without discarding unresolved
GPU contents, prematurely releasing resources or weakening synchronization.

There are **859 presentation results**, the last at recorder time **76.5835 s**;
these are API observations, not correct-pixel or playable-game validation.
The error snapshot is at recorder time 76.8828 s; recorder and supervisor clocks
have different origins. PerVertexKHR still appears **206** times. Integer-blit
type errors do not recur; other shader/SRT and clamp failures remain.

The fresh failure inventory reports **3,707 image allocations / 3,457 destructions
/ 250 live images / 676.18 MiB**, with **255 scene proxies destroyed / 21 live /
19.60 MiB** and no queued proxies. Registered images are 229 / 656.58 MiB. No
tracking overflow, unmatched frees, observation errors or dropped records are
reported. These application/VMA figures do not account for every Metal/driver
allocation or prove no resource remains retained after a VMA destruction event.

One large copy requests **3,098,044,176 bytes**, selecting one **3 GiB** buffer.
No large shared-binding or staging-tail event occurs; the next observed new
large allocation is 96 MiB. Native savings from those prepared cases remain
unverified. The final staging-pool snapshot is **29.452 seconds old**, so its
reusable-byte estimate must not be treated as current or as permission to free
live storage. The failure-time native allocation and image snapshot are fresh.

Audio has 15 periodic windows per port, averaging about **47,845 input frames/s**
with no queue-wait failures or backend errors; maximum delivery gap **72.046 ms**.
This does not validate audible continuity. Earlier SIGBUS corruption and queue
failures remain unresolved despite not recurring in this capture.

The native probe is validated as failure-reporting evidence, not an OOM fix.
The build contains the tessellation-instance adaptation, but no instance-specific
pixel/readback result proves that path correct. The nested-coroutine queued-
consumer ownership review above remains a prerequisite for another comparison.
No new game/GPU run or dependency/configuration change is performed for this
analysis. Keep all guards and selected pins/cache 24/9.

### Five-source refresh: tessellation adaptation and coroutine review priority

The owner's requested SotC/Pink/bloodborne-mac/runtime/recomp source refresh is
recorded in [Upstream review](UPSTREAM_REVIEW.md#owner-requested-five-source-refresh-and-selected-adaptation)
and [Dependency versions](DEPENDENCIES.md). All five are permanent references;
the inventory now distinguishes selected pins from newer reviewed heads.
Exact public Git history/diffs are available despite earlier web-page failures.
The selected runtime/CPU/reference remain b2ff9cd0/b9652d75/411fdd86.

Runtime `dcaf50060880` is selectively integrated through the checked
`sotc-metal-tess-instances-v1` overlay: multi-instance tessellated draws use the
existing single-instance path and deferred patch draws preserve their base
instance. `BB_TESS_INSTANCES=0` restores the old multi-instance skip for a future
supervised comparison. No launcher setting or shader cache changes. The CPU
fixture reproduces the old skip and checks actual routing/pending/replay with
negative controls; kernel encoding is replaced by a CPU leaf. ARM64 rebuild,
fifteen renderer regressions, 25 builder tests and full JIT/interpreter
contracts pass. No game/GPU run or correct-pixel claim follows.

**New pre-comparison priority:** reference `0127e36` reports corruption with
completed nested-task destruction, including after recording-thread deferral,
and disables that optimization by default. Our earlier `c9be8142` adaptation
still frees completed child frames. Its synthetic leak/count check proves
completion, not that asynchronous consumers have stopped borrowing frame data.
Review all five local completion sites against draw/preparation ownership and
add deferred-consumer controls before proposing another native comparison.
The reference has different queue machinery; this warning does not prove our
SIGBUS, OOM or GPU stall cause, and its Windows allocator/rollback is not copied.

The reference's speculative hot-readback corruption fix has no matching local
hot-region gatherer. Runtime `ab31a4c` deliberately discards unresolved GPU image
contents over a critical mark; keep our content-preserving guard. Pink's
pending-write, readback lifetime and optional NaN/MAD changes are tracked as
separate candidates, not enabled settings. Preserve scene 640x360, DMA/async off,
cache 24/9, upload guards, deferred retirement and watchdogs.

### Prepared improvement: observe native state when reporting a stalled batch

The new `sotc-metal-probe-v1` overlay registers a weak observer for each traced
native command buffer. A stall or Metal failure report now prints `Metal native`
with fresh command-buffer status/error, retained-reference policy and the
device's current allocation/recommendation, alongside the existing callback
history. This targets the gap in the 17:29 UTC capture: `status=0` in the old
callback record was an unobserved default, not proof Metal reported NotEnqueued.
The new property observation is separate and does not overwrite that history.

The observer promotes its weak reference only during the query. A reclaimed
command, an unregistered probe, an unavailable device and a C++ provider failure
are explicit; unavailable memory must not be interpreted as zero pressure.
Properties are sampled sequentially outside the journal mutex, not as an atomic
GPU snapshot. No command-buffer/resource ownership, GPU waits, semaphore values,
submission ordering, watchdog thresholds or staging sizes are changed. Direct
property status does not identify an executing shader or establish why a queue
is delayed. The bounded 1,024-chunk journal stores weak observers and does not
keep old command buffers alive. Normal recording registers observers without
querying native properties. The existing `SOTC_GPU_SUBMIT_TRACE=1` controls this
addition; no extra experiment setting is introduced.

CPU journal fixtures verify native-vs-callback separation, fresh/absent/failed
providers, safe reentry outside the lock and recording-generation isolation.
An Objective-C ARC fixture runs the actual weak bridge against Foundation-only
objects: status/error/memory change, missing device, expired/nil command and
observer copies that retain neither the command nor its resources. No Metal
framework, device or GPU is loaded by that fixture. The ARM64 rebuild, fourteen
renderer regressions, 25 builder tests and full JIT/interpreter contracts pass;
native validation is pending.
No game/GPU run has been started and no stall fix is claimed. Pins/cache 24/9
and the retained-reference/staging/proxy protections remain.

### Latest owner comparison: GPU progress stop with confirmed child exit

The **2026-10-22 17:29 UTC** owner comparison ends after **116.824 seconds**
on the ten-second GPU-progress watchdog, with confirmed child exit **21** and
supervisor exit **31**. Cooperative cancellation is acknowledged 1.326 ms after
the request. No duration limit, Metal insufficient-memory error, CPU fault or
audio queue failure is recorded. This capture follows the retained-reference
ARM64 build; the launcher's native Metal path is active. It does not establish
that the lifetime change caused the different stop reason or resolved prior
OOM/corruption. No additional game/GPU test was started during this analysis.

There are **1,052 presentation results**, the last at visual-recorder time
91.819 seconds, with scene decisions at 640x360. Metadata is not pixel validation;
the capture option remains off. The maximum native allocation sampled alongside
presentations is **11.485 GiB** (last **11.477 GiB**); no forced final native
allocation snapshot exists at the watchdog, so final pressure is unknown.

The awaited submission retains a full summary of **26,020 observed CPU notes**
with a 96-note tail: 1,567 guest draw candidates and 13 guest dispatch candidates,
plus native draw/dispatch/copy/blit/state observations. Guest and native notes
may describe the same work; these are not GPU execution totals. The queue call
returned success after 0.093 ms, and Metal commit returned after 0.028 ms. At the
report, neither scheduled nor completed callback was observed for that chunk.
The preceding submission has both callbacks and successful Metal completion.
No journal callback/note loss is reported. These observations locate the missing
completion boundary, not an executing instruction or proven failing shader.
Do not disable the watchdog, simulate completion or extend the timeout as a fix.

Complete image replay records **5,651 allocations / 5,203 destructions / 448 live
images / 1,111.45 MiB**, with no unmatched events. Proxy cleanup records **817
destroyed / 23 live / 22.18 MiB**, including two queued proxies / 4.91 MiB. The
registered inventory is 423 images / 1,084.37 MiB. The last periodic memory
summary precedes the last image event by 20.623 seconds and must not be called
the final image inventory. No dropped image/buffer/summary records are reported
in that last summary; actual retained Metal objects may outlive VMA destruction
observations, so this replay is not complete driver-memory accounting.

One fallback requests **3,218,630,416 bytes** and allocates **3 GiB**, leaving
only **2.47 MiB** of padding. No large `staging_tail_selected` or
`copy_binding_shared` event is recorded; all observed large staging offsets are
zero. Small requests below the 64 MiB observation threshold remain outside this
event evidence. The next observed large new allocation is 96 MiB for an 85.63
MiB request, so it cannot fit that large buffer's padding. Native savings from
the prepared staging-tail/sharing cases are not validated by this run. The last
presentation's staging snapshot is already 44.503 seconds old.

Audio records 22 windows per port at approximately **47,975 input frames/s**,
maximum delivery gap **31.533 ms**, with zero queue-wait failures. This measures
input delivery, not uninterrupted playback or audiovisual alignment. Shader
rejections persist: **354 PerVertexKHR messages**, 805 SRT-offset errors and 270
pipeline-creation failures in the last asynchronous presentation snapshot. The
two float-output/integer-attachment errors do not recur. Next review should
separate queue scheduling/dependencies from actual shader execution, preserving
the complete batch summary and tracking driver retention versus application
image accounting. OOM, CPU metadata corruption and prior audio failure remain
unresolved even though none is the recorded stop in this comparison.

### Prepared improvement: retain native Metal batch resources

The `sotc-metal-retention-v1` overlay changes both native Metal command-buffer
creation sites (initial `OwnBuffer` and `CommitChunk` rollover) from
`commandBufferWithUnretainedReferences` to `commandBuffer`. Explicitly encoded
resources therefore use Metal's retained-reference contract independently of
the following Vulkan timeline signal. The combined launcher already selects this
native command path with `BB_METAL_FRAME=1`; this does not replace MoltenVK or
change its installed implementation. The lifetime pattern is informed by
[MoltenVK #2860](https://github.com/KhronosGroup/MoltenVK/pull/2860), not a wholesale
driver patch or a proven cause of an earlier SotC fault.

Existing past-next Vulkan retirement, scratch-completion handlers, residency,
indirect/device-address resource declarations, error completion, watchdog and
staging guards remain. Retaining directly encoded objects does not prove that
every indirectly addressed allocation is declared or kept correctly. It may
hold resources longer; no memory reduction, OOM avoidance or stall/corruption
fix is claimed. Runtime/CPU/reference revisions and cache 24/9 are unchanged.

The new `tools/check_metal_retention.py` extracts production `OwnBuffer`,
`CommitOwned`, `CommitChunk` and `ReturnScratchWhenDone` into an Objective-C ARC
fixture linked only to Foundation. CPU queue doubles model the documented Metal
retention contract. The original code records six premature-release observations
across three encoded resources; the adapted code records zero. Independent
batch completion, wrapper deletion, reset without submission and scratch-pool
completion controls pass. This tests production API selection/ownership with
CPU doubles, not Apple's GPU execution or the driver's actual timeline window.
The ARM64 renderer rebuild and symbol/source audit, fourteen renderer regressions,
25 builder tests (including exact predecessor checkout acceptance and rejection
of a foreign tracked file) and full JIT/interpreter contracts pass. The built
library records the new overlay in its build metadata. Native validation remains
pending at that build checkpoint; the subsequent owner comparison above does
not establish correct native resource lifetime or recovery. Resource retention can raise
in-flight memory pressure, so the next independently authorized comparison must
inspect both completion progress and memory before calling this a stability fix.

### Prepared improvement: append uploads into unused staging tails

The checked `sotc-staging-tail-v1` overlay avoids creating another dedicated
upload buffer when an existing upload has enough unused space in the same
recording tick. It first preserves the existing best-fit selection of fully
completed, non-held buffers. Only if no such buffer fits may an explicitly
aligned, non-deferred `HostUncached` request append beyond the previous used
range. The previous allocation must also have allowed tail use and belong to
the exact current tick. Each slice has its own offset and exact requested size;
its start satisfies the caller alignment, noncoherent atom size and a 64 KiB
minimum. Invalid alignments, overflow, full tails, earlier ticks, held/deferred
storage, downloads and device-local requests retain the ordinary allocation path.

This is fresh storage within a retained allocation, not reuse of occupied bytes
or sharing of a snapshot. GPU tick completion is still required to reset the
prefix. Returned mapped pointers and flush offsets follow the selected slice.
The existing synchronous and deferred upload consumers use these offsets;
`RunGuestCopy` flushes relative to the actual buffer mapping. Host-copy waits,
barriers, descriptor ranges and the separate adjacent-binding sharing guard are
unchanged. No buffer is freed, shrunk or remapped, and large-buffer trimming
remains unchanged. Runtime/CPU/reference pins, cache 24/9, requested scene and
DMA/async settings are unchanged. Large appended requests add the private
`staging_tail_selected` event alongside `staging_selected`, with buffer, offset,
length and tick. Both remain subject to the existing 64 MiB observation threshold.

The actual-source original/adapted CPU fixture retains **3,296 MiB before versus
3,072 MiB after** for three concurrent synthetic uploads into a retained large
buffer. A separate boundary case avoids an additional 96 MiB allocation by
using padding in a 3.5 GiB buffer. Multi-GiB capacities are symbolic; small exact
payloads verify nonoverlap, mapped pointers and the production flush method.
Negative controls cover ownership, pending ticks, type, invalid/stronger
alignment, noncoherent atoms, full tails and integer overflow. Completed-buffer
reuse resets offsets only after real simulated completion, without destruction.

The rejected alternative was simply rounding allocations more tightly. The
growth control shows a 64 MiB rounding policy retaining **6,336 MiB** for two
successive growing requests where the existing class reuses **3,584 MiB**.
The overlay therefore leaves allocation rounding intact. The ARM64 rebuild and
symbol audit, thirteen renderer source regressions, 24 builder tests and full
JIT/interpreter contracts pass.
Native tail use, savings, pixel correctness and OOM avoidance are still unverified;
the single 3 GiB fallback itself remains. This is a targeted allocation reduction,
not a claimed fix for the latest OOM, PerVertexKHR failures or earlier corruption.

The [pre-comparison review](DEPENDENCY_WATCH.md#pre-comparison-staging-review-2026-10-22-1657-utc)
records access limits and a newly observed abbreviated app runtime gitlink whose
diff is unavailable. It is not integrated. No tool-driven game/GPU run is started.
The owner's next comparison uses the same combined launcher, once, with preset 4,
DMA/async off and metadata enabled. Inspect nonzero staging offsets/tail events,
new dedicated allocations, native pressure and the final stop reason.

### Latest comparison: scene rendering resumes; Metal OOM remains

The **2026-10-22 16:32 UTC** owner run ends after **104.474 seconds** on Metal
error **8 / Insufficient Memory**, with confirmed child/supervisor exit **23**.
It advances substantially beyond the immediately preceding logo-only owner
report: **983 presentation results**, with reduced-scene decisions first observed
at recorder time 55.792 seconds and the last presentation at 97.623 seconds.
There are 63,879 reduced-scene decisions by the failure report. These counters
establish renderer progress, not correct pixels or a new owner-confirmed gameplay
milestone. The largest presentation gap is 5.736 seconds. Requested scene remains
640x360, DMA/async off, metadata on, pixel capture off and no duration limit.

The complete buffer journal records **one large fallback copy**, requesting
2.885 GiB and allocating **3 GiB**, with 117.47 MiB of rounding padding and a
0.721-second CPU copy. There is **no large `copy_binding_shared` event** and no
dropped buffer record: this run does not exercise the duplicate-copy sequence
that motivated the prepared correction below. It neither validates its native
savings nor identifies that correction as the cause of run-to-run variation.

Native allocation is **11.588 GiB at failure**, with a sampled peak of 11.596
GiB, below the device recommendation of 11.840 GiB. The recommendation is not a
hard guarantee of allocation success, and these values are not physical RAM
consumption. Complete image-event replay matches the forced failure ledger:
**5,550 allocations / 5,253 destructions / 297 live images / 780.69 MiB**.
Of these, 279 registered images retain 762.37 MiB; **18 scene proxies retain
18.31 MiB after 938 actual proxy destructions**. There are no dropped records,
tracker errors or pending proxy deletions. This is not evidence of an unbounded
proxy leak. The last staging inventory is already **41.947 seconds old** and
cannot authorize releasing its reported reusable bytes; the collector sample
is 440.292 ms old. Guest-written-image retention remains a separate correctness
constraint, not permission to discard content under pressure.

The failing batch retains all twelve observed CPU notes, including two guest
dispatch candidates and two native dispatch notes, with no draw/blit/copy note.
Metal commit returns at 0.017 ms, scheduled is observed at 3.350 ms and error
completion at 32.022 ms. These are host observations and CPU candidates, not
identification of an exact failing GPU instruction or a shader-specific cause.
There are **252 PerVertexKHR conversion errors**, 280 pipeline-creation failures
in the asynchronous counter snapshot and continuing SRT/Phi errors. No CPU fault,
GPU watchdog or audio queue failure is recorded. Thirty-eight audio windows
(nineteen per port) deliver about 47,873-47,874 input frames/s, maximum gap
76.525 ms; audible continuity remains unverified.

For comparison, the **16:26 UTC** run ends after **15.167 seconds**, also on
Metal OOM 8 with confirmed exit 23, and the owner reports only logos. It records
87 presentation results, no reduced-scene decisions, and one 3.027 GiB fallback
request rounded to **3.5 GiB** (484.47 MiB padding). Native allocation is 11.660
GiB at failure, sampled peak 11.670 GiB. Complete image replay matches 124
registered images / 496.39 MiB with no scene proxies or tracking losses. Its
four audio windows deliver about 43,823-43,882 input frames/s, with a 345.414 ms
maximum gap and no queue failure. No large shared-binding event is recorded.

Source comparison confirms that allocation rounding is unchanged from the pinned
runtime: requests around this boundary round in 512 MiB steps. The effective
guest range differs between captures; the records do not establish why. The
new run recovers scene-renderer progress without any intervening code or setting
change by this review, so the earlier logo-only outcome does not establish a
deterministic regression from sharing. A controlled causal comparison is still
missing. Next isolate the single multi-GiB fallback and the retained-memory
baseline using original synthetic boundary/growth sequences. Any smaller
allocation class must retain the complete requested range and account for later
growth creating additional retained buffers; do not restore the withdrawn range
truncation experiment or weaken held/completion guards. This review changes
documentation only and launches no game. Private replay stays with the captures.

### Prepared correction: share adjacent identical read-only fallback copies

The checked `sotc-binding-copy-sharing-v1` overlay addresses the duplicate upload
identified in the 15:58 UTC capture below. `BindBuffers` owns a local, fixed-size
scope for one shader-stage binding preparation. Consecutive read-only,
unformatted slots with the same **effective clamped address and length** can use
one already-copied staging snapshot. Different original declared lengths do not
prevent sharing when both clamp to exactly the same effective range.

Every slot still executes `ObtainBuffer` and its mapping/page-table processing.
Sharing is considered only after the ordinary unified-memory lookup falls back
to a copy and only for a complete single pool mapping. The runtime mapping-table
generation must be even and unchanged, with the same physical mapping, mapping
end and scheduler tick. The first copy is not reusable if these observations
change while it is made or an unmap is pending. Writes, formatted buffers,
different ranges, nonconsecutive slots, intervening special/direct bindings and
new stage/draw/dispatch preparations obtain fresh results. Exceptions invalidate
the candidate. This does not cache snapshots between GPU operations or replace
CPU/GPU synchronization for guest writes.

The staging pool keeps ownership and its existing held/completion checks.
Sharing neither frees nor overwrites an occupied buffer. Descriptor offsets,
binding ranges, write invalidation and access/barrier bookkeeping still run for
each slot. Large shared bindings emit the bounded private `copy_binding_shared`
event, distinct from a new allocation, CPU copy or completion-based pool reuse.
The allocation rounding, split imports, scene configuration and cache 24/9 stay
unchanged; runtime, CPU and reference pins are unchanged.

`tools/check_binding_copy.py` compiles the actual unified `ObtainBuffer` route,
`UnifiedCopy` and staging allocation methods against original CPU doubles. The
synthetic sequence reproduces **6 GiB retained / three copies before**, versus
**3 GiB / two copies after**, including the earlier completed-buffer reuse. These
are symbolic capacities with small exact-byte payload checks, not native memory
measurements. Guard cases cover remaps (including changes during a copy), odd
mapping generations, sparse/short mappings, writes, formatting, slot gaps,
changed ranges/ticks, new dispatch contents, exceptions and real pool-method
retention until completion. The ARM64 build/symbol audit, twelve renderer source
regressions, 24 builder tests and full JIT/interpreter contracts pass. Builder
fixtures cover clean application, all three earlier cohorts, idempotence and
rejection of incomplete/unknown sources. No game/GPU run is started. Native
confirmation of sharing and memory savings remains pending: the two newer runs
above each record only one large fallback copy and no large shared-binding event.
OOM persists in both.

### Early native OOM: two 3 GiB upload buffers and duplicate read-only bindings

The **2026-10-22 15:58 UTC** owner comparison closes after **11.704 seconds**
with confirmed **child/supervisor exit 23**, following an actual Metal command-
buffer error **8 / Insufficient Memory**. There is no watchdog, CPU-fault or
cooperative-stop failure in this capture. Settings remain 640x360 requested,
DMA/async off, JIT/statistics and metadata on, pixel capture off, no duration
limit. This early failure does not reproduce or revoke the owner's first-
gameplay-image observation from the preceding run.

The forced native failure snapshot reports **13.889 GiB allocated**, versus a
device recommendation of **11.840 GiB**. This is native allocation accounting,
not physical RAM consumption or a hard memory limit. The uncapped buffer journal
records three large cross-window copies and **two distinct 3 GiB upload buffers**.
The first request creates one buffer; the next submission reuses it. A second
binding in that same submission creates the other buffer. The latter two
requests have the **same address, clamped length, shader and scheduler tick**,
but different slots; both are read-only and unformatted. Their original declared
lengths differ, while their effective copied ranges are identical. The failure
event's staging sample is 773.754 ms old and reports **6 GiB dedicated upload**.
Three completed CPU copies are traffic, not three simultaneous allocations.

Source inspection confirms that each ordinary binding independently calls
`ObtainBuffer`, whose cross-window fallback copies into a staging allocation.
The staging pool correctly excludes allocations that are held or whose GPU tick
has not completed. Do not weaken those guards to reuse the in-flight copy. The
next specific candidate is sharing an exact read-only fallback snapshot within
one binding preparation, with original/adapted synthetic tests for writes,
aliasing, different ranges, intervening operations and ownership/completion.
The observed duplicate is not yet proof that arbitrary snapshot sharing is safe,
nor that eliminating it resolves all native OOM/stalls.

The complete image history exactly matches the forced ledger: **103 allocations,
17 destructions, 86 registered images / 347.54 MiB**, with **no scene proxies**
created yet, no dropped records and no tracker errors. This is an earlier memory
boundary than the previous long-run registered-image growth. There are twenty
presentation results, ending at recorder time 8.400 seconds; the largest gap is
5.692 seconds. No reduced-scene decision or correct-pixel evidence is recorded.
Two complete audio windows deliver approximately 47,520–47,533 input frames/s,
with a 29.515 ms maximum gap and no queue/output failure. PerVertexKHR messages
do not occur before this early stop; that is not validation of those paths.

The new submission extension is exercised natively on the Metal-error path:
the complete thirty-note failing batch includes four guest dispatch candidates,
five native dispatch notes and one buffer-to-image copy, with no draw/blit notes.
Commit returns at 1.177 ms, the scheduled callback is observed at 16.188 ms and
the error-completion callback at 23.046 ms, relative to commit registration.
These are host observations, not precise GPU instruction timing. The journal
still does not identify the exact failing command. Native large-tail retention
and the new watchdog timing report remain untested by this early failure.

Keep the earlier OOM, GPU stall, CPU corruption and audio failure separate.
This analysis changes documentation only; no dependency, code, setting or game
input is changed and no additional game run is started. Private offline replay
and calculations remain with the ignored capture. The previously requested
diagnostic commits are now present in the local history.

### Prepared full-batch submission summaries and host timing

The **2026-10-22 15:27 UTC** comparison below retains only the last 96 of 26,764
CPU notes for the awaited batch. The submission recorder now also keeps fixed-
size counters for all observed notes, separating guest draw/dispatch candidates
from native API draw, dispatch, copy, blit, clear, barrier and state notes. The
first and last guest draw/dispatch candidates survive tail eviction, with their
own recording generation and ordinal. Combining command buffers merges these
summaries without recounting their retained tails. Empty/missing recordings and
handle reuse remain explicit. Missing source recordings make these counts
incomplete. These are metadata counts: guest and native notes can describe the
same operation, and recording is not proof of GPU execution. The candidates
describe a logical batch/open recording, not a precise Metal chunk boundary.

Monotonic host timestamps now distinguish submission registration/return and
Metal commit registration/return, scheduled callback and completion callback.
Reports show observation ages and elapsed times, with **-1 for an observation
not seen**. A callback may precede the return from `commit`; its first observed
time is retained. These include host scheduling and recorder overhead, not
precise GPU start/end times. Existing watchdog and native-error reports print
the new metadata without adding GPU markers, resource reads or new submissions.

The CPU fixture covers a 26,775-note multi-buffer batch whose early candidates
are absent from its tail, missing/empty buffers, reused handles, callbacks before
commit return, duplicate callbacks, concurrency and cooperative cancellation.
Summary storage is bounded below 1 KiB per recording/submission, in addition to
the existing 96-note tail. The ARM64 rebuild and symbol audit, eleven renderer
regressions, 24 builder tests, full JIT/interpreter contracts and the submission
fixtures pass. The submission fixtures are now included in `--contracts-test`.
Native validation of this extension remains pending. This improves evidence for
the unresolved stall; it does not fix it or the earlier OOM/corruption. Keep all
pins, cache 24/9, memory ownership, scene settings and stopping guards. No game or
GPU comparison is started. Confirm the previous child/window has closed and
refresh the tracked public upstream state before any next native comparison.

### First gameplay image confirmed; native comparison stops on GPU progress

The owner reports seeing the **first image of the gameplay section** in the
**2026-10-22 15:27 UTC** comparison on Apple M1. This refines the earlier gameplay
milestone; it does not establish responsive or stable play, correct pixels or
working saves. Pixel capture remains off. The native log no longer contains the
two `float4`/R32Uint/RG32Uint blit pipeline errors, nor an integer-library compile
or unsupported-integer-copy error. This is useful native absence-of-error evidence
for the prepared overlay, not per-format GPU readback or proof that every signed,
mip, scaling and edge case is correct.

The renderer reports a submission without completion for ten seconds. The
supervisor requests cooperative cancellation and observes the child's notice
2.125 ms later, but **does not confirm child exit**. Its footer ends after
**136.225 seconds**, with **child status unknown and supervisor exit 1**. This
is a GPU-progress stop followed by failed stop confirmation, not Metal OOM,
SIGBUS or a duration timer. The targeted process-status lookup is denied in this
tool context, so process absence is not established. Confirm the old diagnostic
window/process has closed before any further comparison; do not run another
instance or bypass the cooperative stop policy.

The preceding batch has completed Metal callbacks. The awaited batch returns
success from Vulkan submission and returns from Metal commit, with no scheduled
or completed callback observed for its retained chunk. Its bounded tail retains
96 of 26,764 CPU command notes, including indexed draws and image blits. These
are candidates, not the exact executing or failing GPU instruction. The actual
batch and shader identifiers remain private. PerVertexKHR failures persist
(406 messages), as do SRT/Phi errors and buffer clamps. No CPU-fatal or audio-
backend-failure path is exercised in this run.

There are **968 presentation results**, ending at recorder time 108.752 seconds;
these are not gameplay FPS or pixel validation. The largest gap between recorded
results is 4.170 seconds. No later presentation result is recorded before the
supervisor stop. Native allocation peaks at **11.584 GiB**, last **11.572 GiB**;
these samples predate the stop and are not final pressure measurements. The
**3 GiB upload** remains in the last staging inventory, but that inventory is
already **57.953 seconds old at the last presentation**. Its reusable fields
cannot justify freeing live storage. The buffer-origin journal reaches its cap.

The image journal remains below its cap. Offline replay exactly matches the
last bounded summary (377 images / 901.52 MiB, 1,007 proxies destroyed / 27 live),
then continues another 18.844 seconds. At its last event it records **6,129
allocations / 5,665 destructions / 464 live images / 1,135.69 MiB**. Of these,
435 registered images retain 1,107.32 MiB; **26 proxies retain 23.46 MiB after
1,037 actual proxy destructions**. Two other images have deletion queued
(4.91 MiB), and one small image has no recorded association yet. There are no
unmatched replay events; the last bounded tracker reports no errors or overflow.
This distinguishes continuing proxy cleanup from growth of registered images;
it does not prove a leak or identify the stalled resource. No forced final
memory snapshot accompanies this watchdog stop.

Audio has **52 healthy windows**, 26 per port, spanning about 130.227 seconds
per port and delivering **47,734 input frames/s**. The maximum submission gap is
**95.384 ms**, with no queue wait failure or output error. Delivery continues
after the final presentation result; speaker continuity and audiovisual alignment
remain unmeasured. Archive writes take up to 13.634 seconds, but timing alone
does not make them the cause of the terminal stall.

Next priorities: inspect the awaited submission's resource lifetimes and native
encoding/completion boundary using synthetic fixtures, while tracking registered-
image retention separately from proxy cleanup and staging reuse. Keep the earlier
CPU corruption/audio failure open. Preserve runtime b2ff9cd0, CPU b9652d75,
reference 411fdd86, cache 24/9, 640x360 and DMA/async off. This review changes
documentation only and starts no game/GPU run. Private reproducible calculations
remain with the ignored capture. Publication after review was requested by the
owner; do not claim it complete without a successful push.

### Prepared integer blit correction; gameplay milestone reported

On **2026-10-22**, the owner confirms reaching the game's gameplay section on
Apple M1, while explicitly reporting that the port is **not playable yet**.
This is an owner observation, not a pixel capture, performance measurement or
proof of stable gameplay/save support. At the owner's request, the README no
longer has the Warning callout; the native testing procedure and runtime guards
remain unchanged.

The latest analyzed capture below contains two Metal render-pipeline failures:
R32Uint and RG32Uint destinations were paired with the blit's `float4` output.
The checked `sotc-integer-blit-v1` overlay now selects `uint4`/`int4` fragment
outputs and matching integer textures. `read()` preserves integer texels without
float conversion and uses the source mip explicitly. The first supported scope
is same-format, single-sample, nearest-filtered 2D color blits. Signedness/format
conversion, arrays, cubes, 3D, multisampling and unsupported subresources report
an explicit error before recording any part of the integer batch. They do not
fall back to the float shader. Existing rectangle scaling/mirroring remains.
The original float/depth shader source and selection remain in a separate
library, so failure to compile the new integer library cannot invalidate them.

Validation: the ARM64 library rebuild and symbol audit pass, along with eleven
renderer source regressions, 24 builder tests and full JIT/interpreter contracts.
The integer regression evaluates the actual selector/guards and mechanically
translated MSL bodies with CPU texture doubles: 384 signed/unsigned cases cover
exact 32-bit values, three mips and nearest edge coordinates. The pinned selector
fails the integer contract; the adapted one passes. Builder fixtures cover clean
application, migration from both earlier overlay cohorts, idempotence and rejection
of unknown/partial sources. `xcrun metal` is unavailable: no native Metal shader
compilation, GPU copy or game execution was performed by these checks. Correct
M1 pixels and absence of the two errors still need the owner's comparison.

This targets the typed-copy failures, not the OOM or CPU corruption. The staging
allocation policy and ownership remain: smaller rounding could cause additional
retained buffers if requests subsequently grow, so the 2.5/3 GiB guards are not
changed. The Metal archive already has an asynchronous writer; moving it to a
worker would not be a new fix, and its timing alone does not establish causality
for the presentation gap. Keep these investigations separate.

The [fresh upstream review](DEPENDENCY_WATCH.md#pre-comparison-integer-blit-review-2026-10-22-1525-utc)
records current access limits and preserves the previous exact-SHA baseline.
Keep runtime b2ff9cd0, CPU b9652d75, reference 411fdd86 and cache 24/9. After the
previous window is closed, the owner can run the same combined command, once,
with 640x360, DMA/async off, JIT/statistics and metadata on, pixel capture off and
no automatic duration limit. Stop manually after a brief comparison. Inspect
integer blit/shader errors, real exit status, memory inventory and audio progress
before another run. The owner requested GitHub publication after that comparison;
the subsequent comparison and its limits are recorded above.

### Native repeat: Metal OOM despite proxy retirement; long presentation gap

The **2026-10-22 14:58 UTC** owner run ends after **120.309 seconds** on
**Metal command-buffer out-of-memory error 8**, with confirmed child/supervisor
exit **23**. There is no wall deadline, CPU fault, audio timeout or GPU-watchdog
stop in this capture. The failure is not a 120-second timer. Settings remain
640x360, DMA/async off, JIT/statistics and metadata on, pixel capture off; a
DualSense connects. This run follows the diagnostic preparation below, but does
not exercise its new CPU-fatal or audio-backend-failure branches. The preceding
SIGBUS and stopped audio queue remain unresolved, not disproved or fixed.

The forced failure snapshot records **11.571 GiB** native allocation; the visual
journal's peak is **11.577 GiB**, against an **11.840 GiB** recommendation. That
recommendation is not a hard limit or a promise that another GPU allocation can
succeed. The retained failed command-buffer generation has all six CPU metadata
notes, including one compute dispatch candidate. It does not identify the precise
GPU instruction or resource that exhausted memory.

Proxy cleanup still works: **923 created / 900 destroyed / 23 live**, retaining
**20.30 MiB**, with no pending proxy deletion. The complete image journal has
**5,786 allocations / 5,403 destructions / 383 live images**, **899.23 MiB**.
Offline replay exactly matches the forced ledger: 357 registered images retain
852.91 MiB, 23 proxies retain 20.30 MiB, and three other images have deletion
queued (26.02 MiB). Those three retirement observations are only 5.9–7.6 ms old;
they are not evidence of a stuck deletion queue. No event was dropped and no
tracker overflow, unmatched/untracked free or observation error is recorded.

One cross-window copy requests about 2.740 GiB and allocates **3 GiB** of upload
staging; its CPU copy takes 2.082 seconds. The final staging inventory retains
that buffer, with a sample age of 1.642 seconds. Total sampled staging is 3,500
MiB. From the first presentation result after 20 seconds to the error, tracked
Vulkan memory grows **386.20 MiB**, exactly the image-byte growth; native
allocation grows 397.39 MiB. The large upload is a fixed contributor during that
interval, while image inventory grows. Neither this trend nor registered state
proves a leak or permits discarding unresolved GPU contents. The collector's
last sample is 3.430 seconds old at failure, with tick 258; cumulative retention
decisions are 37 unresolved-GPU and 67 tiled-download, separately counted.

There is a **24.771-second gap between presentation API results**, from 84.588
to 109.359 seconds on the visual recorder's clock, not supervisor elapsed time.
The frame CSV independently reports a 24,767.14 ms interval. The surrounding
statistics window records 706 shader/pipeline compilation events and 22,157.5 ms
of accumulated compilation time; the log also reports a 7,790 ms Metal binary
archive write in this interval. These timings can overlap/nest and must not be
added. They make pipeline compilation/cache serialization a concrete profiling
candidate, without identifying one culprit shader or proving all of the pause
is compilation. Resource collection samples have a 30.006-second gap, then resume;
image allocation/destruction and audio delivery continue during that interval.
There are 1,002 presentation results, not validated pixels or gameplay FPS.
PerVertexKHR failures, the two integer-format blit failures and clamps remain.

Audio records **44 healthy windows**, 22 per port, covering about 110.17 seconds
per port and delivering about **47,270 input frames/s**. No queue wait failure,
output error or backend-failure snapshot is recorded. The largest submission
gap is **448.116 ms**; the device quantum remains 1024 frames / 48 kHz. The final
partial window is not flushed by immediate Metal-error exit. This validates
neither uninterrupted audible output nor the new failure-only diagnostic paths.

Next review priorities are safe large-upload lifetime/reuse and the growth and
retention of registered images, plus timing of pipeline compilation/archive
writes around long presentation gaps. Keep the earlier guest allocation/write
provenance investigation separate. Do not remove ownership/completion guards,
silence shader errors or infer a fix from failure-mode alternation. This review
changes documentation only, preserves pins/cache 24/9 and starts no new game/GPU
run. Private evidence and reproducible calculations remain under the run's
ignored output. A fresh upstream review is required before a new comparison.

### Prepared comparison: audio backend failures and caller-local memory

After the 14:23 UTC analysis below, the ARM64 diagnostic is rebuilt with bounded
audio failure snapshots and additional fatal CPU context. The subsequent 14:58
UTC owner run above ends on Metal OOM without triggering these failure paths.
This is evidence collection, not a claimed fix for
the allocator corruption or stopped audio queue.

With `SOTC_AUDIO_TRACE=1`, the first eight backend failure groups sample every
selected port, including peers skipped by an earlier port's failure. They record
the device/format query, pause-query result, input queue and available converted
output bytes. The failed wait records first/last/minimum input bytes and poll/
change counts. Queue timeouts, clock failures and failed SDL calls are distinct;
an SDL error is copied immediately after a failed call, before diagnostic getters
can replace it. Timeouts do not inherit stale SDL errors. Optional getters are
not playback requirements. Real errors, partial submissions, the queue bound and
two-second deadline remain intact; no stream is restarted or data fabricated.
The private HTML report includes these snapshots. See
[audio diagnostics](AUDIO_DIAGNOSTICS.md) for interpretation and caps.

Fatal JIT context also samples eight protected words immediately before each
of at most eight frame-pointer candidates. These are raw candidate local words,
not validated arguments or writer provenance. Existing guest-range/GPU-page
guards and signal-derived exit codes remain. See
[CPU diagnostics](CPU_DIAGNOSTICS.md).

The ARM64 rebuild and full JIT/interpreter contracts pass, including multi-port
timeout isolation, immediate error preservation/JSON escaping, absent optional
getters, exact PCM behavior and fatal-context reads. The report's escaped-error
and incomplete-tail regression passes. Fresh public pre-comparison review is
[recorded with access limits](DEPENDENCY_WATCH.md#pre-comparison-audio-diagnostic-review-2026-10-22-1455-utc).
Keep runtime b2ff9cd0, CPU b9652d75, reference 411fdd86 and cache 24/9; scene
640x360, DMA/async off, metadata on, pixel capture off and no automatic game
duration limit. Use the combined launcher only after the prior window closes.
The next log should distinguish the failed port's observed backend state from
its peers and provide more caller context. Allocation/write provenance remains
the main unresolved CPU investigation.

### Native CPU context captured: invalid allocator metadata and audio queue stop

The **2026-10-22 14:23 UTC** owner run ends after **120.001 seconds** on
**SIGBUS**, with confirmed child/supervisor exit **138**. The supervisor reports
no wall budget; this is not a 120-second timer stop. No Metal command-error,
GPU-watchdog or unresolved-import stop is recorded. Scene remains 640x360,
DMA/async off, JIT/statistics and metadata on, pixel capture off; a DualSense
connects. The new bounded CPU context completes without a truncation marker.

Private offline inspection finds a **store**, at a different instruction from
the preceding SIGSEGV. The surrounding guest code appears to allocate/split a
free-list block. Its supposed node contains readable resource-name text, including
the word used as a count. That captured word reproduces the saved index, scaled
offset, stored value and exact failing address. The original and linked bytes
of the inspected block match. This strengthens a corrupted/stale node or invalid
reuse hypothesis: the immediate address arithmetic is consistent with its bad
input. It does not identify the writer, prove use-after-free, exclude an earlier
translation/ordering defect or establish a connection with GPU maintenance.

All eight frame candidates have a call immediately before their return position
(six direct, two indirect). Candidate callers include byte-buffer/string growth
and allocation paths. This supports the interpretation but is not a validated
unwind or a record of the indirect targets. Exact words, names, addresses,
instruction bytes and disassembly remain in ignored private analysis output.

Audio has a separate newly observed failure. Fifteen healthy windows per port
deliver about **47,919 input frames/s**. The first queue failure appears
**80.176 seconds after the audio recorder's window origin**, not supervisor
start. Seventeen failing windows then show roughly two-second queue waits;
after the first, sixteen accept no new buffers on the first port. Its input
queue stays at **40,960 bytes**, above the **32,768-byte** pre-submit limit.
The batch returns the real provider error; the 16 output-error records hit their
logging cap and are not the total failure count. The first port's failure exits
the batch before submitting the second, so missing later second-port windows do
not diagnose its device. Device pause/removal, callback progress and backend
error details are not recorded. No causal link to the later CPU crash is proven.

The latest resource summary has **1,535 proxy creations / 1,505 destructions /
30 live (27.32 MiB)**, with none pending. Total live inventory is **247 images /
715.16 MiB**; 6,973 allocations minus 6,726 destructions matches the ledger.
Twenty-two summaries show continued collection, ending at collector tick 342;
51 unresolved-GPU retention decisions remain separate from 106 tiled-download
decisions. The image journal reaches its cap (875 dropped events by the last
summary), with no tracker overflow, unmatched/untracked frees or observation
errors. A CPU stop does not force a final resource snapshot.

Native allocation peaks at **11.435 GiB**, last **11.430 GiB**. A single large
cross-window request allocates the retained **3 GiB** upload; the last dedicated
upload inventory is **3,112 MiB**, including another allocation. CPU copy
completion is not GPU completion. Pressure remains high despite proxy cleanup.
The visual journal has 1,175 presentation-result records, not validated pixels;
PerVertexKHR failures, two integer-format blit failures and size clamps remain.

Next priorities are allocation/free/write provenance for the bad guest node,
including synchronization, and bounded audio device/queue-progress diagnostics.
Do not replace metadata, skip the store, weaken memory guards or increase the
audio timeout as a remedy. The AnyPS5 thread now supplies additional review
candidates, especially command-memory reuse and barycentric built-ins; see
[the watch baseline](DEPENDENCY_WATCH.md#anyps5-macos-thread-review-2026-10-22).
These are different implementations, not established fixes for this crash.
That analysis changed documentation only. The subsequent diagnostic preparation
and pre-comparison review are recorded above; the targeted AnyPS5 review alone
does not replace the broader upstream check.

### CPU fault investigation: bounded context and indexed-load regression

The host diagnostic now collects protected, bounded register-base memory,
guest-stack words and frame-pointer candidates on a terminating translated
fault. GPU-tracked pages and non-guest ranges are skipped; partial reads remain
explicit. There is no new execution-path hook, game patch, renderer mutation,
dependency update or change to exit 139. The implementation adapts the pinned
bloodborne-runtime probe's field/stack diagnostics without its game assumptions.
See [CPU fault diagnostics](CPU_DIAGNOSTICS.md) for bounds and evidence semantics.

An original synthetic masked-index fixture passes **8,064 cases in each of JIT
and interpreter**, including 64-bit indices, wraparound, scales, displacement,
unaligned reads and protected page edges. The real diagnostic's fatal path
retains exit 139 and records the fixture's metadata/frame candidate; protected
and unmapped read fixtures pass too. ARM64 build, native emitter/JIT/vector
checks and full JIT/interpreter contracts pass. This has not reproduced the
native corruption or established its cause. The subsequent **120.001-second
SIGBUS** above now exercises the added context in a native owner run; the
**182.664-second SIGSEGV** below is the preceding boundary.

The subsequent owner-run context now compares object words with the saved
index and identifies call-site candidates privately. Use that evidence to
select an allocation-lifetime/write-origin or narrower CPU reproduction.
Keep CPU metadata corruption and the still-high GPU memory use as separate
unresolved problems. Fresh public upstream review is still required before
supplying another native comparison, with unavailable sources recorded.

### Native busy-queue comparison: proxy destruction observed, CPU fault boundary

The **2026-10-22 13:55 UTC** owner run uses the consolidated Documents checkout,
640x360, DMA/async off, JIT/statistics on, metadata on and pixel capture off.
The supervisor has no wall deadline. It ends after **182.664 seconds** on
**SIGSEGV in translated guest code**, with confirmed child/supervisor exit
**139**. No Metal command-error, GPU-watchdog or unresolved-import stop is
recorded. A DualSense connects; the main loop begins at 1.5601 seconds. This
supersedes the 78.963-second OOM boundary below, without establishing OOM
avoidance in a longer run or proving that the maintenance change caused the
different failure.

Busy-queue maintenance now has native lifetime evidence. The last available
resource summary records **1,263 proxy creations / 1,233 actual destructions /
30 live proxies (27.32 MiB)**, versus 336 live / zero destroyed in the preceding
run. The 1,009.31 MiB destroyed is cumulative allocation traffic, not concurrent
memory savings. Sampled live proxies range from 17 to 35 after scene transition.
No proxy deletion is pending in that last summary. Total image inventory is
220 images / 704.28 MiB, with 7,669 allocations and 7,449 destructions; the
ledger arithmetic matches. Twenty-seven resource snapshots show continued
collection, with the final collector tick at 398. Unresolved-GPU retention
records 53 cumulative decisions, separately from 154 tiled-download decisions.
Neither count denotes distinct images or cumulative upstream kept/freed totals.

The detailed image journal reaches its 8 MiB cap: 5,729 image events are dropped
by the last summary, while tracker overflow, unmatched/untracked frees and
observation-error counters remain zero. Its detailed history cannot reconstruct
the final ledger. The bounded ledger summaries remain usable, but SIGSEGV does
not force a final resource snapshot; these are last sampled values, not exact
failure-time inventories.

The visual journal samples native allocation up to **11.423 GiB**, last
**11.405 GiB**, against an 11.840 GiB recommendation. Pressure remains high.
One **3 GiB** upload buffer remains: one cross-window copy and two short-mapping
fallback copies complete on the CPU, reusing that buffer. The last staging
inventory is 59.667 seconds old; its reusable classification is not permission
to release the resource at failure. There are 1,254 presentation API records,
not validated pixels or gameplay FPS. PerVertexKHR pipeline failures, integer
blit failures and buffer-size clamps remain.

Offline decoding of the private prepared image identifies a load using a
size/offset read from object metadata and masked for alignment. Its saved
size/offset is implausibly large; the fault address matches the effective
address computed from the recorded guest registers. The relevant original and
linked instruction bytes match. This narrows the immediate failure, but does
not distinguish earlier metadata corruption, an invalid object/lifetime, or a
CPU translation error. The log contains no dynamic object-memory snapshot,
writer provenance or call stack establishing the cause. Exact addresses and
disassembly remain only in ignored private analysis output.

Audio has 66 complete windows, 33 per port, delivering about **47,815 input
frames/s per port**. Maximum submission gap is **182.876 ms**, with no recorded
queue/output failure; the device stays at 1024 frames / 48 kHz. Audible output
and audiovisual alignment still require owner observation.

Next debugging priority is the invalid metadata/offset feeding the CPU load:
inspect its allocation/lifetime and narrow the write or translation responsible
using private evidence and original synthetic ABI/JIT comparisons. Do not skip
the load, invent a valid size, or undo GPU-completion ownership guards. Keep
the maintenance correction, pins/cache 24/9 and launch settings while gathering
evidence; a causal regression is not ruled out. No runtime code, dependency or
game input is changed for this analysis, and no additional game/GPU test is run.
The preceding access-limited public review remains the review boundary.

### Documents workspace launch preparation: 2026-10-22 13:54 UTC

The selected checkout is now `~/Documents/sotc-mac`; its private relocation
checkpoint reports `ready-in-documents`, renderer rebuilt and CPU contracts
passed. The earlier pending-move wording below is historical. A fresh host-import
audit finds 46 present and zero missing, without loading the GPU library.
The [pre-comparison upstream review](DEPENDENCY_WATCH.md#pre-comparison-access-limited-review-2026-10-22-1354-utc)
is access-limited: exact current public heads/PR diffs could not all be verified.
Retain selected pins and run only the existing owner-supervised comparison.
Process inspection is unavailable in this session, so the owner must ensure
the preceding diagnostic window has closed. Native validation remains pending.

### Consolidated workspace

The full project, original game input, selected source checkouts, installed
libraries, private diagnostic history and busy-queue maintenance overlay are now
assembled in one checkout. Start here, rather than applying the earlier standalone
patch to a different tree. The previous maintenance-only checkout is historical.
[Workspace instructions](guide/workspace.md) describe relocation and continuation.
All **1,678 game files / 15,535,038,891 bytes** match the source copy by SHA-256.
The game was not launched or altered during consolidation. The full ARM64
rebuild in the assembled checkout, ten renderer source regressions, 24 builder
tests, two relocation tests, 46/46 host imports and JIT/interpreter contracts pass.
Installed dynamic libraries and rebuilt renderer dependencies resolve inside the
assembled checkout. The finalizer repeats relocation/build checks after the move.

Installed Mach-O dependency paths, pkg-config metadata and internal tool links
are repaired for the current root; system libraries remain external. A moved
CMake cache receives fresh configuration before any build, preserving previous
output and preventing generated commands from writing into the former folder.
The private relocation checkpoint records the actual and requested destination.
The Documents move still requires the owner's Terminal because this session's
write scope is limited to the staging workspace. The supplied finalizer relocates
paths and rebuilds/checks in the destination without launching the game.

The native stopping boundary remains **78.963 seconds, Metal OOM 8, exit 23**.
Busy-queue cleanup has CPU/build validation only. Resume by reviewing the latest
public upstream state and then inspect one owner-supervised 640x360 comparison
with DMA/async off, cache 24/9 and the existing GPU-progress stop preserved.


### Busy-queue maintenance correction

The newest owner comparison exercises the integrated dependency cohort at
640x360 with DMA/async off and no duration limit. It ends after **78.963 seconds**
on actual Metal out-of-memory code 8, child/supervisor exit 23. Native allocated
memory is **11.664 GiB**. The complete image journal matches the final ledger:
573 live images / 941.54 MiB, including **336 reduced-scene proxies / 277.75 MiB**.
None of those proxies is retired or destroyed; 317 source UIDs have already been
destroyed. A single **3 GiB** upload buffer is reused for eleven large cross-window
copies. The last collection observation precedes failure by **24.086 seconds**;
all proxies are created after that observation. Tracker and dropped-record error
counters are zero. These are resource observations, not correct-pixel evidence.

`Liverpool::SignalDecodedFrames` runs inside the busy decode loop, while the
ordinary `OnSubmit` maintenance waits outside that loop for empty queues. The
new checked overlay calls `Rasterizer::MaintainCaches` at ordered decoded-frame
boundaries too. This shares the existing download/collector/proxy-retirement work,
throttles the extra passes to 250 ms, drains prepared draws and marks the ordered
task as reading guest memory. Ordinary idle maintenance remains unconditional;
`runtime.TickFrame` remains exclusive to `OnSubmit`. No guest work is discarded,
no GPU-wide wait or early eviction is added, and the large buffer is unchanged.

The frame's GPU-completion signal is queued **before** maintenance's past-next
deferred owners. This preserves progress in the scheduler's FIFO: a frame must
not wait for the following tick merely to notify the guest that it finished.
Proxy images and views still survive until real completion at current tick + 1.
The incremental overlay has digest-checked migration from the previous complete
cohort as well as a clean application from the runtime pin; partial/unknown
sources remain rejected. Dependencies and shader cache stay **24/9**.

`check_busy_cache_maintenance.py` compiles the production frame-retirement and
maintenance methods with CPU resource doubles. In both serial and pipelined
modes, 200 continuously busy frames retain 201 proxies before versus one active
proxy / 200 actual destructions after correction. It checks the throttle,
forced idle passes, pending guest-read protection, unchanged runtime frame ticks,
frame-signal progress at the current tick, and image survival until the next tick.
This is synthetic lifetime/progress evidence. The full ARM64 renderer build,
all ten renderer source regressions, 23 builder tests, 46/46 host-import checks
and complete JIT/interpreter contracts pass. Native OOM avoidance remains
unverified; the owner must inspect one new comparison before another experiment.


### Current dependency integration and native comparison boundary

The owner explicitly requested integrating all dependency updates and keeping
their current/latest versions visible on GitHub. The selected sources are now
runtime **b2ff9cd0b982aa481fc48d4ce51b3cc7b4cd2548**, CPU
**b9652d75edc0a281f06cec0da6ff72e9f5150a70** and SotC reference
**411fdd86dadce347dcc4443228b009ae9265a548 / v0.60.5**. See
[the complete inventory](DEPENDENCIES.md) and [lock](../tools/dependencies.json).
The reference source is updated; its Windows renderer does not replace our Mac
renderer. Runtime/CPU/reference histories advance 20/2/72 commits respectively.

All selected recipe libraries were freshly built/installed, with the official
MoltenVK v1.4.2 binary retained at the latest stable release. New stable versions
include fmt 12.2.0, magic_enum 0.9.8, robin-map 1.4.1, VMA 3.4.0, Xbyak 7.44,
xxHash 0.8.4, Boost 1.92.0 and FFmpeg 9.0.2. Dear ImGui is checked at 1.93.0;
Sirit now uses installed SPIRV-Headers v1.202609.0. The CPU prefix's older Zycore
was replaced with the bundled 1.5.2 pair. ROCm's current half header is identical
to our embedded 1.12.0. glslang 16.6.0 is built locally as ARM64, resolving the
former fresh-build preflight blocker. Current Vulkan/SDL/SPIRV-Cross releases
and unchanged FSR-Vulkan head are recorded separately from unreleased proposals.

The checked overlay is consolidated against the new runtime base. The three
earlier upstream backports are already incorporated there. Local scene-proxy
retirement, exact-only PerVertex compatibility, wave64/SRT/Phi corrections,
buffer-copy chunking, staging reuse, private diagnostics and native failure exits
remain. Shader binary version is **24**, metadata **9**; the withdrawn buffer-
bound experiment remains absent. Ajnc c9be8142 completed-coroutine cleanup is
adapted to all five local nested-command/self-test sites. Its CPU regression
reproduces 500 outstanding child frames before and zero after; this is not native
heap or Metal OOM evidence.

The last public-head refresh caught the additional **b2ff9cd0** collector fix,
which is now included: MaybeCpuDirty can describe a neighboring write, while an
image's only valid contents remain on the GPU. Such unresolved GPU-modified
images are retained unless CpuDirty proves an actual CPU overwrite. The
actual-predicate CPU fixture verifies the old/current/adapted behavior, preserving
ordinary tiled-download and pressure eviction rules. `gc_kept_unresolved_gpu`
counts this reason separately. This correctness guard can retain more memory;
do not remove it to reduce pressure or claim it solves our OOM.

Sources/builds are isolated under the `22923f080229` workspace key; the actual
required HEAD is b2ff9cd0 and is verified independently. This fixed integration
key avoids moving installed-library identities while the final upstream commit
is incorporated. The old qualified renderer/dependencies remain untouched;
private rollback binaries and original source snapshots are retained. Do not mix
libraries, source revisions or dependency prefixes from different cohorts.

Native emitter/JIT/vector checks and full JIT/interpreter contracts pass. The
fresh ARM64 renderer builds with regenerated host shaders and the selected
dependencies. All **nine production-source renderer regressions**, **22 builder
tests** and **46/46 host-import symbol checks** pass. CI now checks the generated dependency inventory and uses
the current checkout action by immutable commit. Hosted CI execution is distinct
from local checks. No tool-driven game or GPU test was launched.

Next is **one owner-supervised comparison** with scene **640x360**, DMA/async off,
JIT/statistics on, metadata on and pixel capture off. Default game duration remains
unlimited and GPU-progress supervision stays active. Refresh public heads again
if delayed; inspect that one log before another run. Compare proxy live/pending/
destroyed bytes, unresolved-GPU retention, the single large upload and actual
Metal allocation/error. The last confirmed native boundary remains 09:41Z:
**101.864 seconds, Metal OOM code 8, child/supervisor exit 23**, before this cohort.
Correct pixels, stable playback and avoidance of OOM remain unverified.

### Stale reduced-scene proxy retirement: built, native comparison pending

The new `sotc-scene-proxy-retirement` checked overlay addresses the retained
per-UID/mip map identified below. `Rasterizer::OnSubmit` already drains the draw
pipe before maintenance. It now calls `SceneTargets::CleanupStaleEntries` there,
after the ordinary texture collector. A proxy is eligible only when lookup of
its original **ImageId and exact UID** fails. Native images still present,
including normal deferred deletion, retain their proxies. Dirty live proxies
and all native-access/resolve semantics are preserved.

Stale entries leave the map, tracked-UID set and recent-pointer cache; scene
generation advances to invalidate texture sets holding old proxy views. A moved
collection owns **both views and images** through `DeferOperationPastNext` and
releases them only on real completion at current tick + 1. No new `Finish`,
fault-thread mutation, forced eviction, timer or fabricated completion is added.
Entry destruction releases views before their image. This is a local adaptation
of [bmy 8185b88d](https://github.com/bmy/bbport-mac/commit/8185b88d80573fa300595d5c54450183b4109935),
not a dependency bump; [source review](UPSTREAM_REVIEW.md#reduced-scene-proxy-retirement)
records the original global-wait approach and its limits.

`image_scene_proxy` events now tag the allocation's source UID/mip and role 1.
Role arrays distinguish unspecified allocations from reduced scene proxies;
created/destroyed and pending counts/bytes distinguish retirement from actual VMA
destruction. A pending event's tick is its required completion tick. These are
private metadata; no pixel capture is enabled and old runs remain readable.

`tools/check_scene_proxy_retirement.py` compiles the production `Get`, `Entry`,
cleanup, `OnSubmit`, scheduler past-next deferral and image destructor with CPU
resource doubles. The pinned control retains **204 proxies / zero destructions**;
the adapted case retains **two active proxies / 202 actual destructions**. It
checks reused ImageIds, two source mips, recent-pointer hits/invalidation,
generation, tracked UIDs, live dirty-content preservation, idempotence, view-before-
image destruction and survival until current tick + 1. This is a synthetic
ownership regression, not a GPU-driver or Apple M1 result.

Validation: ARM64-only incremental renderer build passes using existing
dependencies/generated shaders; a fresh dependency build still needs external
glslang. Seven renderer source regressions, 28 builder tests including exact
memory-state migration, 46/46 host-import coverage and full JIT/interpreter
contracts pass. No tool-driven game/GPU run has validated the correction.
The confirmed native failure remains the 09:41Z Metal OOM 8 below.

Fresh public review on 2026-10-22 resolves the watched default heads to the same
SHAs already listed in [dependency watch](DEPENDENCY_WATCH.md). Ajnc remains at
411fdd86/v0.60.5; its coroutine fix and PR status remain separate. MoltenVK #2858
and SPIRV-Cross #2699 remain open drafts; #2697 remains open/non-draft, all
unmerged at their recorded heads. Their shaders/drivers are not changed here.

Next: one owner-supervised run through the same introduction, **640x360,
DMA/async off, cache 23/9**, with memory/audio/visual metadata on and pixel capture
off. Refresh upstream again if there is a delay before that comparison. Inspect
role-1 live/pending/destroyed counts and bytes against native allocation growth;
verify released proxies do not return via cached views and note any changed
visual artifact, device error or stop. Do not equate the old 806.35 MiB untagged
inventory with guaranteed savings. The retained 2.5/3 GiB upload allocation,
other image roles, Metal pressure and shader defects remain separate candidates.
Stop after that run and inspect its log before another experiment.

### Complete repeat trace: unassociated reduced images accumulate without destruction

The newest owner run starts at **09:41Z on 2026-10-22** and ends after
**101.864 seconds** on confirmed Metal command error 8, child/supervisor exit 23.
The main loop starts in 1.557 seconds and a DualSense is detected. Native allocation
is **12,512,018,432 bytes (11.65 GiB)** against the same recommendation, a 191.78
MiB margin. Scene remains **640x360**, DMA/async off, cache 23/9 and pixel capture
off. A shorter run does not establish a performance or regression comparison.

This time there is only **one** observed large cross-window copy. Its request is
2,629,644,160 bytes, rounded to **2,684,354,560 bytes (2,560 MiB / 2.5 GiB)** staging;
allocation/copy/flush completes in **995.63 ms**. The previous run's initial
larger compute request does not cross the selected window in this workload.
Do not assume every run allocates a 3 GiB staging buffer. Total staging inventory
at its last sample is 2,988 MiB; that sample is 31.58 seconds old.

The **complete** image journal has 5,972 allocations, 4,768 actual destructions
and 19,518 ownership events. Its 6.90 MB size is below the 8 MiB cap. All dropped
record, overflow, unmatched/untracked free and diagnostic-exception counters are
zero. Replaying all lifetime events reproduces the final ledger exactly:
**1,204 live images / 1,476.63 MiB**. Of these, 223 registered images occupy
**670.28 MiB**, while **981 allocations without a cache owner occupy 806.35 MiB**.
No unregistered or queued-deletion bytes remain at failure.

Unlike the preceding capped run, the full history proves that **none of the 981
unassociated images is destroyed** during this run. Every one is 640x360 or
320x180, one mip/layer/sample. Four recurring format/size classes account for
975 of them and about 800 MiB. Their final-live inventory appears after roughly
65 seconds and grows through failure, while registered-cache bytes remain near
670 MiB. This materially strengthens the reduced-scene proxy retention candidate:
the source map is keyed by original UID/mip and lacks per-source retirement.
Allocation role is still not explicitly tagged, so do not claim each handle has
been traced to `SceneTargets::Get`. Next isolate source/proxy UID ownership and
qualify retirement after all prepared/recorded work, including cached recent
pointers/views; retain actual GPU-completion guards and the 640x360 setting.

The last collector snapshot is **35.14 seconds old**. Its usage/threshold samples
are not current failure-time pressure measurements, and resettable GC counters
are not cumulative totals. The corrected window-sample labels are exercised in
this run. The failed Metal batch again has a complete seven-note CPU tail with
one compute candidate, not a precise failing GPU instruction. The journal records
1,236 presentation API successes, which do not prove correct pixels.

There are 38 complete audio windows, nineteen per active port, measuring about
**47,733 input frames/s per port**, maximum submission gap **156.539 ms**, and no
recorded queue/output failure. Input-queue zero is not a device underrun.
This is an evidence update; no renderer policy change or new tool-driven GPU
comparison accompanies it. The prior ARM64 build and full CPU checks remain the
validation baseline. Refresh upstream before the next proposed native trial.

### Native resource evidence: large-copy origins and late auxiliary-image growth

The preceding owner run starts at **09:25Z on 2026-10-22**, using `d86465b` and
`SOTC_MEMORY_TRACE=1`. The new metadata is exercised natively. The main loop starts
in 1.573 seconds, a DualSense is detected, and the run ends after **175.402 seconds**
on actual Metal command error 8 (`Insufficient Memory`), child/supervisor exit 23.
Native allocation is 12,558,565,376 bytes (**11.70 GiB**) against a recommendation
of 12,713,115,648 bytes, a 147.39 MiB margin. The recommendation is not a hard
limit. Scene stays 640x360, DMA/async off and cache 23/9; pixel capture is off.
The duration difference from 94.305 seconds is not a performance/stability result.

The first raw, read-only, unformatted compute binding declares almost 4 GiB. Its
mapping-clamped request is **3,047,712,528 bytes (2,906.53 MiB)**, while the selected
3,584 MiB imported window has only **1,436.65 MiB** left at that physical offset.
The observed cross-window fallback therefore allocates **3,072 MiB** staging.
Allocation plus sparse CPU copy/flush takes **1.728 seconds**, including about
1.232 seconds before the staging buffer is selected. A second shader requests
2,639,096,320 bytes and crosses the same window end; it **reuses the same buffer**
and completes its CPU copy in **133.28 ms**. There are exactly two large fallback
requests/completions, 5,686,808,848 cumulative copied bytes, not two simultaneous
multi-GiB allocations. The one 3 GiB upload buffer remains through failure.
This proves the allocation path and originating bindings in private metadata,
not the precise GPU allocation/instruction that fails much later.

The failure-time `UniqueImage` ledger records **9,015 allocations, 8,404 actual
VMA destructions and 611 live images**, totaling **1,045,502,720 bytes (997.07 MiB)**.
Of these, 224 registered cache images account for **679.87 MiB**, while **387
unassociated/constructed-state images account for 317.20 MiB**. Unregistered and
queued-deletion bytes are zero at the final snapshot. A preceding sample contains
four queued images/18.53 MiB, which have cleared by failure; this does not establish
safe lifetime handling for every resource or physical-driver release.

The cache-associated inventory is around 690 MiB through the 155.5-second visual
time estimate. Most later growth is in unassociated images, which appear in the
last roughly eighteen seconds. This is stronger localization than the earlier
aggregate image-growth observation. As a **source-backed hypothesis**,
`SceneTargets::Get` creates reduced images keyed by original image UID and mip;
its map is cleared on resolution change (after `Finish`) or destruction, with no
per-source retirement. Recreated source images can therefore leave old proxies
retained. Other temporal/motion/upscale resources also use unassociated
`UniqueImage`s; this run does not label their allocation role, so it does not prove
that the 387 images are all scene proxies. Next isolate proxy counts/bytes and
source UID retirement, then qualify any cleanup with actual-source CPU lifetime
tests. Preserve deferred ownership and prepared draw/view references; do not erase
live proxies, change the preset or force global finishes as an untested remedy.

Tracker overflow, untracked/unmatched frees and diagnostic exceptions are zero.
The detailed image journal reaches its **8 MiB cap**, dropping 13,922 later records;
the fixed live ledger and final summary remain complete under their observations.
Per-resource history after the cap is unavailable. Visual image/Vulkan counters
are sampled at the last presentation, about 41 ms before failure; their values
differ from the failure-time ledger and must not be treated as one atomic sample.
The failed Metal batch retains its complete seven-note CPU tail, including one
compute candidate; this is not the precise GPU instruction responsible.

Collection is enabled and sees driver usage above its sampled critical threshold.
There are 304 cumulative tiled/GPU-written retention decisions, not 304 distinct
images. The existing upstream kept/freed counters reset when frame statistics
read them. The first recorder misleadingly named their samples `*_total`; those
fields must not be used as cumulative eviction totals. The recorder/report now
label them `gc_kept_window_sample` / `gc_freed_window_sample`, retaining old-log
reader compatibility. This corrects metadata labels only, not collection policy.
The ARM64 rebuild, actual-source memory fixture and full JIT/interpreter CPU
contracts pass after this correction; shader/cache/resource semantics are unchanged.

There are 68 complete audio windows, 34 per active port, delivering about **47,488
input frames/s per port**, maximum submission gap 120.404 ms and output-lock wait
89 microseconds, with no queue/output failure. Input-queue zero is not a device
underrun measurement. Visible scene correctness remains unverified in this run.

At the owner's request, review fresh public reference and relevant upstream
changes **before each new native game/GPU comparison**, independently of the
daily watch. Compare actual diffs and prior integrations; keep pins and isolate
behavioral backports. A repeat check on 2026-10-22 still resolves the SotC
reference to `411fdd86` and release v0.60.5. No new GPU run accompanies this review.

### Resource diagnostics prepared for the next owner comparison

[Memory diagnostics](MEMORY_DIAGNOSTICS.md) now add bounded private large-buffer
origins, staging allocation/reuse, image allocation/destruction, cache ownership,
queued-deletion metadata and collector summaries to the offline visual report.
`SOTC_MEMORY_TRACE=1` defaults on in the combined launcher. The checked overlay
changes no shader code, persistent metadata/cache version, requested buffer size,
import window, eviction policy or fence/resource lifetime. Scene stays 640x360,
DMA/async off, shader binary/meta versions 23/9. The withdrawn bounds experiment
stays inactive. At this checkpoint 08:50Z was the newest native evidence; the
09:25Z resource comparison above supersedes that boundary.

The immediate next comparison uses the same existing graphics-game command after
the prior diagnostic closes. Inspect which shader/slot requests the initial
multi-GiB copy, its mapping/window boundaries and rounded allocation. Then inspect
image allocation/destruction totals, largest live resources, queued-deletion
bytes and collector thresholds as memory grows. Retained bytes and current
recording ticks do not authorize freeing resources. These records have not yet
been validated at that checkpoint; the later owner run above exercises them.
No tool-driven GPU run is launched.

The ARM64 incremental library builds with existing generated shaders/dependencies;
a fresh dependency build still needs external glslang. All six renderer source
regressions, 27 builder tests, recorder/report/failure-exit checks and full
JIT/interpreter CPU contracts pass. The memory source fixture runs the real
range/copy/staging methods and both actual VMA destruction paths with CPU resource
doubles. It confirms 3 GiB allocation rounding and pending-vs-destroyed accounting
without allocating that amount or accessing a GPU. Four untracked source/header
copies discovered during compilation were preserved outside the build tree;
source verification now rejects such inputs. Their discovery is not evidence
for the cause of earlier native failures.

The owner also asked about the current reference project.
[The reviewed v0.60.5 comparison](UPSTREAM_REVIEW.md#sotc-reference-v0605-comparison)
confirms that the reference pin is 72 commits behind the observed main head.
The release is not integrated wholesale. In particular, its nested CPU coroutine
frame leak fix is absent and remains a candidate for a separate checked backport;
it is not part of this diagnostic-only comparison. Keep reference/runtime pins.

### Visible output returns after rollback; memory exhaustion persists

The earlier owner comparison starts at **08:50Z on 2026-10-22**, after rollback
`2dcaa37`. The owner confirms that the image has returned. This supports keeping
the guarded-buffer-bound experiment withdrawn; it does not identify the exact
cause of the preceding black output or establish correct scene rendering.
The main loop starts in 1.712 seconds and a DualSense is detected. Scene remains
**640x360**, DMA/asynchronous submission off, shader binary/meta versions 23/9.
Pixel capture remains off; visible output is the owner's observation.

The game still stops on actual `MTLCommandBufferErrorDomain` **code 8**, with
`Insufficient Memory`, after **94.305 seconds**. Child/supervisor exits are both
23. There is no duration deadline or GPU-watchdog stop. Native allocation is
12,515,328,000 bytes (**11.66 GiB**), against a 12,713,115,648-byte recommendation,
a 188.625 MiB margin. The recommendation is not a hard memory limit.

One dedicated upload buffer retains **3,072 MiB**. It is first observed by the
7.784-second journal sample and remains the same size/count through failure.
The failure snapshot is 2.267 seconds old; its reusable-byte values do not
authorize releasing retained resources. Dedicated device-local inventory is
140 MiB, with 160/128 MiB device-local/upload rings, **3,500 MiB staging total**.
There are four new dedicated buffers and seventeen reuses; no oversized reuse
is recorded in this workload. These counters overlap native/tracked allocations.

From the first sample after ten seconds to failure, tracked Vulkan allocation
grows from 4,515,781,148 to 5,022,816,540 bytes. Image allocation grows from
512,956,160 to 1,019,991,552 bytes by exactly the same **507,035,392 bytes
(483.55 MiB)**. Staging inventory is stable during that interval. This localizes
the later tracked growth to images, not the initial upload allocation, but does
not distinguish legitimate scene loading/cache retention from a leak or identify
the allocation that fails. The failed batch retains 96 of 324 CPU notes, not
the precise failing GPU instruction.

The journal has 906 presentation API successes and one native error. Final
asynchronous counters include 280,249 missing-pipeline draws out of 674,440
requests, 272 failed pipeline creations and 57,371 reduced decisions out of
57,445 scene decisions. General shader support is still unresolved.
There are 36 complete audio windows, eighteen per active port: about **46,200
input frames/s per port**, with a maximum 186.015 ms submission gap, no recorded
queue/output failure and output-lock waits up to 234 microseconds. Input-queue
zero is not evidence of a device underrun; uninterrupted output is unverified.

Keep the working renderer unchanged while isolating the initial cross-window
full-buffer staging copy and later image lifetimes. A future correction needs
CPU proof before another single owner-supervised GPU comparison. Do not revive
the unvalidated shader-bound experiment, cap guest buffers arbitrarily, trim
retained resources or weaken import/tick/fence guards. This checkpoint changes
only recorded evidence; no runtime change or new tool-driven GPU run is made.
The rollback ARM64 build, five renderer source regressions, 25 builder tests and
full JIT/interpreter CPU contracts remain the validation baseline.

### Black output, repeated OOM and withdrawn buffer-bound experiment

The preceding owner run starts at **08:36Z on 2026-10-22** and ends after
**160.610 seconds** on `MTLCommandBufferErrorDomain` code 8, `Insufficient Memory`.
Child and supervisor both exit 23. The owner reports no logos or visible scene,
while input still works. Presentation API success does not prove visible pixels;
pixel capture was off. Earlier visible logos/menu and defective 3D remain
historical evidence, not the state of this run. Scene stays **640x360**, DMA and
asynchronous submission off.

Native allocation at failure is **12,468,715,520 bytes** against the unchanged
12,713,115,648-byte recommendation. The last journal tracks 5,002,137,756 bytes of
Vulkan allocation, including 1,242,582,400 bytes of images. Dedicated upload
inventory is 2,977,955,840 bytes across six buffers; its snapshot is 1.154 seconds
old. These overlapping counters do not identify the resource that failed or
justify releasing retained resources. Final asynchronous counters include
185,119 missing-pipeline draws out of 470,597 requests and 194 failed pipeline
creations. There are 40,867 reduced decisions out of 41,175 scene decisions.

The local guarded read-only buffer-bound experiment in `9eca092` was intended to
avoid a multi-GiB fallback copy when a shader's proven access range is small.
**No applied buffer reduction is logged in this native run.** Its synthetic CPU
fixtures and build passed, but native matching and memory savings are unverified.
The experiment is withdrawn from the active renderer, including its persistent
metadata additions and cache-version changes. The private source matches the
previous checked overlay manifest again, with shader binary/meta versions
**23/9**. The working audio-quantum and staging best-fit corrections remain.
The local historical commit preserves the experiment for isolated research.

Zero reductions do not explain the black output or establish that the experiment
caused it. The previous ARM64 renderer is rebuilt incrementally using the existing
dependencies/generated shaders. The five actual-source renderer regression tools,
25 builder tests and full JIT/interpreter CPU contracts pass. A completely fresh
dependency build still needs the unavailable external glslang compiler. No
tool-driven GPU run validates this rollback.
At this checkpoint restored output was unverified; the later 08:50Z owner
comparison above confirms its return without proving correct scene rendering.
Keep the same launch settings and change only the renderer revision for that
comparison. Do not start a game run automatically.

The large-copy investigation remains open: a raw descriptor is clamped to the
end of its guest mapping, and a request crossing the selected imported window
falls back to a full staging copy. Allocation rounding can turn an approximately
3 GiB request into a retained 3 GiB upload buffer. A synthetic guard-bound proof
does not demonstrate matching against the native final IR. Do not substitute an
arbitrary buffer cap, disable split imports, add overlapping aliases or weaken
resource/tick ownership checks. Later image growth also remains to investigate.

### Native audio improvement and exercised staging reuse

The earlier owner run starts at **07:23Z on 2026-10-22**, using the corrections in
`a49852f`. The owner reports that audio is much better. The native prerequisite
draw/readback passes on Apple M1; the game starts its main loop in 1.49 seconds,
detects a DualSense and runs for **137.642 seconds**, versus 59.026 seconds in the
preceding comparison. It still stops on actual Metal out-of-memory failure,
`MTLCommandBufferErrorDomain` code 8, with confirmed child/supervisor exit 23.
There is no duration deadline, GPU-watchdog or import stop; a targeted lookup
finds the process absent. Scene stays **640x360**, DMA/async off and cache 23.
The longer run is not a gameplay stability or performance result.

There are 52 complete five-second audio windows, 26 per active port, spanning
about 130 seconds per port. They confirm an actual **1024-frame, 48 kHz device**.
The input limit is 32,768 bytes before accepting the next 8,192-byte guest block,
allowing five blocks instead of the former two. Mean delivery is **47,684 input
frames/s per port**, versus 23,912 previously; median window rates are about
47,988–47,995 frames/s. This is native evidence for the queue correction and
agrees with the owner's audible improvement. Maximum submission gaps still reach
121.85 ms, output-lock waits peak at 54 microseconds and no queue/output failure
is recorded. Zero input-queue observations do not establish hardware underruns;
uninterrupted playback and audiovisual alignment remain unverified. See
[Audio diagnostics](AUDIO_DIAGNOSTICS.md).

The new staging path is exercised: four new dedicated buffers, nineteen reuses
and **four reuses that the old size restriction rejected**. Dedicated upload
inventory is **2,560 MiB versus 3,072 MiB**, one retained buffer. Dedicated
device-local inventory is 140 MiB; device-local/upload rings are 144/128 MiB.
Total staging is **2,972 MiB versus 3,500 MiB**, a 528 MiB reduction in this
comparison, including 512 MiB less upload inventory. These counters overlap other
allocation classes; do not add them to native allocation or physical RAM.
No retained buffer is freed. The final staging snapshot is **31.8 seconds old**,
so its reusable-byte counts do not establish safety to release resources at the
failure. Preserve actual tick, held, memory-type and import lifetime guards.

Native allocation at failure is 12,562,169,856 bytes (**11.70 GiB**) against a
12,713,115,648-byte recommendation, a 143.95 MiB margin. The recommendation is not
a hard allocation or physical-memory limit. Tracked Vulkan allocation peaks at
4,851 MiB, including 1,562 MiB of images, versus 4,771/953 MiB previously.
Image inventory grows later while staging remains stable. The two runs reach
different work, so staging savings do not establish lower total pressure or
identify the exact resource that exhausts memory. The failed chunk retains its
complete twelve-note CPU tail, with the same two compute candidates seen in an
earlier failure; this is not a precise failing instruction or proof of causation.

The visual journal has 3,769 events, including 1,256 presentations with API
success and one native error. Pixel capture stays off; correct pixels remain
unverified. The CSV has 1,242 complete rows and a different clock origin. Final
asynchronous counters include 715,689 missing-pipeline draws out of 1,571,829
requests, 412 pipeline-creation failures and 141,161 reduced decisions out of
141,347 scene decisions. There are 354 PerVertexKHR messages, 271 SRT-walk errors
and 22,799 clamps; totals from this longer run are not comparable error rates.

Next investigate the request and size rounding that produce the single 2.5 GiB
upload buffer, and the later image working set/lifetimes. Keep the audio correction
and existing settings. Do not trim retained buffers or disable split imports:
upstream documents late resource/argument-buffer lifetime failures. No runtime
change or tool-driven GPU run accompanies this evidence update; the prior ARM64
build, source regressions and full JIT/interpreter contracts remain the validation
baseline. Pixel readback and a tracing-overhead comparison still need native tests.

### Earlier half-rate audio and retained staging memory

The earlier owner run starts at 07:00Z on 2026-10-22, after the previous 06:39Z
comparison. Both stop on the actual Metal out-of-memory error (code 8) with
confirmed child/supervisor exit 23, after 59.026 and 64.496 seconds respectively.
The newest starts the main loop in 1.39 seconds, detects a DualSense, and has no
duration deadline, GPU-watchdog stop or import-stop report. A targeted lookup
afterwards finds its PID absent. Keep scene 640x360, DMA/async off and cache 23.

The latest failure records 12,465,848,320 native allocated bytes against a
12,713,115,648-byte recommendation, a 235.81 MiB margin. The failed native chunk
retains 96 of 160 CPU notes, including a compute dispatch and indirect/final draw
candidates. This does not identify the exact GPU instruction/resource responsible.
The visual journal retains 2,119 records, including 706 presentations with API
success and the native error. Pixel capture is off, so correct pixels remain
unverified. The line-buffered CSV has 693 complete rows; its clock origin differs
from the supervisor and visual journal.

Two active audio ports now have eleven five-second windows each. They deliver
about **23,912 input frames/s**, 49.82% of the requested 48 kHz; submission gaps
reach 74.33 ms. Output-lock waits peak at 8 microseconds, and no queue/provider
failure is recorded. This identifies a delivery problem rather than proving a
hardware underrun from zero input-queue samples. The old two-block queue cap can
feed only 512 input frames before waiting. An actual-source fake-device test with
a 1024-frame quantum reproduces half-rate delivery: **471 blocks in five seconds**.
The corrected adapter queries actual device frames/frequency, sizes a bounded
queue for a complete quantum plus a guest block, and produces **939 blocks** in
the same deterministic test without fabricated PCM. The native device's quantum
was not recorded in the old run; new windows include it. At this checkpoint,
native improvement was still unverified; the 07:23Z comparison above supersedes
that boundary. See [Audio diagnostics](AUDIO_DIAGNOSTICS.md).

Staging inventory is identical in both native comparisons: 3,072 MiB dedicated
upload, 140 MiB dedicated device-local and rings of 128/160 MiB, **3,500 MiB total**.
This is a subset of tracked/native allocations, not memory to add to them.
Zero held bytes do not prove buffers are reusable: outstanding ticks were not
counted. The old inventory was published only on frame ticks, so early all-zero
arrays mean unknown accounting. New snapshots include age, counts and completed,
unheld reusable bytes, with publishing after prewarm/new allocations too.

Source review finds a retained-cache inefficiency: completed sufficiently large
buffers were rejected when more than roughly 25% above a new request. The new
checked staging overlay selects the smallest sufficient buffer across size
classes, preserving memory type, deferred ownership and actual GPU-tick guards.
It frees no retained buffer and changes no import or fence behavior. Actual-source
CPU fixtures reproduce 288 MiB/five buffers for a sequential synthetic workload;
the adapted path retains 128 MiB/one buffer and preserves pending/held/type/best-fit
checks. At this checkpoint, these were CPU results rather than measured game
savings. The native comparison above now exercises reuse; do not enable trimming.

The newest counters also show 69,033 missing-pipeline draw requests out of 198,029
requests, with 195 failed pipeline creations, 180 PerVertexKHR log messages, 271
SRT-walk errors and 10,865 clamps. Asynchronous totals are not per-frame results
or a partition of every draw outcome. Reduced scene decisions are recorded for
21,722 of 21,756 scene decisions. General shader support remains unresolved.

The ARM64 library and full JIT/interpreter contracts pass with the new corrections.
No tool-driven GPU run was launched. The later owner-supervised comparison above
establishes the real quantum, delivery improvement and staging reuse while still
ending at the native memory boundary.

### Earlier visual evidence, copy recording and upstream watch

The owner now additionally reports choppy audio. [Audio delivery tracing](AUDIO_DIAGNOSTICS.md)
is enabled by default in the combined launcher and records private periodic
metadata in stderr/`audio.jsonl`, summarized by the visual report. It measures
actual submission gaps, input queue observations, deadline and queue/mutex waits;
it does not label input-queue zero as a hardware underrun. PCM handling and queue
policy remain. The old game log records only initial accepted output batches;
that initial run did not yet identify the delivery problem described above.

The latest code adds [bounded visual diagnostics](VISUAL_DIAGNOSTICS.md), enabled
as CPU metadata by the combined launcher. Optional `SOTC_VISUAL_CAPTURE=1` adds
completed game-frame readbacks, at most one outstanding, every five seconds and
24 attempts, with size/headroom/staleness guards. The offline report distinguishes
API results from pixels, dark/unchanged preview measurements and draw-skip/memory
totals. Complete timeline lines and the optional flip CSV are flushed; completed
capture records survive timeline rotation. Rendering stays 640x360. The native
ARM64 build, CPU evidence fixtures and full contracts pass. Later owner runs above
exercise metadata and reuse accounting; optional pixel capture and an overhead
comparison remain untested. Native memory exhaustion remains the current boundary.

[bmy/bbport-mac](DEPENDENCY_WATCH.md) was reviewed at its actual current default
branch `macos-native-0.6`. Its whole renderer/driver architecture is not imported.
Its specific 4096-region copy batching correction is adapted to our two production
copy paths, with capacity/alignment checks and actual-source CPU regressions. The
pinned methods reproduce the recording-size failure; the adapted ones preserve
copy data/order and barriers through larger cases. This prevents a separate CPU
assertion, not the native out-of-memory failure below. Pins/cache version remain.

The owner also requested ongoing dependency review; a daily public-source watch
is configured, and its source list/baseline is in [Dependency watch](DEPENDENCY_WATCH.md).
[MoltenVK #2858 and SPIRV-Cross #2699](PERVERTEX_REVIEW.md) are open drafts, not
production replacements. The compiler proposal builds separately and converts
four original synthetic per-vertex cases using an actual exported producer layout;
default and incomplete-layout paths correctly reject them. Native GPU linking,
topology/bindings, SotC shaders, pixels and runtime memory are not validated. Keep
the current renderer intact while designing any future isolated integration.

### Earlier confirmed native memory failure and reduced scene draws

The newest owner-run preset-4 capture starts the main loop at about 1.46 seconds,
detects a DualSense, and self-exits after **83.45 seconds** with confirmed child
and supervisor exit 23. The native prerequisite synthetic draw/readback passes.
There is no duration deadline, GPU-watchdog stop, import stop or shader-metadata
failure. The new Metal completion-error path is now exercised on the real device:

- The NSError domain is `MTLCommandBufferErrorDomain`, code 8, with description
  `Insufficient Memory` and an explicit IOGPU out-of-memory callback reason.
- At failure `currentAllocatedSize` is 12,508,184,576 bytes (**11.65 GiB**), versus
  `recommendedMaxWorkingSetSize` 12,713,115,648 bytes (**11.84 GiB**). The counters
  are about 195.44 MiB apart (98.39% of the recommendation). The recommendation is
  not a hard allocation limit; these are device/process counters, not total macOS
  RAM pressure or a complete resource inventory.
- The failing native chunk is retained and matched to a submitted command-buffer
  generation. Its complete twelve-note CPU tail contains two guest compute dispatch
  candidates, with group counts 1x1x1 and 4x1x1, plus bindings/barriers. One shader
  identity also appeared in the preceding run's erroneous native chunk; it is a
  repeated lead, not proof of the shader or exact GPU instruction that exhausted
  memory. A small dispatch does not establish a small total resource working set.
- CPU-only replay of both recorded compute modules converts successfully through
  the installed SPIRV-Cross MSL path. This checks conversion, not execution, GPU
  memory accesses, native pipeline acceptance or which resource caused the error.
- The error path terminates before the later watchdog/cancellation sequence. The
  supervisor confirms child exit, and a later targeted PID check finds it absent.
  This validates failure handling, not a repair for memory exhaustion. Its shorter
  elapsed time is not directly comparable to older runs that waited for a watchdog.

The scene configuration remains **640x360**, half targets 320x180 and UI 1920x1080;
DMA and async submission remain off. Unlike the preceding 107.54-second capture,
three later statistics windows now record reduced scene draws: 106/106, 406/407
and 499/501 per frame. This is evidence of the reduced scene path being used for
those draws, not complete image correctness. Other windows still record no scene
draws. There is no new owner visual report to validate scene contents.

Sampled tracked Vulkan allocations peak at 4,793 MiB and images at 992 MiB. The
final sample therefore has roughly 3,801 MiB beyond the counted images. The guest
backing pool is 6,592 MiB, imported through two overlapping windows whose declared
sizes sum to 7,104 MiB (512 MiB overlap). These figures include aliases and distinct
sampling times; do not add them as a physical-memory/RSS measurement. Reducing
scene pixels cannot shrink all these pools, temporary buffers, native resources
or compiled pipelines. Allocation-class accounting and safe reuse/lifetime are
now the main memory investigation; the exact allocation causing exhaustion remains
unknown. Keep the split imports: upstream documents incorrect MoltenVK offsets
past 4 GiB. Large staging buffers are intentionally retained upstream because
premature destruction caused later argument-buffer references/device loss; do not
enable `BB_STAGING_TRIM_LARGE` or disable residency as an untested remedy.

Rendering failures remain: 230 PerVertexKHR messages, 262 failed graphics pipelines,
271 SRT-walk failures and 19,605 buffer-size clamps. These are message counts across
different progress windows, not comparable unique-shader or error-rate reductions.
Unsupported-Phi messages and undeclared shared-U32 errors are absent. The frame CSV
retains 1,035 complete rows through about 66.30 seconds relative to its first row;
a partial final row and buffered loss prevent treating that timestamp as exit time.
No new GPU test was launched by the restricted tool process for this analysis.

### Earlier native stalls after the lookup/SRT update

Two subsequent owner-run tests stop on GPU progress, earlier than the preceding
156.90-second shader-metadata boundary. Both use preset 4, DMA off, async submission
off, JIT/statistics/journaling on and no duration deadline. The owner's report is
less visual progress. Do not describe these runs as rendering validation.

| Capture order | Elapsed | Observed stop | Child/supervisor status |
|---|---:|---|---|
| Previous metadata boundary | 156.90 s | Missing shader metadata | 23 / 23 |
| First new attempt | 16.61 s | GPU-progress watchdog | unknown / 1 |
| Latest attempt | 107.54 s | GPU-progress watchdog, with native Metal error evidence | unknown / 1 |

The prerequisite synthetic Metal draw/readback passes in the owner's Terminal.
Both game attempts detect a DualSense and enter the main loop (about 1.41/1.55 s).
The first attempt retains a pending native chunk with commit returned but neither
scheduled nor completion callback; the preceding chunk completed successfully.
The latest attempt instead records the preceding chunk's completion callback with
**status 5, error 8**. The installed Apple SDK defines these as
`MTLCommandBufferStatusError` and `MTLCommandBufferErrorOutOfMemory`: insufficient
memory to execute that command buffer. The old trace did not retain the NSError
domain/description. This is concrete native error evidence, not a measurement of
total macOS memory pressure or proof of which resource exhausted it.

The latest watchdog's awaited submission is absent from the retained journal (or
had not reached submit); it cannot be reconstructed from the preceding shader's
identity. The latest log still has 281 PerVertexKHR messages, 323 failed graphics
pipelines, 271 SRT-walk failures and 12,110 buffer clamps. Unsupported-Phi diagnostic
messages and missing shared-U32 errors are absent, but the run stops at a different
boundary. There is no new required metadata failure or snapshot. Do not claim the
previous missing shader is repaired based on this earlier exit.

The scene setting remains **640x360** (half targets 320x180, UI 1920x1080), but all
sixteen latest statistics samples have zero reduced-size scene draws. The earlier
metadata run has nonzero reduced scene draws. A preset selection therefore does
not prove those frames used the reduced scene path. Sampled Vulkan allocation
maxima are 5,167 MiB in the latest run versus 3,393 MiB in the previous metadata
run, while image maxima are 709 versus 1,988 MiB. These are different progress
windows and exclude other native/system allocations; neither establishes a total
RAM trend. CSV `alloc_mb` is **new device allocation per frame**, not live/resident
memory. Do not interpret its maximum as a process memory peak.

Both new supervisors observe cooperative cancellation quickly but cannot collect
child exit within the existing allowance. A later targeted existence check finds
both child PIDs absent; exit codes/times remain unconfirmed. No further GPU test
was launched from the restricted tool process.

A new checked `sotc-metal-command-error` overlay fixes independent error handling:

- The native completion handler always checks `MTLCommandBufferStatusError`, even
  with the optional submission journal disabled. Previously error status was only
  stored in the journal; the application continued until a later stall report.
- `runtime/src/gpu_command_failure.h` prints the real error domain, code,
  description, label, command-buffer identity, recording generation, and the
  device's actual allocation/recommended-working-set counters, then self-exits 23.
  It does not retry GPU work, fabricate successful progress or signal fake fences.
- With tracing enabled, the fatal report includes the bounded originating
  submission/open-recording candidate tail. Lookup matches both handle and
  generation; missing/evicted records remain explicit. This is not a per-command
  GPU execution marker.

Native ARM64 rebuilding, 23 digest/migration builder tests, full JIT/interpreter
contracts and CPU-only completion/journal fixtures pass. The latter check failure
with tracing on/off, unknown error details, success continuation, pre-submit
failure, recorded submissions and reused handles. No native GPU run validates this
new stop behavior or fixes the memory exhaustion. Pins/cache version 23 and the
640x360 configuration are unchanged. Fresh builds still need the missing external
glslang prerequisite.

CPU replay of the latest capture's 1,059 modules converts 778 (including four exact
vertex-zero rewrites); all 281 failures remain unsupported PerVertexKHR fragment
inputs. Private reports stay under `work/`. This confirms a persistent compiler
limitation, not driver or image correctness. There is no controlled bisect tying
the new stalls to one lookup/Phi change. Continue with memory/resource residency,
the exact per-vertex input path and actual scene eligibility before another native
comparison; preserve the earlier metadata evidence separately.

### Previous shader-metadata boundary and CPU-only continuation

The newest owner-run preset-4 capture reaches the main loop at about 1.47 seconds,
detects a DualSense, and stops after about 156.90 seconds with confirmed child and
supervisor exit 23: `SearchBinaryInfo` cannot find shader metadata. No duration
deadline, unresolved-import stop, missing shared-U32 declaration or GPU-watchdog
stop appears. This validates continuation beyond `sched_yield`, not stable gameplay
or correct rendering. The failing shader's role/address/backtrace is absent from
the old assertion; do not infer that it ran on a preparation worker.

The log still has 474 PerVertexKHR messages, 561 graphics-pipeline failures, 805
SRT-walk failures and 797 unsupported-Phi messages. Counts describe messages,
not unique modules or frames. Compressed-format storage-image failures and many
buffer-size clamps remain. The owner's visual report remains defective 3D animation.

Implemented after that capture, without GPU execution:

- `runtime/src/gpu_shader_metadata.h` reads actual `BinaryInfo` through bounded
  copies. It promotes prologue arithmetic before addition/multiplication, preserves
  the original 64 KiB candidate scan, supports unaligned trailers, validates full
  code ranges and never invents a length/hash. `gpu_shader_lookup.h` uses the pinned
  runtime's checked backing-memory hooks, so metadata reads do not themselves fault
  through GPU page tracking. The rest of shader compilation still uses the real code.
- Missing speculative graphics, geometry-copy or prepared-pipeline metadata marks
  preparation unavailable and retries on the owning GPU thread. A required missing
  program still stops with exit 23. It now prints role, thread, header, mapped length,
  hint and a native backtrace, and writes at most 64 KiB plus context under the private
  capture's `shader-metadata-failure/`. Existing evidence is preserved. This is
  diagnosis/retry behavior, not proof that the recorded required shader is valid.
- The portable SRT walker accepts a Phi only when every incoming edge emits exactly
  the same supported offset program. Constants, uniform user data, matching
  arithmetic and nested equal values retain their actual offsets. Divergent edges,
  lane reads, cycles, excessive depth and exponential expansion stay dynamic.
  Gameplay DMA remains disabled; synthetic DMA=1 checks initialize no GPU.
- Shader binary version 23 invalidates stale SPIR-V and corresponding metadata.
  Digest-checked migration includes the previous compatibility overlay (version 22).
- `tools/replay_shader_cache.py` compiles a standalone CPU converter using the actual
  exact-only compatibility policy and installed SPIRV-Cross. Each private module has
  an isolated ten-second conversion budget and input-size/header/instruction checks.
  It writes only a private JSON report, not converted game shaders. It does not link
  the renderer or Metal/Vulkan frameworks. Synthetic tests check exact-only policy,
  malformed files and dependency absence.

CPU replay of all 1,609 modules in the latest capture converts 1,134, including six
exact vertex-zero rewrites. The other 475 are fragment modules, all rejected with
`PerVertexKHR decoration is not supported in MSL.` The converted set has 591 vertex,
351 compute and 192 fragment modules. This standalone MSL 3.1 conversion omits
pipeline resource bindings, specialization and tessellation variants; it does not
prove MoltenVK pipeline creation, Metal compilation or pixels. Do not enable
approximate derivative reconstruction to hide the remaining per-vertex limitation.

The incremental ARM64 library rebuild, twelve actual SRT/shader-info IR cases in
both synthetic DMA modes, two metadata/lookup tests, wave64/exact-input regression,
15 auxiliary interface modules, pipeline-validity guards, 22 builder tests and all
46 host imports pass. Full native JIT/interpreter contracts also pass. The fresh
build's missing external glslang prerequisite is still unresolved; incremental
builds reuse existing generated assets. No new native game run validates these
latest changes. The tool's supervised prerequisite check still enumerates zero
Metal devices and exits 24 before a draw; do not bypass that restriction.

The owner first allowed a 320-pixel comparison, then explicitly restored the
**640x360 scene configuration**. No 320-wide scene change was applied. Preset 4,
DMA off and async submission off remain the native configuration; the existing
320x180 half-resolution targets are intermediate buffers, not a 320-wide scene.
Output/UI remain 1920x1080. Actual reduction eligibility still depends on each draw.
CPU-only conversion needs no pixel-resolution change.

For further autonomous analysis, use an existing private capture:

```sh
python3 tools/replay_shader_cache.py work/your-capture/cache/your-profile \
  --first-vertex-provoking --output work/shader-replay.json
python3 tools/check_shader_memory.py
python3 tools/check_shader_compat.py
```

Use `--first-vertex-provoking` only when the target device fixes the provoking
vertex to first (the reviewed native device lacks `VK_EXT_provoking_vertex`).
Reports and any failure snapshots remain private. Priorities: inspect a real
metadata failure snapshot if required lookup still fails; resolve general per-vertex
inputs exactly; then investigate compressed storage images, unresolved dynamic SRT
reads, clamps and the visible scene. A new GPU run is needed to measure pixels.

### Earlier yield and wave64 correction

**Newest native boundary and implemented corrections:** the owner-run preset-4
capture now confirms no duration deadline in both supervision layers. It detects a
DualSense, starts the main loop at about 1.37 seconds, and exits after about 160.93
seconds at the unresolved POSIX `sched_yield` import. Child and supervisor both
report exit 20; no GPU-progress watchdog stop is recorded. This establishes a new
import boundary, not stable gameplay. Rendering errors persist: 310 PerVertexKHR
messages, 354 graphics-pipeline creation failures and four undeclared shared-U32
compute failures. These are message counts, not unique shaders or frames.

The scoped POSIX `sched_yield` exports now call the native scheduler, preserve
guest errno on success and translate host failure to POSIX -1/guest errno. Guest
ABI checks in both JIT and interpreter cover both observed library scopes, thread
identity and rejection of data/unrelated/disabled bindings. Full native contracts
pass. Game continuation beyond this newly implemented import remains untested.

The checked local shader-compatibility overlay makes two renderer corrections:

- Wave64 ballot lowering runs after shared-memory simplification and previously
  introduced a U64 scratch view alongside its U32 stores on a device without
  explicit workgroup aliasing. It now loads the aligned pair as two U32 values and
  packs the same low/high halves, keeping the scratch reservation and barriers.
  The original pass reproduces missing shared-U32 MSL declarations in all nine
  synthetic compute modules; the adapted pass produces none. Checks exercise
  64/128/256-thread groups and three initial LDS sizes, including alignment padding.
- Before MoltenVK module creation, raw per-vertex inputs used exclusively at
  vertex zero use the existing upstream rewriter's exact Flat case only when the
  device fixes the provoking vertex to the first vertex. Configurable provoking
  vertex support disables this rewrite. Both new and cached module paths apply
  the policy. Ordinary modules and inputs reading
  vertices one/two or mixed vertices remain unchanged and explicitly unsupported;
  derivative reconstruction is not admitted into this compatibility policy.

Shader binary cache version 22 invalidates the old compute output. The original
three-backport, submission-journal and pipeline-validity states migrate through
digest-checked patches; partial and foreign states remain rejected. The new policy
lives in `runtime/src/gpu_msl_compat.h`; regression driver:
`python3 tools/check_shader_compat.py`. It uses synthetic IR/SPIR-V and no GPU or
game data. Conversion tests do not establish Metal driver acceptance or pixels.

The incremental ARM64 library rebuild, auxiliary emitter regression, actual
pipeline-validity guards, 21 offline builder tests and 46-symbol host-link check
pass. The full builder's prerequisite check currently cannot find the external
glslang executable referenced by the older configuration; the incremental build
reused the existing generated shader assets and succeeded. Restore a real glslang
compiler (or set `SOTC_GLSLANG`) before a fresh dependency/configuration build.
No real GPU experiment was launched for these corrections. SRT/Phi, general
per-vertex inputs, typed copies and correct scene rendering remain open.

### Earlier duration implementation and native capture

This checkpoint supersedes the older black-screen and stopping-boundary descriptions
below. The owner now **sees the 3D animation, with defects**. Correct scene rendering,
introduction synchronization, playable gameplay and stability remain unverified.

The newest reviewed native capture selects preset 4 (640x360 scene configuration,
320x180 half targets, 1920x1080 output), DMA off, async submission off, JIT and
statistics/journaling on. Initialization completes at about 1.10 seconds and the
main loop starts at about 1.62 seconds. A DualSense connects during the run, followed
by sixteen further statistics windows. There is no GPU-watchdog message, import
stop or CPU-fault report before the 120-second wall deadline. The supervisor then
fails to confirm cooperative child exit and ends after about 122.02 seconds with
unknown child status and exit 1, despite a cancellation notice after about 1.81 ms.
A later targeted PID lookup finds the process absent; its exit code/time remain
unknown. A preceding preset-4 run logs window closure and a confirmed natural
child/supervisor exit 0 after about 101.27 seconds.

The newest capture records 199 PerVertexKHR messages, 222 graphics-pipeline creation
failures, four undeclared `shared_mem_u32` compute failures, 271 SRT-walk failures,
263 Phi offset failures and 25044 buffer-size clamps. The final Metal sample still
reports 1696 draws skipped for missing pipelines in its interval. These establish
omitted rendering work, without isolating the owner's visible defects to one cause.
All twenty-two statistics windows print zero scene/reduced draws per frame; these
are rounded averages. `BeginRenderingFull` recognizes scene start using at least
five color targets and an eligible depth target. Preset selection alone does not
prove SotC's actual 3D draws were downscaled. Do not remove this condition blindly
or claim total GPU/memory pressure was reduced. Sampled Vulkan allocations/images
peak at 3795/586 MiB; imported guest memory remains unchanged and macOS pressure
is not measured. The CSV retains 1237 complete rows through about 113.53 seconds
relative to its first row, plus a partial row. Buffered loss prevents identifying
the final presentation time. No pending-submission dump appears because the
renderer watchdog did not trigger; no exact unfinished GPU command is identified.

**No-deadline game duration implemented at the owner's request:**
`SOTC_DIAGNOSTIC_SECONDS=0` disables the supervisor wall deadline and the native
guest alarm. The combined launcher now defaults to zero and explicitly passes the
same setting to both layers. The GPU-progress watchdog, DMA guard, cooperative
cancellation and manual window-close/Terminal interruption remain in place. Explicit
1..1800-second runs remain available; headless probes still default to 30 seconds.
The independent ten-second Metal precheck and 60-second renderer initialization
budget remain unchanged. No duration setting protects against a kernel/driver hang.

Relevant files are `tools/run_gpu_diagnostic.py`, `runtime/src/probe_entry_arm64.c`
and `packaging/test-arm64.sh`. Synthetic tests cover no-deadline natural exit,
GPU-stall cancellation, manual cancellation, launcher propagation and the native
guest alarm in JIT/interpreter. No real GPU experiment was launched for this change.
Validation: `bash packaging/test-arm64.sh --contracts-test` completed natively in
ARM64 with JIT/interpreter contracts passing, including all six duration/launcher
tests and twenty-two supervisor tests. One optional real Vulkan-loader test was
skipped because the isolated GPU dependencies were not present in the validation
checkout. Shell syntax, staged-source audit and patch application checks also pass.
The historical native capture above still used the old 120-second limit; it does
not validate the new mode or resolve the unconfirmed child exit.

After applying and validating these sources in the owner's working copy, the
no-deadline command preserves the preceding renderer settings:

```sh
SOTC_MEMORY_BACKING=file BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 \
  SOTC_DIAGNOSTIC_SECONDS=0 BB_JIT=1 BB_FRAME_STATS=1 BB_UPSCALE_PRESET=4 \
  bash packaging/test-arm64.sh --graphics-game-test
```

Close the window or press Ctrl+C to stop; inspect the log before another run.
Confirm the previous instance has closed first. Do not launch a real GPU experiment
from the restricted tool process. Preset 4 remains a requested scene configuration,
not proof that SotC's actual 3D draws are reduced.

## Goal and current evidence

The goal is native Apple Silicon execution of the PS4 game through bbcpu and the
pinned renderer. On Apple M1, the owner has visually confirmed game logos/menu and
heard the opening introduction audio. This confirms audible game output, but not
full audio coverage, audiovisual synchronization or correct introduction video.
Entering a playable 3D scene, gameplay stability, save persistence and gameplay FPS
remain unverified; there is no packaged playable application.

Supported input is the owner's readable CUSA08809 update 1.01 dump. ARM64 macOS on
Apple M1 is the observed host; the isolated GPU build targets macOS 15 or later.
Other game editions and hardware are unverified.

An earlier native run completes initialization at about 0.94 seconds and enters
the main loop at about 1.38 seconds. It continues past the save/dialog import stops,
then ends at the 60-second diagnostic budget. There is no unresolved-import stop or
CPU execution fault recorded in that interval. GPU activity continues after the
profile-save request. A timeout does not establish stable gameplay or rule out a
stall. The owner's observation confirms logos/menu and opening audio, not correct
rendering of an in-game scene. Draw/dispatch counters are not gameplay FPS.

The mount/cleanup/metadata error batch now handles eight related SaveData calls with
NOT_INITIALIZED and untouched outputs. Full contracts and comparisons against the
supplied original module pass in both CPU modes. The native game handles this mount
failure and attempts a save dialog. No save filesystem was added.

The two remaining save-dialog startup exports are now scoped: Initialize returns
NOT_SYSTEM_INITIALIZED because CommonDialog is unavailable, and Open validates its
base header/address cookie/reserved bytes and outer size before reporting no instance.
The initializer policy and its injected dependency-error comparison are distinct from
Open's original-module validation comparison. The subsequent native run continues
until timeout. After the host incident below, the owner authorized gradual GPU
testing. This tool process still exposes no Metal device, so real comparisons need
the owner's native Terminal. CPU/synthetic contracts do not load the real renderer.

The renderer still skips pipelines because of PerVertexKHR-to-MSL conversion (78
reported errors in an earlier run), and has SRT Phi and compressed-texture usage errors.
The isolated fragment diagnostic reproduces the compiler limitation without game data
or a GPU. IT_GET_LOD_STATS also remains unimplemented.

Physical SDL3 gamepad input now complements the keyboard fallback. See
[DualShock input](guide/building.md#dualshock-input) for connection and logging.
Guest ABI tests cover the entire stick/trigger domain, buttons, two touch identities,
hotplug, output errors and synthetic callback dispatch in both CPU modes. The latest
native 120-second-budget low-resolution run detects a DualSense, without a recorded
disconnection. The preceding short repeats have no physical connection logged.
An earlier complete summary records 778 physical samples,
387 accepted LED requests and 388 accepted rumble requests without disconnection;
the timeout run has no final input summary. These counters do not identify the
connection transport or prove the physical output effect.

## Host watchdog incident

The owner reported a macOS kernel panic with no watchdogd check-ins for 90 seconds.
The incident-associated private graphics log ends during renderer initialization, after
importing approximately 6.4 GiB of guest memory and preparing rendering workers.
It does not reach the renderer-ready line, linked-module initialization or guest
entry. Its selected dynamic-memory setting is 1. The preceding complete graphics
log uses the former configuration, reaches the main loop, reports a GPU submission
unfinished for more than ten seconds and then exits at the 60-second diagnostic
timeout. Neither log identifies the cause of the kernel panic. The earlier GPU
stall means the new dynamic-read setting cannot be the only suspected path.

The unvalidated default-on dynamic-read experiment is withdrawn. The loader now
sets `BB_DIRECT_MEMORY_ACCESS=0` and rejects every other explicitly supplied value
before `dlopen`, including load-only and renderer-only modes. The launch script
rejects those values before Vulkan/Metal prechecks. The unvalidated 300-second
default is withdrawn; the owner requested a 120-second combined budget after the
incident. Explicit 1..1800-second budgets remain supported, but a
user-space alarm cannot guarantee recovery from a kernel/driver hang. Do not
increase timeouts as a remedy or treat DMA off as evidence of safe GPU execution.

The initial pause is superseded by the owner's explicit request to resume gradually.
Do not repeat the DMA-enabled incident configuration or run an automatic sequence.
The normalized evidence above is sufficient for the public handoff; panic reports,
device identifiers, addresses and execution logs remain private.

## Gradual GPU testing

### Earlier compute stall in the newest capture

The newest owner-run capture has a 30-second wall budget, DMA off, async submission
off and JIT/statistics/journaling on, but **no `BB_UPSCALE_PRESET=4` override**.
Output is 1920x1080 and no reduced-scene transition is logged. Do not treat this
as the preceding 640x360 comparison. Initialization finishes at about 1.13 seconds
and the main loop starts at about 1.70 seconds. Only keyboard fallback is recorded;
there is no physical-controller connection in this capture.

The GPU watchdog reports unfinished submitted work after ten seconds. The supervisor
finishes after about 19.49 seconds with **unknown child status and exit 1**: the
cancellation notice arrives about 16.68 ms after the request, but child exit is not
observed within the cooperative allowance. A later targeted PID lookup returns
ESRCH. This establishes later absence, not a confirmed exit code or exit time.
The intermediate preceding log ends during GPU initialization without a supervisor
footer and cannot establish a completed comparison.

The awaited submission's queue call returned success. Its single Metal chunk has
commit return observed but neither scheduled nor completion callback observed;
the preceding chunk completed with status 4, error 0. There are no explicit
wait-semaphore entries, missing buffers, missing notes or missing Metal events.
The pending batch has 164 metadata notes with a retained 96-note tail: eight guest
compute dispatch candidates, one three-vertex draw candidate, transfers and helper
passes. Six guest dispatch notes use a 120x68x1 group grid. The final retained
compute candidate also appears in 31 buffer-size clamp warnings. This narrows
resource-provenance and compute investigation, but identifies neither the command
that executed nor a proven bad descriptor, deadlock or driver failure. The zero
status/error fields remain unobserved-callback defaults.

There are 50 SRT-walk failures, 42 Phi offset failures and 153 buffer-size clamps.
No PerVertexKHR, undeclared shared-memory or integer-blit failure appears before
this earlier stop; their absence does not validate the later 3D paths. No unresolved
import or CPU-fault report appears. The CSV is empty, so it supplies no presentation
count or timing. This changed configuration and shorter run cannot establish the
visual benefit or regression of the pipeline-validity correction below. Keep the
known low-resolution command in the tracing section for the next single supervised
comparison; the 120-second default and protective watchdog remain unchanged.

### Black scene and native submission evidence

**Current result:** the owner reports a black screen where the 3D animation should
appear. The preceding complete low-resolution native run uses the same DMA-off, async-off,
JIT/statistics-on settings with the submission journal enabled and a 120-second
budget. It detects a DualSense, finishes initialization at about 0.94 seconds and
starts the main loop at about 1.38 seconds. Scene raster remains 640x360, half-size
targets 320x180 and UI/output 1920x1080. The GPU watchdog stops it after about
114.91 seconds; child exit 21 is confirmed and supervisor exit 31 preserves the
stall reason. No import stop, CPU-fault report or cleanup error is recorded.

The native journal now identifies the awaited batch's recorded work. Its submit
call returned success, with one command buffer and no explicit wait-semaphore
entries. Its sole Metal chunk has commit return observed but no scheduled or
completion callback observed. The immediately preceding empty batch's Metal chunk
has both callbacks and completion status 4, error 0. The awaited chunk's zero
status/error fields are defaults because no completion callback arrived, not a
polled Metal status or evidence that no driver error occurred. These observations
do not prove which command started or whether the stall is within that batch,
earlier queue dependencies or the driver.

The awaited batch has 28632 metadata notes, not that many draws. Only the 96-note
tail is retained. It includes sixteen indexed indirect draw candidates from two
vertex/fragment shader pairs, with maximum draw count 1 and stride 20. The actual
indirect index/instance counts were not read. Both pairs also appear in failed
graphics-pipeline messages immediately following PerVertexKHR conversion errors.
This links candidate shader pairs to a concrete compiler limitation, but pairs
can share several pipeline variants: it does not prove which variant was bound
or which GPU instruction blocked. The awaited record has no missing buffers;
missing-note and missing-Metal-event counts are zero. Cumulative ring evictions
do not reconstruct the omitted prefix or constitute per-command GPU breadcrumbs.

The log records 118 PerVertexKHR messages, 133 graphics-pipeline creation failures,
four undeclared `shared_mem_u32` compute failures, two integer-format Metal blit
failures, 271 SRT-walk failures, 8094 clamps and 267 incomplete GPU-mapping warnings.
The fragment-interface CPU fixture still reproduces the installed SPIRV-Cross
limitation (expected exit 2); it is not a passing shader-support test. Missing
pipelines and omitted copies are rendering defects consistent with missing scene
content, but the owner's black image has not been isolated to one of them.
Sampled Vulkan allocations/images peak at 5304/1318 MiB, not measured macOS RAM.
The CSV retains 438 complete rows through about 60.53 seconds relative to its
first row, plus a partial row. Buffering prevents dating the last presentation.

Source review exposes a separate validity defect worth correcting before another
capture. `GetGraphicsPipeline` checked the handle after first creation and in the
prepared path, but its ordinary cache-hit path returned the object unconditionally.
A rejected pipeline could therefore be offered again with an empty handle.
`GraphicsFromVulkan` also accepted a nonnull object with an empty handle, allowing
different native Metal entries to share the null Vulkan lookup key. The local
validity overlay checks both paths. This consistently rejects unsupported state;
it does not fabricate a successful pipeline or bypass shader semantics.

The regression compiles the actual source guard blocks with synthetic handles:
the original guards fail both checks, the adapted guards reject null/empty entries
and preserve valid ones. The ARM64 library rebuild, fifteen auxiliary SPIR-V cases,
the new CPU guard test, twenty offline builder tests and full JIT/interpreter
contracts pass. Pristine sources,
the exact original three-backport state and the preceding journal overlay can
migrate; mixed/unknown sources remain rejected. No GPU run of this correction was
launched. Native image verification, PerVertexKHR support, compute shared-memory
emission, typed blits and the precise stall cause remain open.

**Earlier uninstrumented low-resolution stall:** the owner's run uses a 120-second budget
with DMA off, async submission off, JIT/statistics on and scene preset 4. Initialization
completes at about 0.93 seconds; a DualSense is detected before the main loop starts
at about 1.37 seconds. There is no recorded physical disconnection or input-backend
failure. This establishes detection in this run, not transport, physical output
effects, completed hotplug in the preceding run or absence of every input issue.
Scene raster remains 640x360, half-resolution targets 320x180 and UI/output 1920x1080;
later statistics windows record reduced scene draws.

The renderer reports submitted GPU work unfinished for ten seconds, with the last
completed timeline value immediately before the awaited value and later recording
work already present. The supervisor stops on that message before the 120-second
wall deadline: confirmed child exit 21, supervisor exit 31, elapsed about 112.05
seconds. The cancellation notice is observed about 5.24 ms after the request and
there is no cleanup error, import stop or CPU-fault report. There is no recorded
device-lost error. The pinned `Rasterizer::GpuSignalLoop` waits on the work semaphore;
`GpuWatchdog` distinguishes this branch from a tick not yet submitted. This does
not identify the command, shader, dependency or driver behavior responsible, prove
a hardware hang, or report a new host kernel panic. Do not fake label completion,
disable the watchdog or extend the budget as a remedy.

The last statistics window reports a worst flip interval of about 22.17 seconds
and 355 shader/pipeline compiles totaling about 12.75 seconds. These are aggregate
observations, not proof that compilation caused the stalled submission. Sampled
Vulkan allocations rise from 4307 to 5740 MiB, and images from 490 to 1418 MiB;
imported guest memory remains 6592 MiB. The Linux-style zero RSS/VRAM/GTT fields do
not measure macOS pressure. Low pixel count has not eliminated the progress failure.

Both typed Metal blit failures persist, along with 161 PerVertexKHR messages,
263 Phi offset messages, SRT-walk failures, 9567 buffer clamps and 399 incomplete
GPU-mapping warnings. Message counts are not distinct shader/resource counts or
causal evidence. The last Metal sample also reports draws skipped for missing
pipelines. The CSV retains 519 complete rows through about 38.03 seconds relative
to its first row, plus a partial row; cancellation buffering prevents treating
this as the final presentation time or explaining the remainder of the run.

The source/log review did not change shader semantics, dependencies or GPU settings.
Its follow-up submission instrumentation is described below. The typed-copy fixes
remain pending. Keep the protective stop and DMA-off setting. Earlier confirmed
and unknown stop results remain separate historical observations.

### Pending submission tracing

The historical log identifies the awaited timeline value and its predecessor, but
not the draw, dispatch or transfer inside that batch. It explicitly reports that
`VK_AMD_buffer_marker` is unavailable, so the existing GPU breadcrumbs are disabled.
The private cache contains compiled shaders and pipelines, not their submission
membership or execution order. A recently created cache file is not proof that
its shader ran, and no command list can be recovered retrospectively from it.
Four compute compilation failures also reference undeclared `shared_mem_u32`;
they establish a compiler problem, not the cause of the awaited submission.

A local checked overlay now connects the scheduler's logical submission to the
Metal command buffers it commits. `runtime/src/gpu_submission_trace.h` owns the
bounded, thread-safe CPU journal. The combined launcher enables
`SOTC_GPU_SUBMIT_TRACE=1` by default; 0 disables it. Direct library users must opt in.
The supervisor logs the setting. The journal retains:

- Logical scheduler/tick, wait and signal semaphore values, and whether the queue
  submission call returned, with its result. A prepared entry is not a submitted
  or completed GPU operation.
- Vulkan command API names and guest draw/dispatch shader identities and counts
  from `RecordCrumb`. These are CPU-recorded candidates. Host helper passes without
  a guest crumb have API names but may lack shader identities. Indirect argument
  contents and resource memory are never read by this journal.
- Actual Metal chunk commit-return, scheduled and completion-callback observations,
  including partial `CommitChunk` buffers, status and error codes. Scheduled is not
  proof that an individual shader started. A completion callback is per Metal
  command buffer, not per draw. Recording generations prevent handle-reuse confusion.

There are at most 256 open buffers, 512 submission records, 1024 Metal chunk records
and a 96-note tail per recorded buffer/batch. `notes` includes shader metadata and
API calls; it is not a GPU instruction/draw count. Truncation, missing buffers and
evictions remain visible. On the ten-second watchdog callback, the awaited and
last-completed batch snapshots are printed and flushed **before** the existing
watchdog prefix; otherwise the supervisor could exit before the details arrive.
No new GPU marker writes, guest-memory access, fence completion or render commands
are introduced. Callback/journal overhead is unmeasured on a native GPU run.

The ARM64 library compiles and links with the local overlay, and all 46 host imports
are present. The actual auxiliary SPIR-V regression and full JIT/interpreter
contracts pass. Native CPU fixtures check dependencies, prepared/returned states,
recording reuse, bounded tails, concurrency and callbacks. A CPU-only supervised
fixture proves the dump precedes the watchdog prefix and survives cooperative exit.
Twenty offline builder tests cover pristine application, exact previous-set
migration, unknown/partial rejection and idempotence; twenty supervisor tests pass.
The owner's first native capture is analyzed above. No GPU capture was run in the
tool environment.

For the next single owner-run capture, keep the previous low-resolution renderer
settings and 120-second observation budget; the protective GPU-watchdog stop remains
active. From the repository root:

```sh
SOTC_MEMORY_BACKING=file BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 \
  BB_JIT=1 BB_FRAME_STATS=1 BB_UPSCALE_PRESET=4 \
  SOTC_GPU_SUBMIT_TRACE=1 SOTC_DIAGNOSTIC_SECONDS=120 \
  bash packaging/test-arm64.sh --graphics-game-test
```

The local library is already rebuilt. On another checkout, first run
`python3 tools/build_gpu.py`; an old library cannot implement this journal simply
because its environment contains the new flag. Inspect the resulting private log
before another experiment. Submission IDs can change between runs. Even a complete
CPU list may leave several candidates without per-command GPU execution markers.

### Earlier low-resolution comparisons

**Preceding 30-second repeat:** the owner repeats the low-resolution configuration and reports
that the window closes when turning on the controller. The only input line records
the logical controller opening with keyboard fallback. There is no physical
connection/disconnection message, input-backend failure, unresolved-import stop,
CPU-fault report or GPU-watchdog warning. The recorded stop is the existing
30-second wall deadline, followed by the native cancellation notice observed about
1.90 ms later. This evidence does not establish whether hotplug completed or caused
any separate problem; do not diagnose a controller crash from timing coincidence.

The supervisor again fails to collect the child status within its unchanged
two-second allowance: unknown child status, supervisor exit 1, elapsed about
32.02 seconds. A later lookup of that specific PID returns ESRCH. This confirms
later process absence, not its exit code, exact exit time, a successful supervised
stop or a permanently live process. Preserve the owner's window observation and
this failed collection separately. Do not increase the grace period as a presumed
fix or enable external-signal fallback.

Initialization completes at about 1.07 seconds and the main loop starts at about
1.53 seconds. Scene raster remains 640x360 with 320x180 half-resolution targets
and 1920x1080 UI/output. Five statistics windows record nonzero reduced scene draws
after the first window; the last is 27 reduced draws/frame out of 33 scene draws.
The CSV retains 399 complete rows through about 23.70 seconds relative to its first
row, plus a partial row. Buffered-row loss still prevents a final frame count.
Both integer-format Metal blit failures recur, alongside the earlier shader/resource
errors. Sampled Vulkan allocations peak at 4772 MiB and images at 931 MiB; these
are renderer counters, not measured total macOS RAM pressure or proof of lower load.

The owner also requests direct GPU use now that the scene resolution is reduced.
GPU rendering is already active in the owner's native runs; DMA-off is a separate
dynamic shader-read setting. One supervised synthetic Metal prerequisite recheck in
the tool context still exposes zero devices and confirms child/supervisor exit 24
after about 0.17 seconds, before any draw or game renderer launch. This is a host
execution-access limitation, not a repository GPU-disable switch. No escalation,
alternate Terminal-control route or additional native game test was attempted.

**Preceding confirmed-exit result:** the owner's single low-resolution comparison confirms the
preset was applied: scene raster targets are 640x360 with 320x180 half-resolution
targets and 1920x1080 UI/output. The last two statistics windows record 26 reduced
draws/frame out of 26 scene draws, then 24 out of 24. This verifies application to
some SotC scene passes, not all draws, correct displayed images or gameplay.
Initialization finishes at about 1.90 seconds and the main loop starts at about
2.81 seconds. The 30-second stop request is followed by a native cancellation
notice and confirmed `child_returncode=21 exit=21` after about 30.37 seconds. The
notice is observed about 0.91 ms after the request. There is no cleanup error,
import stop, CPU-fault report or GPU-watchdog warning. This confirms the stop in
this run, without explaining earlier unknown exits or proving system stability.

The CSV retains 240 complete rows spanning about 18.37 seconds from its first row.
The explicit 120-flip flush and cancellation `_Exit` can discard later buffered
rows, so this is not a final frame count or the time presentation stopped. Four
statistics windows report flip rates about 8.0 to 14.9/s; these do not establish
gameplay FPS or a causal performance change. The initial idle figure exceeds
100%, so it is not a usable instantaneous GPU-utilization/pressure measurement.
No physical gamepad detection is recorded in this log; image progression is still
unreported. Shader/resource failures remain: 54 PerVertexKHR errors, 259 Phi offset
errors, 267 SRT-walk failures, six compressed-format/usage errors and 3,637 clamps.
Lower message counts are not evidence of corrected shaders or better output.

Imported guest memory remains 6592 MiB. Sampled Vulkan allocation maxima are
4867 MiB versus 4593 MiB in the preceding baseline, and Vulkan image maxima are
797 versus 751 MiB. These different snapshots/progress intervals do not establish
total macOS memory pressure or isolate a causal preset effect, but they provide
no evidence that this test reduced total memory. Full-size originals and UI remain
allocated while reduced proxies add storage. See the source-verified limitations
in the comparison below.

Two new Metal pipeline errors identify a concrete next renderer target: R32Uint
and RG32Uint blit destinations reject the generic float4 fragment output. In the
pinned `renderer_metal/mtl_frame.mm`, `BlitPipeline` chooses only float-color or
depth shaders, and `BlitImage` returns without copying when the pipeline is nil.
The failure is cached by destination format, so two messages do not mean only two
copy operations were affected. A correction needs typed integer reads/outputs and
nearest-copy semantics that preserve integer values, rather than a float output
cast or skipped-copy success. Validate pixel formats, rectangles, mip selection
and representative integer values with synthetic data before another native
comparison. The new sampler-LOD-bias warning is a separate remaining limitation.
No renderer patch or further native game comparison was made for this log analysis.

**Earlier timed-notice checkpoint:** that instrumented run records its cancellation notice
about 1.47 ms after the parent stop request, but still ends with unknown child status
and supervisor exit 1 after about 32.02 seconds. The owner confirms that the window
closed itself, without manual termination. A subsequent targeted `proc_pidpath`
lookup returns ESRCH for that run's PID. This confirms process absence at the later
check, not its exit code, exact exit time or the cause of the supervisor's failed
collection. No process sample was taken because that PID was no longer present.
Do not describe this execution as a permanently live process or a proven driver
hang; do not retroactively replace the unknown return code with success.

Initialization finishes at about 0.92 seconds and the main loop starts at about
1.35 seconds. The same DMA-off, async-off, JIT/statistics-on 30-second settings are
recorded, with five statistics windows, no import stop, CPU-fault report or GPU
watchdog warning. Its CSV retains 438 complete rows through about 25.37 seconds
relative to the first row, plus a partial row. Errors persist: 52 PerVertexKHR,
259 Phi offset, 267 SRT-walk, six compressed-format/usage errors and 7,687 buffer
clamps. The DualSense is detected. This run's image progression remains unreported.

The owner now explicitly authorizes low-resolution GPU tests to reduce load.
The next single scene-preset comparison is prepared below. A supervised Metal
prerequisite attempted from this tool context returns zero devices and status 24
after about 0.35 seconds, with a confirmed child exit. No real game/GPU test was
launched here, and no permission escalation or alternate Terminal-control path
was attempted.

**Earlier acknowledgement checkpoint:** that run includes the output-backpressure correction and
now emits `STOP_DIAGNOSTIC_CONTROL: parent requested termination`. The supervisor
still fails to observe child exit within its two-second allowance. Its footer
reports `child_returncode=None exit=1` after about 32.02 seconds. This is still a
failed stop. The notice comes immediately before `_Exit` in our control thread;
it proves that the reporting point was reached, not that the exit call completed
or that the OS reaped the child. Neither a driver/kernel teardown cause nor an
insufficient grace period is established. Do not increase timeouts as a remedy.

The header retains DMA off, asynchronous submission off, JIT on, frame statistics
on and the same 30-second budget. Initialization finishes at about 1.03 seconds
and the main loop starts at about 1.65 seconds. There is no unresolved-import stop,
CPU-fault report or GPU-watchdog warning. Five statistics windows and 438 complete
CSV rows are retained, through about 25.23 seconds relative to the first row,
followed by a partial row. Pixel correctness and image progression remain
unreported. Errors persist: 48 PerVertexKHR errors, 259 Phi offset errors, 267
SRT-walk failures, six compressed-format/usage errors and 7,423 buffer clamps.
The DualSense is detected; no final input/audio summary is available.

The supervisor now records `child_pid` before reading child output and appends
`control_notice_seen` and `control_notice_after_request_ms` to its completion
footer. The latter is a parent-side observation delay, including output delivery;
it is not the child's exact cancellation latency, kernel teardown time or an
exit acknowledgement. The existing child return code remains the exit evidence.
A split-notice/delayed-self-exit synthetic fixture verifies that observing the
notice still produces a failed stop while exit remains unknown. GPU settings,
the wall budget, two-second allowance and no-signal-fallback policy are unchanged.
No GPU run was launched for these logging changes.

All 20 supervisor tests and four native diagnostic-budget tests pass after the
logging change, using only synthetic processes/guests. The native control header
and renderer are unchanged from the preceding backpressure correction.

Before another GPU instance, confirm that the prior `probe_entry_arm64` process
has ended. The latest recorded PID is absent at the targeted check above. Window
closure alone is insufficient when the supervisor cannot reap a future instance.
An exit-phase process state/stack could distinguish the remaining collection
failure; this is separate from the authorized scene-preset test. The private log's PID
identifies the owned diagnostic in Activity Monitor. If it remains present after
the failure, use Sample Process for that specific diagnostic and keep the report
under ignored `work`. Record a sampling failure or an already-exited process as
such; do not request elevated access or substitute external termination signals.

### Low-resolution scene comparison

The owner has run the single comparison adding only `BB_UPSCALE_PRESET=4` to the
preceding 30-second settings; its measured result is summarized above. The command
below records that configuration, not an instruction to rerun automatically.
The pinned `BbSettings::Load` accepts this preset;
`SceneResolution::ForPreset` requests 640x360 from the default 1920x1080 output,
with 320x180 half-resolution effects where eligible. This uses the existing
host-side raster proxy path. The window, UI/output images and imported guest pools
retain their preceding sizes. Do not set `BB_RENDER_RES`: its compatibility path
assumes a game-specific startup resolution patch that is not applied to SotC.

```sh
SOTC_MEMORY_BACKING=file BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 \
  SOTC_DIAGNOSTIC_SECONDS=30 BB_JIT=1 BB_FRAME_STATS=1 BB_UPSCALE_PRESET=4 \
  bash packaging/test-arm64.sh --graphics-game-test
```

Keep the default FSR 3.1 provider and output resolution; do not add frame-limit,
worker, DMA or memory-budget changes to this comparison. The supervisor header
now also records `BB_UPSCALE_PRESET`, `BB_RENDER_RES` and `BB_OUTPUT_RES`, so inherited
overrides can be identified. A CPU-only check compiles the actual pinned settings
implementation and scene-size header, then verifies preset 4 selects 640x360 without
a fixed guest-resolution session. It does not exercise rasterization or a GPU.

Look for `Scene resolution: raster 640x360` and nonzero `reduced-size draws` in the
five-second statistics. The setting/size message alone does not prove that SotC's
draws used the proxies: the renderer checks scene dimensions, formats, attachments
and pass eligibility. Native-size guest images remain allocated and reduced proxy
images can be additional allocations. Pixel work may fall for eligible passes;
lower total memory pressure, fewer shader failures, image correctness and host
stability are not established. Guest pool imports remain approximately 6.4 GiB.
Read the new log and memory counters before another comparison. This tool context
has no Metal device; actual validation needs the owner's native Terminal.
The upstream VRAM/GTT/RSS fields read Linux `/proc` files and report zero on this
Mac; those zeros are not evidence of low memory pressure. Vulkan allocation/image
counters describe only their own allocations, not total macOS unified-memory use.

**Earlier unacknowledged-stop checkpoint:** after the POSIX clock-alias correction, that instrumented
run reaches its 30-second deadline without an import stop, CPU-fault report or GPU
watchdog warning. Initialization finishes at about 0.91 seconds and the main loop
starts at about 1.34 seconds. DMA and asynchronous submission remain off, with JIT
and frame statistics on. The log records five flip-statistics windows. Its CSV
retains 438 complete rows through about 24.80 seconds relative to the first record,
followed by a partial row. Flip records do not establish pixel correctness, game
progression or gameplay FPS; the latest image progression is still unreported.

The supervisor records its deadline request, followed by a cooperative-stop error:
child exit is not observed in the two-second allowance. Its footer reports
`child_returncode=None exit=1` after about 32.04 seconds. There is no native
`STOP_DIAGNOSTIC_CONTROL` acknowledgement. Classify this as a failed stop and ensure
the previous instance has closed before another GPU comparison. Do not infer a
kernel/driver hang, clean exit or continued process lifetime solely from this file.
Shader/resource failures persist: 64 PerVertexKHR errors, 259 Phi offset errors,
267 SRT-walk failures, six compressed-format/usage errors and 7,942 buffer clamps.
The DualSense is detected again; no final input/audio summary is available.

CPU-only fixtures reproduce two output-backpressure failure mechanisms in the old
cancellation code: a full inherited output pipe blocks the native stop notice
before `_Exit`, and a child writing queued output cannot exit while the supervisor
waits without draining it. Both fail before the fix. The native control thread now
makes its notice nonblocking and best effort, then exits regardless of delivery.
The supervisor consumes bounded output batches while awaiting exit, retaining the
same two-second allowance and never using external-signal fallback. All 19 targeted
supervisor tests pass, including the two regressions. This explains a reproducible
mechanism consistent with the native log, but does not establish that it caused
that native failure. No new GPU run was launched for this correction, and native
stop confirmation remains pending.

The full synthetic `--contracts-test` passes in JIT and interpreter with this
change. The Metal prerequisite also compiles with the updated control header;
it was not executed. Neither check loads a real GPU or validates recovery from a
kernel/driver hang.

**Earlier import-stop checkpoint:** the preceding flip-statistics run ends at an unresolved POSIX `usleep`
import in the original NGS2 audio module, on its sound-system thread. Initialization
finishes at about 1.17 seconds and the main loop starts at about 1.63 seconds. The
supervisor confirms `child_returncode=20 exit=20` after about 19.67 seconds, before
the 30-second deadline. There is no CPU-fault report, GPU-watchdog warning or cleanup
error. DMA and asynchronous submission remain off, with JIT and frame statistics on.
The run's visual progression has not been reported; do not attach the prior
first-logo observation to it automatically.

The private CSV retains 134 complete flip rows spanning about 9.03 seconds relative
to its first row, with a longest recorded interval of about 1.12 seconds. One
five-second summary reports a 16.7/s flip rate. These are presentation-path records,
not proof of distinct displayed images, correct pixels, gameplay FPS or absence of
a later stall. This import-stop path explicitly flushes streams before `_Exit`;
concurrent work can still end between observations. Do not use the CSV's relative
origin as process startup time or its last row as the process stop time. The file
still has 49 PerVertexKHR errors, 220 Phi offset errors, 228 SRT-walk failures, six
compressed-format/usage errors and 1,153 buffer clamps. Input uses keyboard fallback
with no physical gamepad detected; the earlier comparison detected a DualSense.

The missing symbol's NID matches POSIX `usleep`, and the supplied module's import
identity is `libkernel` library version 1 / module version 257. The linker correctly
normalizes that identity into the executable's kernel namespace. Our clock resolver
only bound the POSIX-library spelling. It now binds the exact kernel-library alias
to the same real POSIX sleeper, preserving interruption and errno semantics. The
same review found the already-present POSIX `gettimeofday` import in the kernel
namespace, so its checked adapter receives that exact alias too. No NID-only
fallback, new timing policy or fabricated success is introduced. The next native
comparison uses the same instrumented 30-second configuration below.

The full synthetic `--contracts-test` passes in JIT and interpreter after this
change, without loading the real GPU or game. New guest-call checks cover both
scopes, rejected data/foreign scopes, zero and real microsecond waits, 32-bit
argument truncation, signal interruption with POSIX EINTR, success errno
preservation and guarded realtime outputs. The subsequent native run has no import
stop during its recorded interval, as summarized above.

**Earlier cooperative-stop checkpoint:** an earlier native GPU run confirms cooperative cancellation.
Its header records DMA off, asynchronous submission off and JIT on. The control
channel is ready before renderer initialization, which finishes at about 1.10
seconds; the main loop starts at about 1.63 seconds. At the 30-second deadline,
the log records the stop request, `STOP_DIAGNOSTIC_CONTROL` and a completion footer
with `child_returncode=21 exit=21` after about 30.52 seconds. There is no cleanup
error, unresolved import, CPU-fault report or GPU-watchdog message. This verifies
the stop for that run; it is not a renderer-stability or gameplay result.

**Earlier visual boundary:** the owner reports only the first logo, without reaching the
second. The earlier menu and introduction-audio observations remain historical;
this run does not reproduce them. The duration and submission configuration differ
from earlier runs, so neither is an established cause of the missing progression.
The last sampled Metal activity drops to one draw and nine dispatches, but these
are command counters, not a record of displayed images or a diagnosis of a stall.

That file still has 21 PerVertexKHR/skipped-pipeline messages, 220 Phi offset
errors, 228 SRT-walk failures, six compressed-texture usage errors and 910 clamps.
The DualSense and Metal command activity are recorded. Message/command counts are
not FPS or evidence of better rendering. The subsequent investigation records flip
progress with the existing host-side statistics described below. Do not change
renderer settings based solely on error counts. The following paragraphs retain
earlier checkpoints.

The earlier completed native log uses DMA off and a 120-second budget. Initialization completes
at about 1.69 seconds and the main loop starts at about 2.33 seconds. The run ends
at STOP_TIMEOUT without an unresolved import, CPU fault or GPU-watchdog message.
It records Metal draw/dispatch activity throughout its sampled intervals and detects
the DualSense. This does not verify scene entry, introduction-video correctness or
system stability. Five PerVertexKHR conversion errors, 220 Phi offset errors, 228
SRT-walk failures and 996 buffer clamps remain; message counts are not distinct
shaders and cannot be compared as performance or correctness improvements.

`tools/run_gpu_diagnostic.py` now supervises the combined process in a separate
session. Its wall budget includes renderer startup. Owned diagnostics now receive
cancellation through an inherited pipe and exit themselves from a native monitor
thread. The channel is validated/started before renderer initialization. A GPU
watchdog stop returns 31; deadline expiration returns 21 after confirming child exit.
Parent cancellation or parent-pipe closure also requests self-exit. The launcher
uses `--cooperative-stop` and never escalates to external signals in this mode.
Generic process-group signaling remains available for synthetic process fixtures.
The child exit status is otherwise preserved, output is mirrored into a new private
log, and there is no retry. This is an additional user-space stop, not protection
against a frozen kernel/driver. The standalone Metal prerequisite uses a separate
ten-second supervisor. The combined default remains 120 seconds.

The owner has now run the 30-second comparison with asynchronous submission off
(`BB_ASYNC_SUBMIT=0`), DMA off, JIT on and the same renderer pins. The header confirms
the settings and the two recording workers remain. Initialization finishes at
about 0.924 seconds and the main loop starts at about 1.36 seconds. Five Metal
samples record draw/dispatch activity, with no import stop, CPU-fault report or
GPU-watchdog warning in the file. Its last write is near the 30-second boundary,
but there is no child timeout, supervisor stop or completion footer. Therefore the
exit cause is unknown; do not classify it as a confirmed timeout or clean exit.

This run records 48 PerVertexKHR errors/skipped-pipeline messages, 259 Phi offset
errors, 267 SRT-walk failures, six compressed-texture usage errors and 7,788 buffer
clamps. Its larger command counts and error counts do not establish better FPS,
worse rendering, scene entry or a causal benefit from the submission setting.

The supervisor persists a stop decision before cleanup and records
the observed child status, supervisor exit code and elapsed time on its normal
completion paths. Natural exits, busy output, child signal statuses and timeout
cleanup output are covered with synthetic processes. The earlier missing footer
cannot be reconstructed retrospectively. No additional GPU test was launched for
this change.

The next native log now confirms the deadline decision at 30 seconds, followed by
`STOP_GPU_SUPERVISOR_CLEANUP: Operation not permitted`. Its footer reports child
status unknown and supervisor exit 1 after about 30.56 seconds. This establishes a
cleanup failure, not confirmed child termination. It again reaches the main loop
(about 1.38 seconds), with no import stop, CPU-fault report or GPU-watchdog message.
The GPU shader/resource failures remain. The earlier generic error did not identify
the exact cleanup syscall; stage-specific errors are now recorded as well.

The owned probe and Metal check use the cooperative channel to avoid depending on
external signal permission. This does not elevate privileges, retry denied signals
through another API or guarantee recovery from a kernel/driver hang. CPU-only
native fixtures check deadlines despite ignored TERM, parent cancellation, parent
channel closure and invalid descriptors. Synthetic infinite guests verify the
actual probe's channel in JIT and interpreter before its longer own alarm.
No GPU run was used to implement that change. The subsequent native run confirms
child exit under GPU load, as summarized above. This remains the reference
configuration; it is not an instruction to rerun automatically:

```sh
SOTC_MEMORY_BACKING=file BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 \
  SOTC_DIAGNOSTIC_SECONDS=30 BB_JIT=1 \
  bash packaging/test-arm64.sh --graphics-game-test
```

Inspect the newest game log before another run. Record what was actually visible,
whether progression stopped, the selected setting, terminal stop and any GPU-stall
message. Keep other renderer settings unchanged and do not turn DMA back on.

### First-logo presentation trace

The owner has run the pinned renderer's `BB_FRAME_STATS=1` instrumentation. The
following command records its preceding baseline for comparison; the current
prepared next test is the [low-resolution scene comparison](#low-resolution-scene-comparison)
above. Do not automatically repeat this baseline:

```sh
SOTC_MEMORY_BACKING=file BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 \
  SOTC_DIAGNOSTIC_SECONDS=30 BB_JIT=1 BB_FRAME_STATS=1 \
  bash packaging/test-arm64.sh --graphics-game-test
```

No additional GPU run was launched for the new observation fields. Use a native Terminal only
after the prior diagnostic process has ended; inspect the resulting log before
another comparison. The launcher chooses a private `frames.csv` inside this run's
isolated GPU-state directory unless `BB_FRAME_LOG` is already specified. The
supervisor records both settings and the CSV path in its header. Remove
`BB_FRAME_STATS` from the environment to return to the preceding baseline.

In the pinned `VideoOutDriver::Flip`, this instrumentation reads host clocks and
existing counters after the presenter call. It reports flip-rate/wait summaries
every five seconds when flips continue and records per-flip CSV rows. It is not a
pixel readback, a new shader, or evidence of correct game frames. Profiling adds
CPU work and can affect timing. The CSV is buffered, with an explicit flush every
120 flips; cooperative `_Exit` can discard its final rows. An empty, absent or
short file does **not** prove that no flips occurred. Keep CSVs and logs private.

Look for continuing flip records versus continuing command activity, long frame
intervals, host/GPU wait counters and compilation work. If flips continue while
the first logo remains, investigate image/resource contents; if the trace stops,
investigate the submission/presentation waits. No result alone justifies skipping
a fence, replacing a shader result with zero, increasing the budget or enabling
DMA. The retained records establish some flip progress; they do not yet identify
the cause of the earlier first-logo observation. Establish the exit-phase failure
and confirm actual process termination before another renderer change.

## First session: inspect without GPU execution

Run commands from the repository root in a native macOS Terminal. Quote absolute
paths containing spaces. Keep stdout/stderr and generated reports private.

```sh
uname -m
# Must print arm64.
git status --short
git submodule status --recursive
bash packaging/test-arm64.sh --contracts-test
```

Read the newest `logs/graphics-game-*.log` by modification time, not an old filename
from chat. Compare its final lines with the preceding completed run. No new GPU
run is needed for this inspection. The combined game now defaults to no duration
limit; headless remains 30 seconds. `SOTC_DIAGNOSTIC_SECONDS` accepts 0..1800, with
zero disabling only the game-duration timers. Renderer-only initialization retains
its separate 60-second limit. A timeout is an observation boundary,
not successful gameplay or evidence that all imports are implemented.

The combined command rebuilds the host diagnostic, prepares a separate linked image,
checks Metal access and host symbol coverage, and launches the renderer. SDL/window
initialization stays on the host main thread; guest execution runs on a worker.
Each run has an isolated GPU configuration/cache directory under `work`.

## Reconstructing the environment

The full setup lives in [Building](guide/building.md) and [Game files](guide/game-files.md).
The main entry points are:

| Command | Purpose and limit |
|---|---|
| `bash packaging/test-arm64.sh --fetch` | Fetch exact pinned sources; do not run over non-Git snapshot directories |
| `bash packaging/test-arm64.sh --test` | Build CPU dependencies/archive and run native emitter, JIT and vector checks |
| `bash packaging/test-arm64.sh --contracts-test` | Synthetic runtime/ABI tests in both CPU modes; no game or real GPU required |
| `bash packaging/test-arm64.sh --prepare` | Prepare the owner's original executable privately |
| `bash packaging/test-arm64.sh --content-check` | Audit supplied archive manifests/extents; does not establish asset decoding |
| `bash packaging/test-arm64.sh --linked-test` | Paired headless initialization comparison; intentionally stops at unsupported imports |
| `bash packaging/build-gpu-arm64.sh` | Build/resume the pinned isolated renderer and dependencies |
| `bash packaging/test-arm64.sh --graphics-check` | Synthetic offscreen Metal draw/readback; no game frame |
| `bash packaging/test-arm64.sh --vulkan-check` | Check Vulkan surface extension discovery |
| `python3 tools/check_gpu_link.py` | Inspect actual host/library symbol coverage; does not validate ABI or load the library |
| `python3 tools/check_pipeline_validity.py` | CPU-only source-guard regression: original invalid acceptance versus adapted rejection |
| `bash packaging/test-arm64.sh --gpu-load-check` | Load the library without renderer initialization |
| `bash packaging/test-arm64.sh --renderer-check` | Initialize the renderer and diagnostic window without guest commands |
| `bash packaging/test-arm64.sh --graphics-game-test` | One supervised combined experiment; see the pending-submission capture above |

Important environment variables:

| Variable | Meaning |
|---|---|
| `BB_DEPS` | Optional compatible ARM64 static Zydis/Zycore prefix; default `work/deps-arm64` |
| `SOTC_GAME_ROOT` | Owner's readable game directory containing `eboot.bin` and `sce_module` |
| `SOTC_SYSTEM_MODULE_ROOT` | Original gesture/NGS2 modules; default sibling `sys_modules` directory |
| `SOTC_MEMORY_BACKING=file` | Unlinked temporary-file backing with shared aliases when POSIX shm is unavailable |
| `BB_JIT=1` / `BB_JIT=0` | ARM64 translation / interpreter |
| `SOTC_DIAGNOSTIC_SECONDS` | Accepts 0..1800; 0 means no game-duration limit and is the combined default; headless default 30; prerequisite budgets and GPU-progress cancellation stay active |
| `SOTC_GPU_SUBMIT_TRACE` | Combined diagnostic defaults to 1; 0 disables the bounded CPU submission journal and Metal callbacks; dumps on GPU watchdog |
| `BB_DIRECT_MEMORY_ACCESS` | Unset or literal 0 only; all other values rejected before GPU loading/prechecks |
| `BB_ASYNC_SUBMIT=0` | Current single comparison: submit on the calling thread; preserve recording workers |
| `BB_FRAME_STATS=1` | Opt-in host-side flip/wait statistics for the first-logo investigation; remove the variable for baseline timing |
| `BB_FRAME_LOG` | Optional CSV destination; with statistics on, the combined launcher defaults to private per-run `frames.csv`; final rows may be buffered |
| `BB_UPSCALE_PRESET=4` | Prepared low-resolution comparison: requests 640x360 host scene targets at default output; confirm actual reduced draws, native/guest memory does not automatically shrink |
| `SOTC_GPU_JOBS` | Isolated build parallelism, 1..8 |

The launch script explicitly selects its bundled Vulkan loader, MoltenVK ICD and
SDL Vulkan library. Preserve this: the Metal path still depends on Vulkan WSI.
`VK_KHR_surface` failures can come from selecting a different system loader/ICD.
The builder repairs generated FFmpeg/pkg-config metadata for prefixes with spaces.
Do not move the repository merely to conceal a quoting regression.

A restricted automation process can compile and run CPU contracts while exposing
no Metal device. Run only the authorized single comparison in native Terminal. A failed device
precheck in that process is not evidence that the user's GPU is unsupported.

## Code ownership and execution path

| Area | Files and responsibility |
|---|---|
| Build/test orchestration | `packaging/test-arm64.sh`; compiler flags, source lists, isolated modes, log capture |
| Input preparation | `tools/prepare_game.py`, `tools/prepare_linked_arm64.py`; private BBPROBE images and original module/TLS metadata |
| Host entry and stops | `runtime/src/probe_entry_arm64.c`; image mapping, relocations, initializer order, watchdog, import/fault reporting |
| Cooperative cancellation | `tools/run_gpu_diagnostic.py`, `runtime/src/diagnostic_control.h`; owned pipe, native child monitor, self-exit and parent status confirmation |
| Import policy | `runtime/src/bootstrap_contracts.c`; scoped namespace mappings and capability gates |
| CPU context and vector support | `bootstrap_cpu_context.c`, `bootstrap_cpu_vector.c`; pinned bbcpu adapters and explicit extensions |
| Threads/TLS/fibers | `bootstrap_threads.c`, `bootstrap_tls.c`, `bootstrap_keys.c`, `bootstrap_fibers.c` |
| Shared memory and address policy | `platform_arm64.c`, `runtime_memory_arm64.c`; real physical backing/aliases and low-address constraints |
| GPU notification storage | `tools/prepare_memory_support.py`, `runtime/src/memory_hook_queue.h`; checked private source adaptation with growable storage |
| Library/device binding | `runtime/src/bootstrap_gpu.c`; checked dlopen/dlsym, main-thread initialization and actual backend publication |
| Graphics import adapters | `runtime/src/bootstrap_graphics_routes.c`; reviewed encoders, live display validation, submit ABI and provider errors |
| Local services | Other `bootstrap_*.c` files; explicit narrow clocks/input/audio/service contracts |
| Renderer build | `tools/build_gpu.py`; pinned private source, checked upstream backports, dependencies and `work/gpu-build/libbbgpu.dylib` |
| Native GPU internals | Pinned `runtime/upstream/gpu` and its private build checkout; shaders, pipelines, buffers, textures, Metal/Vulkan |

Input preparation preserves original FS/TLS loads; do not reintroduce native x86 TLS
rewrites. The CPU executes supplied original libc/Fios2 and, in graphics mode,
gesture/NGS2 modules. Scoped host imports call reviewed adapters or an actual initialized
GPU provider. Unknown imports stop; missing services report explicit unavailability.

The host build copies the CPU archive and replaces two members with local context/
vector adaptations. Do not mutate the original archive. Mutex statistics and GPU
notification storage are transformed only in checked private build copies. The memory
preparation validates the complete pinned source digest and replaces just its storage
declaration and capacity guard. The source checkout remains unchanged.

Do not run two host build/test scripts concurrently: both regenerate the same private
archive and diagnostic sources in `work`.

## Prioritized debugging work

1. **Investigate the host watchdog incident without reproducing it.** Read the latest
   instrumented stall and earlier initialization-only/panic-associated logs together. Review renderer
   startup, shared guest-memory residency, queue progress and the pinned upstream
   sources. The cause is unresolved; do not assume DMA, memory pressure or the
   controller is responsible. The owner has resumed gradual testing; run one
   supervised comparison, inspect it, and do not replay the DMA-on incident.
   Scene entry and introduction video remain later goals; the owner has confirmed
   logos/menu and opening audio.
2. **MSL fragment interface support.** The preceding complete low-resolution log has 118 `PerVertexKHR` conversion
   failures. `python3 tools/check_fragment_interface.py` isolates this with the actual
   installed SPIRV-Cross library: an ordinary input converts, raw per-vertex input
   reading vertex one fails. Exit 2 means the known limitation is reproduced; it does
   not mean shader support passed. Neither case initializes a GPU. The Vulkan device
   advertises fragment barycentrics, but this alone does not prove the selected MSL
   conversion path supports raw vertex arrays. Trace `V_INTERP_MOV_F32` and
   `Qualifier::PerVertex` through `vector_interpolation.cpp` and
   `spirv_emit_context.cpp`. Do not remove the decoration or substitute interpolated
   input without preserving the requested vertex selection. The earlier duplicate
   auxiliary location error is absent; not every tessellation path is verified.
3. **SRT resource tracking.** Shader direct memory access is disabled and enabling it
   is blocked after the host incident. The earlier SRT/shader-info fixture checks
   uniform reads,
   Phi, per-lane and arithmetic offsets with DMA off/on: dynamic resources are
   selected only when the setting is enabled. This does not execute those accesses
   on a GPU or validate game resources. The existing walker still logs its static
   offset failures. Its result is offline compiler evidence, not hardware validation;
   do not rerun real-library experiments during incident containment or describe
   the messages as fixed. `flatten_extended_userdata_pass.cpp` fails offset
   computation on `Phi` and then fails its SRT walk. Investigate predecessor/value
   handling and resource provenance in the recompiler. Buffer-size clamps may relate
   to bad descriptors, but their causal link is not established. Do not treat clamps
   as a repair or replace unknown offsets with zero.
4. **Compressed texture usage.** BC1/BC5 format/usage errors request combinations
   including storage and extended views. Check capability detection and whether each
   image actually requires storage access; design a supported decode/view path if it
   does. Dropping usage flags without tracing consumers is not sufficient.
5. **Prove visible output.** Trace registered VideoOut buffers, actual submissions,
   completed GPU work and presentation, then inspect a real frame locally. Encoding
   commands, a window, accepted audio buffers or zero “unsupported commands” do not
   establish correct game pixels.

Renderer correctness remains open; distinguish the validated SPIR-V correction from
its pending visual verification. The float-denorm capability warning alone does not
identify the cause of the final stop. Do not silently toggle label/fence completion
policies to advance execution; completion must reflect actual GPU work.

## Resolving further imports in batches

For `STOP_AT_IMPORT`, retain the exact scoped name, thread, first argument and caller
context privately. Numeric import indices depend on the prepared image; do not use
them as API identity. Inspect the supplied module's symbol identity, pinned source
and original calling convention before binding a function. Review related calls as
a family to avoid one-import-per-run progress, but keep unknown contracts unresolved.

The original x86-64 guest passes its first six integer arguments in registers and
later arguments on its stack. The host ARM64 ABI differs. Public `bbcpu_call` takes
at most six arguments: for seven or more, construct a synthetic x86 thunk as in
`tests/test_graphics_submit.c` or `tests/test_graphics_draw.c`. Test 32-bit narrowing,
signed/full-width values, output layout, untouched failure buffers, namespace and
capability gates, errno and actual backend errors. Symbol presence alone is not ABI
compatibility.

Known compatibility repairs include compute packet header bits, indirect-draw stage
validation, colored-marker layout, submit pointer/size checks and flip-wait label
queries. Do not replace these adapters with raw native exports just because names
match. SaveData, trophy, AppContent and dialogs mostly expose explicit unavailable/
no-instance behavior; they are not working persistence/UI services.

Where useful, compare a synthetic fixture against the owner's original module in a
private local harness. Provide only the real dependencies or narrowly controlled test
inputs necessary for that comparison. Distinguish original-compatible valid behavior
from deliberate malformed-input safety guards. Publish the synthetic fixture and
findings, never the original binary, flattened module or generated guest code.

## Validation and shipping a change

For runtime changes, run the full contracts command. CPU context/vector changes also
require `--test`. For renderer changes, run the relevant synthetic regression and
rebuild the isolated GPU library before requesting a combined Terminal run. Preserve
these pins unless an explicit, reviewed dependency update is intended:

| Source | Revision |
|---|---|
| runtime | `3170535f59cadbe5ce9c38e6bb961ed5fd215194` |
| recomp | `f749c155c2ff3bb932795de85c9c8fe9c4aa59ad` |
| SotC reference | `2250f328feafab953aac85e758e7bddb2e80e891` |

The queue regression is `tests/test_memory_hooks.c`: provider replay and guest-ABI
release of 257 real aliases, 8,193 mixed FIFO events, callbacks outside the region
lock, reentry, reuse, count rollback, absent hooks and explicit allocation/capacity
failure. `tests/test_prepare_memory.py` checks the restricted transformation and
rejection of changed upstream input. The SSE4a fixture is `tests/test_cpu_sse4a.c`, run by both `--test` and
`--contracts-test`, including the documented QEMU-compatible truncation policy in all four forms.
The widening family is `tests/test_cpu_widen.c`, also run in both modes by those
commands. It exercises all existing width pairs as well as the new qword pairs.
The GPU-only auxiliary emitter regression is `python3 tools/check_tessellation.py`;
it needs the isolated build but neither a GPU device nor game data. Backport source
checks are part of `tests/test_gpu_build.py`. Other graphics regressions are split across
`test_graphics_{routes,color,shaders,submit,present,draw}.c`; synthetic dynamic-loader
coverage lives in `tests/test_graphics_entry.py`. RTC is `tests/test_rtc.c`.

Before committing, stage only intended public source/documentation and run:

```sh
git diff --cached --check
python3 tools/check_repository.py
git remote -v
```

The audit reads staged/tracked content. The Linux source-check CI is a partial static/
fixture gate; a green CI run does not replace native ARM64 contracts or GPU testing.
Record the exact command, CPU mode, build revision, first stop and verification limit
in the change description. Never promote game addresses, execution logs, generated
images or private paths into public documentation.

## Local state and handoff maintenance

`game/`, `work/` and `logs/` are ignored private state. They contain the owner's input,
generated executable images/module analysis, build products, caches and execution
logs. Fresh clones do not contain them. Recreate input from the owner's dump; do not
copy an unrelated developer's `work` directory or assume private oracle scripts ship
with this repository. Keep game content read-only and do not access saves.

Update this page when the observed stopping boundary or reproduction command changes.
Separate what was seen in Terminal, what synthetic tests establish, what is only a
hypothesis and what remains unverified. A successful next run should replace the
current timeout/renderer boundary above with its actual result, including any timeout.

## Preferred upstream reference

The user explicitly selected [bloodborne-mac](https://github.com/bloodborne-native/bloodborne-mac/)
as the reference when resolving development doubts. Start there and follow its
[runtime](https://github.com/bloodborne-native/bloodborne-runtime) and
[recomp](https://github.com/bloodborne-native/bloodborne-recomp) sources for the actual
implementation. Check relevant current commits against our pins before adapting code.
Keep source provenance and tests, and reject Bloodborne-specific object/layout
assumptions for SotC. This preference does not authorize unreviewed dependency bumps.
When the reference lacks an implementation, document another primary source explicitly;
the SSE4a overrun policy here follows
[QEMU v10.1.0](https://github.com/qemu/qemu/blob/v10.1.0/target/i386/ops_sse.h).
