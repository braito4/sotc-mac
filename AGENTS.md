# sotc-mac

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
The [paused checkpoint](docs/SIGSEGV_CHECKPOINT.md) records the evidence, validated
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

The [memory-error workflow](docs/MEMORY_ERROR_WORKFLOW.md) is the preferred and most
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

This repository develops a native ARM64 macOS diagnostic for Shadow of the Colossus PS4.
Read README.md and docs/STATUS.md before changing the runtime. Public documentation is English.

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
Read first handoff checkpoint; earlier profiles/captures below are historical.

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
Read first handoff checkpoint; older captures and profiles below are historical.

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
Read first handoff checkpoint; earlier captures/profiles below are historical.

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
Read first handoff checkpoint; earlier prepared profiles below are historical.

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

Prepared SH write safety and shader-source follow-up: three legacy SH write
paths check the entire register/compute destination range before memcpy; original
compute end/crossing and graphics overrun controls overwrite adjacent canaries,
while adapted cases stop with 23. Valid/empty writes remain safe; empty writes
avoid destination references. Opt-in SOTC_SHADER_PROVENANCE=1 retains 64 numeric
descriptor slots/eight address-write or changed-lookup events each, with omissions,
suppression and slot evictions. Metadata failure saves exact scanner input, no
second backing read, plus private provenance sidecar. Code spans and lookup
observations use the same captured address as metadata validation, preserving
coherence even if the descriptor changes during reads; prior wrong-address
return is reproduced by a deterministic wrapper control. Actor/bank/packet pointers
are CPU observations, not guest-buffer ownership or a writer/lifetime proof.
ARM64/all builder CPU regressions, full JIT/interpreter contracts, 26 builder,
14 report/two metadata tests and 46/46 imports pass. Next command enables shader
provenance and disables unrelated tree probes, retaining fatal fields. Pins/cache
28/9 and render settings fixed. Public heads checked October 23 10:09:50 UTC
unchanged, earlier PR/release limits retained. No game launch; actual native
corruption cause unresolved. Read first handoff checkpoint.

Latest owner capture (October 23 09:56:27 UTC / 11:56:27 Madrid): required vertex
shader metadata rejection during indirect draw after 82.589 s, exits 23/23.
New renderer/probe UUIDs match; format-table guard is not reached. Mapped backing
and 65564-byte scan find no metadata; saved bounded slice/hash/head checked,
no OrbShdr signature. Invalid origin/lifetime/write visibility versus lookup
bounds remain open. Frozen newest-first CPU output complete: 2048 records,
131.696 ms, 878 objects/two threads, all four tree entries; no shader-origin
match or source proof. 893 presentations, 256 pipeline/271 SRT errors. Last/peak
native samples 8.827/8.859 GiB, no recorded OOM/large copies, memory drops/errors
zero. Earlier stop prevents improvement claims. Audio 30 windows/no queue-wait
failures. No fiber/CPU/watchdog/decode stop; previous causes unresolved. Next:
program/register/PM4 source and lifetime evidence. No code/settings/pin changes
or game launch. Read first handoff checkpoint.

Prepared shader format-table correction bounds-checks all 17 production lookup
tables. Checked-array original control traps on the first outside index; 2002
existing entries stay identical and 34 outside indices return undefined formats
for the existing explicit rejection. Installed decoder rejects synthetic outside
and in-table-hole inputs with real exit 23. The latest captured opcode itself
is an in-range undefined slot, so its shader-source/unsupported-format cause is
still unresolved. ARM64 renderer/all builder CPU regressions and 46/46 imports
pass; full CPU contracts pass in JIT/interpreter. Field CLI now tolerates non-UTF8 logs and
records replacements; six analysis tests/reprocessing last capture pass. Exact
format_previous cohort migrates with pins/cache 28/9 fixed. Public heads checked
October 23 09:52:37 UTC unchanged, earlier PR/release limits retained. No game
launch, skipped instructions or renderer setting changes. Same private command
next; see first handoff checkpoint.

Latest owner capture (October 23 09:43:06 UTC / 11:43:06 Madrid): fragment main
shader unknown-format rejection at its first word after 115.810 s, exits 23/23.
Owned 282-word/1128-byte snapshot exists; first word/context/hash checked. Invalid
program origin/content versus missing decode format remains open. Frozen CPU
history complete: 2048 events, zero skips/unavailable, 4.171667 s, 658 objects/five
threads; all four tree entries exercised but do not target shader provenance.
35,675,310 reservations, so complete output is not full lifetime history. 1296 API
presentations, 413 pipeline/805 SRT errors; last/peak native samples 10.469/10.480
GiB, no recorded OOM/large copies, memory drops/errors zero. Audio 44 windows,
no queue-wait failures. No fiber/CPU/watchdog stop; their causes not proved fixed.
Combined diagnostic matches reconstruction. Strict field CLI rejects non-UTF8 log
bytes; replacement decoding permits offline analysis, not corruption attribution.
Next: saved-byte decode-table and program-source/lifetime review. No code/config/
pin changes or game launch. Read first handoff checkpoint.

Prepared fiber guard diagnostics repair: real guard mismatch still exits 23,
but now attempts bounded numeric stack/RSP/expected-observed marker, metadata,
hex-encoded name and register evidence, then frozen newest-first CPU history.
Standalone CPU binaries use a weak evidence-only fallback; the probe's strong
history owner is exported. Nonblocking pipe/socket output, no retries; full/closed
output can still omit evidence. The report now retains all STOP categories and
fiber fields, including missing-context gaps for older captures. Seven native
history tests and real guest guard corruption in both CPU modes pass, alongside
twelve report/five field-analysis tests; full contracts pass in JIT/interpreter. Inside/outside
RSP and invalid arithmetic distinguished without claiming stack exhaustion.
No stack enlargement/guard removal, renderer/settings/pin/cache change or game
launch. Same tree-origin command next. Public heads checked October 23 09:38:41
UTC unchanged; prior PR/release limits retained. Read first handoff checkpoint.

Latest owner capture (October 23 09:30:39 UTC / 11:30:39 Madrid): direct fiber
stack-marker rejection after 109.422 s, exits 23/23. Checked save_running code
compares the current fiber stack's first word with its guard; mismatch does not
prove exhausted stack versus overwrite/reuse/metadata corruption. No fiber name,
bounds, guest RSP or actual guard word emitted. No relation to prior bad tree or
allocator pointer established. Seven tree probes configured/matching binaries,
but direct exit bypasses history: no CPU dump. diagnostic.json matches rebuilt
report yet omits this STOP category; raw log supplies reason. 1181 presentations,
416 pipeline/805 SRT errors; last/peak native sample 10.190 GiB, no recorded OOM
or large copies. Memory drops/errors zero; 42 audio windows/no queue-wait failures.
Next: bounded fiber/guard/RSP evidence and history, plus stop classification;
retain real guard and exit 23. No runtime/settings/pin changes or game launch.
Read first handoff checkpoint.

Prepared tree-origin follow-up replaces allocator probes with seven fields at
four checked function/call-return boundaries: root before/after helper, boundary
links, child sources after comparator and comparator node source. Root observations
remain unconditional; repeated consumer values may be suppressed. Actual linked
bytes/executable bounds and real parser acceptance checked, no game/GPU started.
Combined diagnostics now retain protected-read failures/invalid-address samples
and bounded numeric register/history matches, excluding unreadable values from
value matches. Eleven report/five analysis/six native probe tests pass. Same bins,
pins/cache/settings; native source/corruption validation pending. Public refs
checked October 23 09:28:46 UTC unchanged; earlier PR/release limits retained.
Use new ignored tree-origin command; read first handoff checkpoint.

Latest owner capture (October 23 09:22:03 UTC / 11:22:03 Madrid): CPU SIGSEGV
after 67.702 s, exits 139/139, same linked-node key comparator as the 07:57 run.
Current linked bytes checked; node pointer already invalid before comparison.
Root/link/writer/lifetime cause unknown. Early minimum/exact guest context work;
newest-first history partial, 532 events/no footer, 3.030205 s, 33 objects/six
threads. Allocator source probes miss current tree root/link boundary. 908 API
presentations, 326 pipeline/271 SRT errors; lower counts reflect earlier stop,
not a fix. Last/peak native samples 9.050/9.065 GiB, no recorded OOM. Three
187.012 MiB copies complete using the same 192 MiB staging allocation; memory
summary drops/errors zero. Audio 24 windows, no queue-wait failures. No GPU
watchdog/shader rejection; their absence is not resolution. Retarget source
provenance before another run. No code/config/pin changes or game launch.
Read first handoff checkpoint.

Prepared GPU-stall report improvement: diagnostic.json now separates completed and
awaited batches, dependencies, buffer recording generations, callback/native status,
and native memory availability. Missing callbacks/samples stay unknown; scheduled
is not per-command execution. CPU budget omissions and GPU candidate omissions
are explicit evidence gaps even with a complete footer. Existing watchdog stop
marker is retained. Nine report/five field-analysis tests pass; production journal
output is covered by the native synthetic suite. Task retirement, staging reuse
and Metal admission controls pass without GPU use. No hang/corruption fix claimed;
no runtime/settings/pin/cache change or automatic game launch. Public default-head
refs checked October 23 09:18:09 UTC unchanged; PR/release review limits retained.
Read first handoff checkpoint.

Latest owner capture (October 23 09:11:07 UTC / 11:11:07 Madrid): GPU watchdog
after 132.309 s, cooperative exits 21/31. Three scheduled Metal command buffers
lack completion callbacks; previous batch completed. Fresh stall allocation is
11.067 GiB, no recorded OOM. Journal retains 128/129 guest candidates, not GPU
execution breadcrumbs. Source probes retain 128 valid newest-first observations
covering 1.613296 s with complete budgeted footer; 1920 more ring records omitted.
No repeated NaN pointer in this retained sample, no CPU fault; prior corruption
remains unresolved. 1214 API presentations, 474 pipeline/805 SRT errors. Audio
queue waits report zero failures. No memory saving or compatibility fix established.
No code/config/dependency change or game launch; see first handoff checkpoint.

Prepared source follow-up fixes oldest-first partial history: output is newest-
first with explicit metadata, analyzer still orders sequence IDs. A reproduced
closed-output SIGPIPE diagnostic bug (6/100 before, 0/100 after) is repaired with
Darwin descriptor-scoped suppression before cancellation startup and fatal output;
no process-wide signal disposition change. Next ignored command uses seven source/
header observations at three reviewed entries plus matching fatal fields, replacing
tree probes. Actual linked bytes/bounds/parser checked. Six CPU/five field-analysis
tests, full contracts and 46/46 imports pass; GPU unchanged. Native source capture
and SIGSEGV cause still pending. No pin/cache changes or automatic game launch.
Read first handoff.

Latest owner capture (October 23 08:59:55 UTC / 10:59:55 Madrid): CPU pointer-read
SIGSEGV after 91.851 s, exits 139/139. Early minimum, native regs and exact guest
instruction/context work. Source pointer word matches two binary32 NaN patterns;
writer/type/lifetime/CPU-GPU cause unknown. One reporter, no nested notice. Full
history partial: 596 events/no footer; minimum latest sample is unrelated tree
source. 1109 presentations, 418 pipeline/805 SRT errors, no recorded OOM; last/peak
native presentation sample 10.370 GiB. Memory drops/errors zero in last summary.
Next retarget source initialization/reuse and retain a recent bounded tail; no
skip/null-pointer substitute. No code/config/dependency change or game launch.
Read first handoff.

Prepared fatal-report correction: every unrecovered entry attempts a compact
minimum before rich mapping/provider diagnostics, including nested reporters
before immediate exit. One nonblocking bounded pipe write records native PC/LR,
thread identity and one recent atomic ring observation; no guest reads, allocation,
locks, waits or retries. Full/closed/unsupported output may omit evidence. Real
signal exits and recovery paths retained. Combined report exposes truncation and
minimal identities independently of native/guest completeness. Six CPU/six report
tests, nested/full/closed controls, actual fatal entry in both modes, full contracts
and 46/46 imports pass. GPU unchanged; native cause/minimum validation pending.
Same focused private command next; no pin/cache change or game launch. Read first
handoff for limitations.

Latest owner capture (October 23 08:49:05 UTC / 10:49:05 Madrid): CPU SIGSEGV
after 95.328 s, exits 139/139; two concurrent/nested fatal notices truncate report.
Focused five probes and matching binaries active; host register block complete,
but no exact guest instruction/context or CPU history. Cannot distinguish multiple
failing threads from reporter recursion. 1149 API presentations, 451 pipeline/805
SRT errors; no recorded OOM, 9.580/9.602 GiB last/peak presentation samples.
Last memory summary drops 4908 buffer records, so full buffer-history claims fail.
No shader rejection; owned shader capture and focused retention remain unvalidated.
Next prioritize bounded minimal history and explicit truncation, never wait on a
faulted reporter. No code/config/dependency change/game launch; see first handoff.

Prepared decoder follow-up saves a bounded failure window from active owned
shader bytes only, never rereading the original guest pointer; first evidence and
exit 23 preserved. Combined diagnostic retains specific decode reasons and flags
post-run presentation gaps even with window-close exit zero (not a live watchdog).
Next private command uses five reviewed root/link observations instead of 21 to
reduce repeated traffic; actual retention/overhead pending. ARM64/all 22 renderer
CPU checks, installed-decoder evidence controls, five report tests, full contracts
and 46/46 imports pass. No opcode fix or native freeze/corruption resolution is
claimed; no pin/cache change or game launch. Read first handoff.

Latest owner capture (October 23 08:39:07 UTC / 10:39:07 Madrid): main compute
shader unknown-format rejection inside an owned supplied slice after 151.065 s,
exits 23/23; not prior silent freeze/metadata/CPU-fault stop. Frozen history works:
2048 records, zero in-window skips, but 95 million attempts retain only 12.879 ms.
Tree probes exercised; no writer/program relation proved. 1473 API presentations,
422 pipeline/805 SRT errors; no recorded OOM/large copies, 10.195 GiB last/peak
presentation sample, not failure-time. Combined report matches reconstruction;
its generic assertion classification omits decode-specific reason present in log.
Next: bounded shader-source provenance and narrower tracing; no skip/fake opcode.
No runtime/settings/dependency changes or game launch; read first handoff.

Latest owner capture (October 23 08:25:33 UTC / 10:25:33 Madrid): apparent freeze,
then requested window closure at 547.666 s, exits 0/0; no recorded fatal/watchdog
stop. Automatic diagnostic.json present and matches reconstruction. Last of 1525
API presentations at t=156.8915 s; audio continues over six minutes after resource
progress stops. 414 pipeline/805 SRT errors; no recorded OOM, last/peak native
presentation samples 9.114/9.145 GiB. 38 approximately 78 MiB copies complete;
last periodic 2/2 counter is stale. No CPU dump on window close, so new history
bridge/tree evidence remains unvalidated. Need thread stacks and independent
render/CPU/submit progress capture; exit zero does not mean success. Read first
handoff. No runtime/settings/dependency changes or automatic game launch.

Prepared combined failure capture: shader-metadata and Metal-command stops now
retain/freeze host CPU history before further evidence work, preserving exit 23;
optional host absence and valid/speculative paths remain normal. Metadata snapshots
include the scanner's previously omitted 28-byte tail. Each graphics run produces
private diagnostic.json with CPU/GPU/shader/memory/audio evidence, both rotating
presentation files and eight separate prior runs; no shared cause is inferred.
Two metadata/three journal/three combined-report tests, full contracts, ARM64/all
22 renderer CPU checks and 46/46 host imports pass. Native shader/pointer causes
remain unresolved; owner tree command is next. No pin/cache change or game launch.
Read the first handoff checkpoint.

Latest owner capture (October 23 08:11:06 UTC / 10:11:06 Madrid): required
fragment-shader metadata rejection during indirect draw, after 165.141 s, exits
23/23. Matching binaries/tree config active; direct metadata _Exit bypasses CPU
history hook, so freeze/node evidence remains unvalidated. 1588 API presentations
across both rotating files, 462 pipeline/805 SRT errors; no OOM/large copies,
10.507 GiB last / 10.522 GiB peak presentation-sampled native allocation.
Mapped backing/failed bounded scan do not prove corruption or a valid shader.
Next: history on this stop and program provenance/lifetime/write visibility.
No code/config/dependency changes or game launch; see the first handoff.

Prepared node-origin follow-up freezes newly entering CPU observations before
fatal context reporting, avoiding ring churn from other threads during output;
in-flight callbacks may still finish, with no thread suspension/wait. Recovered
faults stay unfrozen. New ignored command selects 21 observations at six reviewed
traversal/root/link entries with identical-sample suppression and new fatal
container/node windows, replacing broad array capture. Real config parser and
linked executable bytes/bounds checked; five native CPU tests/eight-producer
freeze controls, full synthetic contracts and 46/46 imports pass. Earlier renderer
checks retained; native validation/corruption correction pending, no automatic
game launch or pin/cache change. See the first handoff checkpoint.

Latest owner capture (October 23 07:57:24 UTC / 09:57:24 Madrid): translated
CPU SIGSEGV after 160.533 s, exits 139/139, invalid linked-node key comparison;
not the previous array fault or a GPU watchdog stop. Matching binaries and fatal
history exercised: 1441 records cover only 4.366 ms after 61 million attempts;
array probes omit the new root/link source. Retarget before another comparison.
1470 API presentations, 444 pipeline/805 SRT failures; no OOM/large copies,
9.905 GiB last presentation-sampled native allocation, not fault-time. Image
journal exceeds old cap with zero drops in last periodic summary; no memory
saving inferred. No code/config/dependency changes or automatic game launch.
Read the first handoff checkpoint; writer/lifetime/CPU cause remains unknown.
Prepared follow-up to the latest watchdog: cooperative cancellation now attempts
128 atomic CPU observations on nonblocking output, stopping at the first failed
write; thread-local SIGPIPE blocking preserves exit 21 with a closed reader.
The analyzer distinguishes missing, partial and complete dumps. GPU journals
retain 128 guest draw/dispatch candidates separately from state notes; image
event file cap is 32 MiB, not a guest/Metal memory reduction. Five native CPU,
four analysis, two submission-journal and 22 supervisor tests, full JIT/interpreter
contracts, ARM64/all 22 renderer CPU checks and 46/46 symbols pass. Pins/cache
unchanged. Private array command stays selected; native validation is pending.
No automatic game launch. Read the first handoff checkpoint.
Latest owner capture (Oct 23 07:39:10 UTC / 09:39:10 Madrid): GPU watchdog
after 159.147 s, awaited batch has two scheduled/uncompleted Metal chunks; real
cooperative child/supervisor exits 21/31, notice after 0.909 ms. 1,336 API
presentations, 477 pipeline/805 SRT errors; no OOM/large copy. Fresh stall-time
native sample 10.023 GiB. Image journal drops 7,229 records. Array trace config
is active, but cooperative _Exit bypasses CPU history dump; no emitted summary
does not mean no observations. Next: bounded nonblocking stop history and exact
awaited-batch candidates, not a fabricated shader culprit/longer timeout. No
automatic game launch. Read the first handoff.
Owner-requested next command now uses a separate private array-state selection:
18 observations at three reviewed entries, six source/header fields, identical
loop suppression and array-relative fatal windows. Actual config parser and
executable/ignore checks pass; public pre-test heads unchanged. No new code,
dependency/cache change, native array validation or automatic game launch.
Read the first handoff; do not repeat the prior formatting-focused command for
this changed boundary.
Latest owner capture (Oct 23 07:33:36 UTC / 09:33:36 Madrid): translated array-
pointer SIGSEGV after 96.320 s, exits 139/139, same instruction as 08:24 Madrid,
not strlen/shader decode. Exact filtered selection/current UUIDs exercised:
1,581,784 observations rejected, zero selected/ring events; native positive
matching remains absent. Current fatal formatting fields omit the array source.
1,145 presentations, 439 pipeline/805 SRT errors; no OOM/large copies, 9.557 GiB
last/max presentation sample. Writer/ownership/type/CPU cause unproven. Review
array object state/lifetime next; no automatic game launch. Read first handoff.
Prepared owner follow-up fixes trace saturation by optional saved-register
match against an image-relative value, before guest reads/cache/ring reservation.
Private filtered-format command uses 18 observations/five reviewed entries and
removes the broad parser probe; original source fields remain unconditional.
Four native CPU tests in both modes, actual matching/rejection exits, 100000-call
negative control, two analysis tests/full contracts and 46/46 symbols pass.
Only diagnostic selection is corrected; shader/string causes remain unknown.
Runtime is reaffirmed as permanent direct upstream: 30-commit compare reviewed,
existing tessellation adaptation retained, FSR/adapter migration tracked and lossy
eviction excluded. No pin/cache change or automatic game launch. Read first handoff.
Latest owner capture (Oct 23 07:19:39 UTC / 09:19:39 Madrid): owned-main-shader
unknown-format rejection after 94.992 s, exits 23/23; no strlen/sample assertion
recurrence or proof of their fix. 1,150 presentations, 453 pipeline/805 SRT
errors; no OOM/large copy, 9.991 GiB last / 10.025 GiB peak presentation sample.
Renderer assertion history hook is exercised. Broad format probes generate
3,318,674 records, retaining only 0.332 s and no targeted source-field site.
Do not repeat that selection unchanged; narrow/filter before another comparison.
No new executable/dependency/config change or automatic game launch. Read first handoff.
Latest owner capture (Oct 23 07:12:03 UTC / 09:12:03 Madrid): translated CPU
SIGSEGV after 83.739 s, exits 139/139, first guest-libc strlen read through an
invalid string. Candidate static/stack path is vsnprintf warning formatting;
no writer/ABI/corruption cause proven. 1,075 presentations, 473 pipeline/805 SRT
errors; no OOM/large copies, 9.678 GiB last/max presentation sample. Prior object
history does not cover this source. Separate ignored format-only 23-observation
selection is prepared for the owner's command, with real parser/executable-range
and three native CPU controls passed. Public pre-test heads unchanged. No new
code, pin/cache change or automatic game launch. Read the first handoff.
Latest owner captures (Oct 23 06:59:22 / 06:57:48 UTC): renderer RawNumSamples
assertion after 84.545 s (23/23), preceded by a different invalid-pointer CPU
SIGSEGV after 69.558 s (139/139). The new CPU history works but is bounded and
does not identify a writer. No OOM/large copy; latest 422 pipeline/805 SRT errors,
9.837 GiB last presentation sample, not assertion-time. Checked sample conversion
now rejects unsupported fallback instead of returning a count absent from the
mask, and logs requested count/mask/caller role on failure. Renderer assertions
also retain atomic CPU history through an optional weak hook. Valid conversions,
real exit 23, cache 28/9 and pins stay fixed. ARM64/all 22 renderer CPU controls
and 26 builder tests pass; no native correction claimed or automatic game launch.
Read the first handoff for measured limits and validation.
Owner-requested field history is prepared/privately activated: generic bounded
SOTC_CPU_FIELD_TRACE at reviewed block entries, 2048 atomic retained observations,
optional identical-sample suppression only at hot consumers, and explicit gaps.
No guest bytes/returns/fields are changed; no exhaustive writer/lifetime claim.
Three native CPU tests (both modes), two analysis tests and 22 supervisor tests
pass; read the first handoff for full validation and native limits. Header update
Vulkan-Headers 1.4.366 is integrated, with loader 1.4.365 independently selected.
ARM64/all 21 renderer controls/26 builder tests/46 host symbols pass; cache 28/9
and base pins unchanged. No new native game validation or automatic game launch.
Latest owner capture (Oct 23 06:29:14 UTC / 08:29:14 Madrid): targeted fields
are active and exercised. SIGSEGV after 69.639 s, exits 139/139, at the same
null-read instruction as 06:09:33. The captured source field and its neighboring
word are zero; writer/initialization/lifetime remain unknown. Other configured
fields stop safely because their base is null for this boundary. 984 API
presentations, 327 pipeline failures, 271 SRT errors; zero large copies/OOM.
Last native sample 8.937 GiB, not failure-time. No semantic runtime change or
automatic game launch follows. Read the first handoff for evidence and limits.
Latest owner capture (Oct 23 06:24:04 UTC / 08:24:04 Madrid): a different
translated CPU pointer-read SIGSEGV after 86.294 s, exits 139/139; 1050 API
presentations, 432 pipeline failures, 805 SRT errors, zero large copies.
The log records SOTC_CPU_FAULT_FIELDS=null: targeted capture was absent. The
private-file loader has moved to the common packaging --graphics-game-test
entry, covering both .command launchers and direct packaging invocation.
Nine real-entry stub controls pass, stopping before any driver/GPU action.
Private field selection covers both observed source-field boundaries. No native
validation or corruption correction is claimed. See the first handoff.
The owner has activated the targeted fatal source-field capture. The native
command reads optional private work/cpu-fault-fields.txt as data, unless
SOTC_CPU_FAULT_FIELDS is explicitly set (empty disables it). Exact selection
remains ignored/private. Actual-launcher stub controls pass without a game/GPU.
No new native run or crash correction is established; older inactive wording
below describes preparation before this activation.
Newest owner capture (Oct 23 06:09:33 UTC / 08:09:33 Madrid): balanced-v2 and
current UUIDs are exercised; translated CPU SIGSEGV after 80.855 s, exits 139/139.
Private disassembly identifies a null object argument from an earlier field load;
that source field is beyond the default snapshot, and its writer is unknown.
There are 1136 presentation results, 377 pipeline failures and 271 SRT errors.
Last native sample 9.557 GiB, no recorded OOM or 3/3.5 GiB upload; two shorter
mapping-boundary copies share a 640 MiB upload. All 246 observed import hits
also fit v1, so this run does not isolate v2's added coverage. Original 1920-case
pointer/call controls pass in both modes with pinned and local CPU bridges.
No safe semantic correction is established. A private inactive source-field
template is prepared; do not skip the read or invent an object. No game launch.
Prepared follow-up (Oct 23): balanced-v2 moves the middle import origin to
1.75 GiB, keeping the rejected 1.5 GiB experiment withdrawn. The selected
6592 MiB pool still uses three imports totaling 7616 MiB. Actual-planner replay
of 10,390 observations covers 28 v1 misses with no observed direct-hit loss;
this predicts binding coverage, not native memory savings. A synthetic long-tail
range demonstrates the remaining placement tradeoff and full-copy fallback.
ARM64/all 21 renderer CPU checks/26 builder tests/three replay tests and full
JIT/interpreter contracts pass. BB_POOL_LAYOUT=balanced-v1 restores the exact
preceding geometry; legacy/single controls remain. Cache 28/9 and pins unchanged.
No native run validates logos or OOM avoidance; read the first handoff.
Latest owner capture (Oct 23 05:46:41 UTC / 07:46:41 Madrid): Metal OOM 8 after
4.033 s, exits 23/23, 11.905 GiB at failure. A slightly-over-3-GiB range misses
complete import coverage and reserves 3.5 GiB staging. One large-copy request,
zero completions, no journal drops. 20 presentations, zero pipeline failures,
50 SRT errors; early stop cannot validate prior shader/CPU fixes. Current binary
UUIDs match. Read the first handoff; keep the failed 1.5 GiB-origin experiment
withdrawn and do not automatically launch another game.
Current prepared cache is 28/9. Guest memcpy/memset now use the selected runtime's
pre/post CPU-write notifications and opt-in CPU watch. Portable SRT code is
structurally validated before execution; full-width/zero-width bitfield extraction
is defined in both the VM and constant folding. Original VM overflow/wrong-read/
UB controls reproduce; 2,244 actual valid cases/twelve rejections, ARM64/all 21
renderer checks/26 builder tests/full JIT+interpreter contracts pass. Native
SIGSEGV, PerVertex and dynamic SRT fixes remain unproven. Pins and DMA stay fixed.
Read the first handoff and latest public-source review; no automatic game launch.
Current owner capture (22:26:02 UTC / Oct 23 00:26:02 Madrid): translated CPU
SIGSEGV after 80.900 s, exits 139/139. Private source-field evidence narrows an
invalid pointer read; no writer/cause proven. 881 presentations, 423 pipeline
failures, 805 SRT errors, zero large copies, last native sample 9.497 GiB. The
intervening run still has OOM/a 3 GiB snapshot. Withdrawn uncompiled 1.5 GiB-origin
proposal fails a prior range control; restored 2 GiB geometry and synthetic
limitation-band controls pass. Read the first handoff; no automatic game launch.
Latest owner capture (22:16:18 UTC / Oct 23 00:16:18 Madrid) ends on an unknown
main-shader instruction after 116.574 s, exits 23/23: 943 presentations, 423 pipeline
failures, 805 SRT errors, zero large copies, last native sample 9.596 GiB. Windowed
startup is exercised; no CFG/OOM/native signal stop is recorded. Main translation
now owns a bounded copy of supplied code through compilation; this protects
post-capture source reuse, not atomic capture or already-corrupt input. Read the
first handoff. Cache stays 27/9 and pins unchanged; no automatic game launch.
Current cache is 27/9: SetPC uses defined unsigned arithmetic, CFG rejects invalid
boundaries before block indexing and emits explicit fatal provenance. Original
UBSan defect control, 60 targets/eight valid graphs/nine rejection cases and
ARM64/20 renderer CPU checks/25 builder tests pass. No native shader fix is
established; read the first handoff. Older cache numbers below are historical.
Newest owner boundary (22:02:30 UTC / Oct 23 00:02:30 Madrid): shader CFG::LinkBlocks
missing-target assertion after 118.513 s, exits 23/23; zero large copies, last
native sample 10.379 GiB. No OOM/SIGSEGV recorded; shader cause unknown. The
capture still has fullscreen=1. Read the first handoff for the active boundary.
The owner removed Game Mode integration: use the direct executable, windowed
default and BB_FULLSCREEN=0 in the native command. No app-bundle/plist startup.
Development has resumed at the owner's explicit request. Balanced imports have
native evidence of avoiding the large snapshot; the latest boundary is host
SIGSEGV outside translated code. Bounded ARM64 register/LR diagnostics and startup
image UUID/range inventory are prepared. Read the first handoff checkpoint.
Game execution remains owner-supervised; no automatic game launch follows.
The latest owner run (21:54:46 UTC) exercises balanced imports and zero large
snapshots, last native sample 8.917 GiB, but stops on unmapped host instruction
SIGSEGV outside translated code after 82.902 s, exits 139/139. No OOM is recorded
and no corruption cause is established. Read the first handoff checkpoint.
Newest targeted memory work uses balanced imported-pool windows to avoid a
recorded 3 GiB fallback snapshot. Direct bindings and BDA use actual window
origins; retained 219-observation replay avoids the giant copy with no new
observed crossing, at +512 MiB imported overlap for this pool. Native savings
are unmeasured. BB_POOL_LAYOUT=legacy restores old geometry; cache stays 26/9.
Read the first handoff checkpoint; do not automatically launch a game.
The owner authorized targeted memory work after the pause: completed CPU task
frames now retire via ordered recording packets at all seven release sites.
Read the first handoff checkpoint. Delayed-consumer CPU/sanitizer controls pass;
native effects are unverified. This does not authorize automatic game execution.
The earlier development pause is historical; the owner has now resumed work.
Read the current handoff before the historical pause record. Latest owner capture (21:16 UTC)
ends on Metal OOM 8 after 185.769 s, exits 23/23, 12.170 GiB; 417 pipeline failures
and 805 SRT errors persist. Do not automatically resume game/device comparisons.
The following checkpoints describe historical evidence and prepared local work.
Cache is now 26/9: checked finite-choice SRT data-read expansion preserves GPU
selection and protects descriptor/pointer dependencies. CPU/build checks pass;
native effects are pending. PerVertex conversion passes Apple syntax for 16 real
pairs in isolation; renderer integration is still absent. Latest owner run
(20:57 UTC, before this adaptation) stops on zero-filled compute-shader metadata
after 78.854 s, exits 23/23; preserve that failure. Read the first handoff.
Newest owner capture (20:53 UTC) verifies native 1920x1080 scene work, no reduced
draws/proxies, 1,325 presentations, then Metal OOM 8 after 123.887 s (23/23),
11.966 GiB. Preset 4 is inherited but upscaler none disables reduction. SRT remains
271; PerVertex failures persist. Image journal drops 8,861 records. Read first handoff.
The owner now selects original scene resolution: preset 0, upscaler none.
Use prueba_grafica_nativa.command to clear inherited 640x360 overrides. DMA and
async stay off. Prior instructions/captures retaining 640x360 are historical;
no native result has yet validated this resolution change.
Prepared SRT correction accepts ReadFirstLane only of proven portable uniform
expressions; 24 synthetic installed-pass cases pass, including induction/lane
negative controls. Cache is now 25/9; pins remain fixed. Staging statistics refresh
on repeated large-buffer reuse without freeing storage. Read the first handoff;
native SRT/pixel/OOM effects remain unverified.
Newest owner capture (20:40 UTC) ends on Metal OOM 8 after 65.352 s, exits 23/23,
11.573 GiB fresh allocation and 680 presentation results. Native SRT provenance
now records 52 samples/eight shaders/six bounded shapes, including Phi induction
recurrences. Do not resolve these by choosing an edge or enabling DMA. Read the
first handoff checkpoint; earlier 20:08 evidence below remains historical.
Pipeline/SRT classification is prepared: 431 failures / 386 shader pairs;
375 have adjacent PerVertexKHR errors, 56 remain unattributed. The checked SRT
diagnostic emits bounded shader/stage/site/IR shapes without guest reads or
resolution changes. CPU tests pass; native samples remain pending. See the
first handoff checkpoint; no cache purge or direct-memory enablement follows.
The consolidated checkout is the only active project. Read
`docs/guide/workspace.md` and the continuation checkpoint before resuming.
The latest 20:08 UTC owner capture ends on Metal OOM 8 after 122.026 s,
confirmed exits 23/23, fresh native allocation 12.469 GiB. Admission is active
(limit/peak 8), but four reserved/inflight commands remain at failure. The
3 GiB upload persists; 485 live images / 1,163.47 MiB and 23 proxies remain.
There are 1,190 presentations: count present_result, not all three JSON lifecycle
events. The preceding 19:53 capture had 1,115 presentations, not 3,345. Audio has
a 15.012 s delivery gap without queue failures. No CPU/decoder/watchdog stop
recurs; their causes remain open. Read the newest handoff; no native fix claimed.
The newest 19:53 UTC owner capture ends on translated SIGSEGV after 118.281 s,
confirmed exits 139/139; the native admission cap is exercised (peak 8). The
preceding 19:49 UTC capped capture still ends on Metal OOM after 121.100 s.
An invalid loaded pointer's source field is outside the previous four-word
snapshot; no writer/lifetime/translation cause is proven. The prepared optional
SOTC_CPU_FAULT_FIELDS reader parses bounded register-relative windows before
threads, preserves guest/GPU guards, rejects address overflow and keeps fatal
exit behavior. Default empty; owner selections/disassembly remain private.
Read the first handoff; do not hardcode game fields, skip loads or claim an OOM
or corruption fix. No new game test is performed for this preparation.
The owner confirms reaching the first gameplay image on Apple M1; the port is not
playable yet. The integer-blit overlay is built and CPU-contract-tested, with
same-format nearest 2D uint/int copies and explicit unsupported-case errors.
The native comparison no longer reports the two float-output/integer-target
errors; exact GPU copy pixels and full-case coverage remain unverified.
This is not an OOM/corruption fix.
Read the first handoff checkpoint and preserve the existing testing guards.
The newest 18:22 UTC owner comparison ends on Metal OOM 8 after 100.149 s,
confirmed exits 23/23 and fresh native allocation 12.434 GiB; the decoder failure
is not reproduced. The checked native admission overlay defaults to eight
reserved/inflight commands in CommitCommandBuffer, releasing slots on actual
completion only; no early split, fake fence or resource eviction. CPU/Foundation
bridge and blocked cooperative-cancellation controls pass, as do ARM64/seventeen
renderer regressions/25 builder tests, full JIT/interpreter contracts and the
46/46 host-symbol audit. Native
memory/performance effects are unverified; eight is not a measured optimum.
SOTC_METAL_INFLIGHT=0 is the uncapped control, 1..64 are explicit limits. Keep
pins/cache, retained references, scene 640x360 and the nested-consumer review.
The checked shader-decode overlay adds supplied-slice bounds checks, safe 64-bit
reads and main/fetch/copy failure provenance, with real exit 23 preserved.
Thirteen installed-decoder CPU cases reproduce a pinned guarded-page overread
and verify explicit adapted rejection. ARM64, sixteen renderer regressions and
25 builder tests, full JIT/interpreter contracts and 46/46 symbol audit pass;
native validation is pending.
SMRD slot 15 is unsupported by current tables, not automatically a new valid
instruction. Fetch synthetic ends/cache memcmp and borrowed-code ownership are
not fixed by these finite-slice guards. Do not claim the last assertion or OOM
resolved. Read the prepared checkpoint before another native comparison.
The earlier 18:06 UTC owner comparison records:
shader-decoder assertion at 95.239 s, confirmed exits 23/23, unknown opcode
1601 in SMRD family 0xc0000000. No recorded Metal OOM/watchdog/CPU fatal signal;
11.447 GiB sampled native peak, final pressure unknown. Preserve explicit decode
failure; unsupported versus corrupt/stale shader input is not distinguished.
Buffer journal saturation limits missing-event claims. Read the first handoff.
The preceding 17:57 UTC owner comparison records:
Metal OOM 8 after 77.670 s, confirmed exits 23/23, native allocation 11.603 GiB.
The weak report-time probe is exercised with status 5/error 8 and real callbacks;
it is diagnostic validation, not an OOM fix. The owner specifies 16 GB unified
memory and wants reduced graphics to lower total pressure. Scene work is 640x360
but original-size images/output and the 3 GiB upload remain; do not equate Metal
allocations with additional dedicated VRAM. Read the first handoff checkpoint.
The new native-state probe adds weak, report-time status/error/device allocation
observations alongside callbacks. CPU journal and actual ARC weak-bridge fixtures
pass, together with the ARM64 build/full contracts; the error path is exercised
in the latest owner comparison. Stall-time probing remains unverified. Never
interpret missing callback status=0 as a sampled NotEnqueued state, or unavailable
native memory as zero pressure. No timeout/order/ownership behavior changes.
The newest 17:29 UTC owner comparison supersedes older boundaries below: GPU
watchdog after 116.824 seconds, confirmed child/supervisor exits 21/31. The awaited
batch has a 26,020-note CPU summary; commit returned, scheduled/completed callbacks
were not observed. Native sampled peak is 11.485 GiB, final pressure unknown.
Image replay records 448 live / 1,111.45 MiB, 817 proxies destroyed / 23 live.
No large staging-tail/shared-copy event, OOM, CPU or audio queue failure occurs.
Prior memory/corruption causes remain unresolved. Do not identify an executing
shader from those CPU notes. Read the current handoff first.
The native Metal retention overlay now requests retained references for initial
and split batches. Production-method Foundation CPU doubles check explicit-resource
lifetime, independent completion, reset and scratch return. ARM64, fourteen
renderer regressions, 25 builder tests and full JIT/interpreter contracts pass;
native validation is pending. This does not fix the installed MoltenVK driver or prove indirect/BDA
ownership, OOM avoidance, corruption resolution or stall recovery. Preserve the
past-next, residency, staging, watchdog and failure-reporting guards.
The submission journal now retains full-batch note summaries and first/last
guest draw/dispatch candidates, with monotonic host observation timings.
ARM64 build and full contracts pass; native validation is pending. Counts are CPU
metadata, not draw totals or GPU execution progress. Do not treat this as a stall fix.
The busy-queue maintenance overlay is integrated, with dependencies/cache 24/9
unchanged and native proxy destruction observed. The latest owner run (15:27 UTC)
stops on GPU progress; the supervisor ends after 136.225 seconds, exit 1, with
child exit unconfirmed despite its cancellation notice. Process-status lookup is
denied; confirm the old process/window is closed before another comparison.
Image replay records 1,037 proxies destroyed / 26 live and 464 total live images /
1,135.69 MiB. Native allocation peaks at 11.584 GiB before the stop; final pressure
is unknown. The 3 GiB upload and registered-image growth remain. The preceding
Metal OOM, SIGBUS allocator corruption and audio queue failure remain unresolved;
none recurs in this capture. Read the current handoff first.
Bounded all-port audio failure snapshots and candidate frame-local CPU reads
are now built and contract-tested; native validation of these additions is pending.
Keep input, selected sources,
installed libraries and new diagnostics under this root. Previous build/source
snapshots are historical; do not launch from an older folder or generated cache.
If moving this checkout, run `tools/relocate_workspace.py` before rebuilding.
The private `work/consolidation/state.json` records the actual root and validation.
The current dependency integration supersedes older pin/cache instructions below:
runtime b2ff9cd0, CPU b9652d75 and reference 411fdd86 are selected, with shader cache
24/9. Read docs/DEPENDENCIES.md and tools/dependencies.json for every library,
latest reviewed version, exact source SHA and open proposal. Keep the inventory
current whenever dependencies or reviewed upstream versions change. Runtime,
CPU and reference updates are integrated at the owner's explicit request;
unmerged PerVertex proposals and alternative driver architectures remain tracked
separately. Preserve the local scene-proxy, coroutine, shader/SRT, audio and staging
corrections when updating upstream. The new GC guard preserves unresolved GPU
contents; record those retention decisions separately from no-pressure downloads.
The versioned workspace key is an integration identifier, not the required HEAD;
the builder verifies the exact selected b2ff9cd0 revision independently.
Use docs/DEVELOPMENT_HANDOFF.md for the current reproduction steps, stopping boundary and debugging priorities; keep it current when those change.
Use https://github.com/bloodborne-native/bloodborne-mac/ as the user's preferred upstream reference when resolving doubts. Follow its runtime/recomp links and review relevant changes against our pins; do not assume Bloodborne-specific layouts apply to SotC or silently update dependencies.
The owner also permanently tracks AjncSystem/ShadowOfTheColossus-PC and
luizgustavs/Pink-shadPS4, alongside bloodborne-mac/runtime/recomp. Resolve current
branches and compare selected and last-reviewed SHAs in tools/dependencies.json;
docs/UPSTREAM_REVIEW.md records the five-source review and decisions. The new
dcaf500 instanced Metal tessellation correction is a checked local adaptation,
not a base-pin update. SotC 0127e36 now questions completed child-frame release;
review queued consumer ownership before another native comparison. Existing CPU
reclamation tests do not establish asynchronous consumer safety. Do not import
Pink's hardcoded heap/stack workarounds or runtime ab31a4c's lossy image eviction.
Also review bmy/bbport-mac, MoltenVK, SPIRV-Cross and KosmicKrisp as requested by
the owner. Read docs/DEPENDENCY_WATCH.md and record branch/SHA changes; the daily
public-source watch does not update pins and is currently paused. The owner also
requested close tracking of https://github.com/boykopovar/AnyPS5/issues/1;
its separate hourly public-source watch is enabled. Review linked PRs against
our PS4/native ARM64 contracts; PS5/Rosetta results are not SotC/M1 validation.
Before each new native game/GPU comparison, refresh public upstream state,
starting with AjncSystem/ShadowOfTheColossus-PC and the relevant tracked runtime,
compiler and driver sources. Resolve current default branches and compare new
commits/releases/PR diffs with the pins and prior reviews. Record the check and
any test-relevant candidates before proposing or running the comparison. Do not
silently update dependencies. This pre-test review is separate from the daily watch.

- Maintain code in runtime/src, tests, tools and packaging. work, game and logs are private
  local output/input; never promote their generated content into Git.
- Never publish game binaries, assets, extracted/generated game code, game addresses,
  execution logs, screenshots, recordings or saves. Original synthetic fixture sources are
  allowed. The game input must remain unchanged.
- Use the pinned upstream submodules. Read their instructions before editing them. Preserve
  credits to bloodborne-native, deadinside28/bbport, shadPS4 and the SotC reference project.
- Prefer real runtime contracts with explicit error behavior. Unknown imports stop; missing
  modules remain unavailable. Do not add blanket-success stubs to claim startup progress.
- Keep JIT/interpreter comparisons and meaningful guest ABI tests. Run contracts-test after
  runtime changes and native CPU checks when changing the CPU context bridge.
- The visual journal defaults on in the combined launcher; pixel readback remains
  opt-in via SOTC_VISUAL_CAPTURE=1. Read docs/VISUAL_DIAGNOSTICS.md for limits and
  evidence semantics. Scene stays 640x360. ARM64 build/CPU checks pass; owner runs
  exercise metadata, but pixel capture remains untested and memory still fails.
  bmy's 4096-region copy batching is adapted
  with an original-vs-adapted CPU capacity regression. PerVertex proposals remain
  isolated: docs/PERVERTEX_REVIEW.md records draft status and synthetic conversion
  results, not runtime integration or correct M1 pixels.
- Read the new scene-proxy checkpoint first: the checked retirement overlay removes
  stale UID/mip proxies during OnSubmit, with the draw pipe drained. Images AND
  views remain owned through DeferOperationPastNext (real completion at current
  tick + 1). Clear recent pointers, tracked UIDs and increment scene generation.
  Role/UID/mip evidence distinguishes active/pending/destroyed proxies. The
  production-method CPU regression retains 204 proxies before versus 2 active/202
  destroyed after correction; this is synthetic, not native memory savings.
  The ARM64 incremental build, seven renderer source regressions, 28 builder tests,
  symbol audit and full JIT/interpreter contracts pass. Native validation is pending.
  This adapts bmy's 8185b88d stale-proxy detection without its GPU-wide Finish.
  Keep the coroutine leak candidate separate and the 2.5/3 GiB staging guards intact.
  Read docs/UPSTREAM_REVIEW.md and docs/DEVELOPMENT_HANDOFF.md.
- The combined launcher also defaults SOTC_MEMORY_TRACE=1. Read
  docs/MEMORY_DIAGNOSTICS.md: bounded private buffer origins, real image
  allocation/destruction, queued deletion and GC summaries now accompany the
  existing visual report. The ARM64 build, six renderer source regressions,
  27 builder tests and full JIT/interpreter contracts pass. The newest 09:41Z
  owner run stops after 101.864 seconds on Metal OOM 8, child/supervisor exit 23.
  Its single cross-window copy allocates 2.5 GiB staging, not 3 GiB. The complete
  uncapped image history proves 981 unassociated images/806.35 MiB remain live
  without any such destruction; all are 640x360 or 320x180. Registered inventory
  is 223 images/670.28 MiB. No image deletion is queued at failure. Retained
  per-UID SceneTargets proxies are a strengthened candidate; allocation roles
  are still untagged. Keep cache 23/9 and all resource/fence/import semantics.
  The last GC sample is 35.14 seconds old; read the current handoff checkpoint.
  GC kept/freed upstream samples reset with frame statistics: never interpret
  them as cumulative totals. Reject untracked renderer source/header copies.
- The SotC reference is still pinned at 2250f328. On 2026-10-22 main is observed
  at 411fdd86, 72 commits ahead; v0.60.5 is not integrated as a whole. Its c9be8142
  nested-coroutine leak correction is absent here and is a candidate for a
  separate adaptation. The associated PR 219 remains open/non-draft/unmerged
  despite incorporation into reference main. Read docs/UPSTREAM_REVIEW.md;
  CPU heap measurements from the author are not Apple M1 or Metal OOM evidence.
- The owner now reports audio much improved. Combined diagnostics enable SOTC_AUDIO_TRACE=1
  for bounded per-port timing/queue metadata and output errors. Read
  docs/AUDIO_DIAGNOSTICS.md; input-queue zero is not hardware-underrun evidence.
  The 07:23Z run measures about 47,684 input frames/s per port, up from 23,912,
  and confirms the actual 1024-frame/48 kHz device quantum. Gaps still reach
  121.85 ms; complete playback and audiovisual alignment remain unverified.
  Keep real PCM/error contracts.
- The earlier 08:50Z owner comparison after rollback restores visible output,
  according to the owner, but ends after 94.305 seconds on Metal OOM code 8,
  child/supervisor exit 23. Dedicated upload is one retained 3,072 MiB buffer,
  first observed by 7.784 seconds and unchanged through failure. From the first
  sample after 10 seconds, all 483.55 MiB of tracked Vulkan growth is image
  growth; this is not proof of a leak. Native allocation reaches 11.66 GiB.
  Keep the withdrawn experiment inactive, cache 23/9, scene 640x360, DMA/async
  off and the audio/staging corrections. Read the newest handoff checkpoint.
- The preceding 08:36Z owner run ends after 160.610 seconds on Metal OOM code 8,
  with confirmed child/supervisor exit 23. The owner reports no logos or visible
  scene despite working input. The guarded-buffer-bound experiment records zero
  applied reductions and is withdrawn, including its metadata/cache layout changes.
  The ARM64 renderer is rebuilt from the prior checked overlays, binary/meta
  versions 23/9; source regressions and full JIT/interpreter contracts pass.
  Keep the audio and staging-reuse corrections. At that checkpoint restored
  visible output was unverified; the 08:50Z owner comparison supersedes that limit.
- The earlier 07:23Z run confirms native OOM after 137.642 seconds, child/supervisor
  exit 23, at 11.70 GiB native allocation. It exercises four oversized staging
  reuses; dedicated upload inventory is 2,560 MiB versus 3,072 MiB previously.
  Total staging is 2,972 versus 3,500 MiB, but image allocations grow later and
  memory exhaustion remains. The last staging snapshot is 31.8 seconds old:
  reported reusable bytes are not permission to free live resources. Investigate
  the single 2.5 GiB upload allocation and image lifetimes without changing
  ownership/tick/import guards. Read the newest checkpoint; earlier summaries
  below are historical. Keep all pins and settings.
- An earlier owner-run preset-4 capture confirms Metal out-of-memory failure after
  83.45 seconds: actual NSError domain/code/description, 11.65 GiB native allocation
  versus 11.84 GiB recommended, and a complete two-compute-candidate command tail.
  The completion-error handler self-exits; child/supervisor exit 23 is confirmed,
  without a GPU-watchdog stop. This validates failure handling, not the memory fix.
  Three later samples now contain reduced scene draws; correct pixels remain
  unverified. Keep scene 640x360, DMA/async off and all pins/cache version 23.
  Track allocation classes and safe lifetimes; the culprit resource is unknown.
  Imported windows overlap and device recommendations are not hard physical limits.
  Do not disable split imports or prematurely free retained large staging buffers:
  upstream records rendering/argument-buffer lifetime failures with those changes.
  General PerVertexKHR, dynamic SRT and clamps remain. Read the current handoff
  before the historical summaries below; tool GPU access/glslang are still blocked.
- Read the handoff's Continuation checkpoint first: it supersedes the older run summaries below. The owner now sees the 3D animation with defects. The latest preset-4 run reaches its 120-second wall deadline without a GPU-watchdog/import/CPU-fault report, detects a DualSense, and continues recording activity after connection. Cooperative child exit is unconfirmed (supervisor exit 1); a later targeted lookup finds the process absent. The authorized removal of the automatic game-duration limit is implemented: zero disables both game-duration timers, and the combined launcher defaults to zero and passes it to both layers. Keep GPU-progress cancellation, DMA protection, cooperative stopping and the independent prerequisite budgets. Headless probes still default to 30 seconds; explicit 1..1800-second game runs remain available. No real GPU experiment validates this change yet.
- The owner has visually confirmed game logos/menu, opening introduction audio and now defective 3D animation. Correct scene rendering and audiovisual synchronization remain unverified. Gameplay stability, a playable app and save support remain unverified. Keep claims tied to measured tests.
- The newest owner-run capture omits the low-resolution preset and uses a 30-second budget. It stops on the GPU watchdog after about 19.49 seconds, with unknown child status and supervisor exit 1 despite a cancellation notice; a later targeted lookup finds the PID absent. Its awaited batch retains 96 of 164 metadata notes, including eight guest compute dispatch candidates, transfers and one three-vertex draw. The last compute candidate also has 31 buffer-size clamp warnings. Its queue call returned success and its Metal chunk returned from commit, without scheduled/completed callbacks observed; the preceding chunk completed. These are CPU candidates, not the precise executing GPU command. The CSV is empty. No reduced-scene transition or physical-controller connection is logged. Preserve this changed configuration separately from the preceding complete low-resolution capture.
- The preceding complete owner-run low-resolution test uses a 120-second budget, detects a DualSense and applies 640x360 scene targets. It stops on the GPU watchdog after about 114.91 seconds, with confirmed child exit 21 and supervisor exit 31. Its retained 96-note tail contains indexed indirect draws from two shader pairs that also appear in PerVertexKHR pipeline errors. The full batch has 28632 metadata notes, not draws. No proof of the exact stalled pipeline variant exists. Absence of later shader/blit failures in the newest earlier stop does not validate those paths.
- The checked submission overlay has now produced a native trace. The combined launcher enables SOTC_GPU_SUBMIT_TRACE=1 by default; 0 disables it. A second local overlay rejects empty-handle cached pipelines and prevents null-handle Metal registration. The original source guards reproduce the bug and the adapted guards pass CPU fixtures; the ARM64 library builds. This fixes invalid state acceptance, not PerVertexKHR support or the black scene. Do not register real shader state under a null handle or claim cache acceptance proves drawable support. See the handoff's black-scene analysis and pending-submission section.
- The owner explicitly authorizes gradual low-resolution GPU tests. The configuration is BB_UPSCALE_PRESET=4, DMA off, async submission off and JIT/statistics on. Keep experiments brief and supervised; the latest 120-second-budget run stalls with DMA off. Imported memory remains 6592 MiB; the latest sampled Vulkan allocation/image maxima are 5304/1318 MiB, versus an earlier 5740/1418 MiB. These are not comparable macOS pressure measurements. Integer-format Metal copies, PerVertexKHR pipelines, SRT/Phi offsets, clamps and incomplete GPU-mapping warnings remain. Investigate typed copies and the stalled submission before another game run; do not suppress errors, fake fence completion or extend timeouts as a remedy. Do not use the Bloodborne-specific BB_RENDER_RES guest-patch path. This tool context exposes zero Metal devices; do not bypass access restrictions or claim it executed the owner's native GPU comparison.
- A host watchdog kernel panic has been reported. The owner has since authorized gradual GPU testing: one brief supervised experiment at a time, one renderer setting changed, then inspect the log before another run. Do not reproduce the earlier DMA-enabled hang or run an automatic sequence. Dynamic shader reads remain disabled and nonzero overrides are rejected before GPU loading; a process supervisor cannot guarantee recovery from a kernel/driver hang.
- The combined game and Metal prerequisite launchers use cooperative cancellation through diagnostic_control.h after an OS cleanup refusal. Keep external-signal fallback disabled in cooperative mode. An unknown child exit status is a failed stop; ensure the old diagnostic window has closed before another instance.
- Check the target remote before pushing. Keep private local state and archives ignored.
