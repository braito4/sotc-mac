# Private CPU fault diagnostics

## Current reader/index capture

The default is now base with the [reader/index profile](INDEX_CAPTURE.md): 24
pointer targets (the original 22 plus two numeric register observations) and 28
scalar channels, including eleven reassigned to the reader/index stages. Previous
profiles and binaries are preserved. All enabled journals total 1204 KiB. The
new native thread field occupies an unused payload word; legacy journals remain
readable. Synthetic controls pass; game coverage and root cause remain unproven.
The following profile and default descriptions preserve earlier stages.

## First native range comparison observed

The first `caller` owner run has matching input/function validation and a real
range-entry marker. It still stops with SIGSEGV outside that interval, repeating a
pre-variant traversal instruction. One run does not identify or exclude a CPU
translation defect. See the [checkpoint](SIGSEGV_CHECKPOINT.md) for the retained
comparison and source-coverage limits. The private default remains `caller`.

## Controlled function comparisons ready

The launcher now supports `base`, `caller`, `buffer` and `table`. Each selected
variant interprets one exact-input, hash-checked function interval while keeping
the remaining code on the JIT. The private default is `caller`. The pointer and
scalar selections below are unchanged. Full synthetic contracts pass, including
range boundaries, cached execution, nested calls and concurrent threads. No
native game run or corruption fix is established for these variants. See
[Controlled CPU function comparisons](CPU_FUNCTION_COMPARISONS.md) for commands,
validation evidence and interpretation limits. Earlier entries are historical.

## Current interior-probe and entry capture preparation

The current private profile has 22 pointer targets (21 distinct instruction sites)
and the unchanged 28 scalar probes. Four added observations cover an array's first
element through the object, the current entry before its load, a returned object's
member before argument loading, and the incoming numeric consumer argument.
The previous eighteen-target profile is preserved. Fixed journal storage is now
1172 KiB; the parser supports at most 24 targets. Persistent additions share the
existing bank zero, with explicit admission refusal and unchanged bounded storage.

A checked generator for the pinned JIT source ends translated blocks before
interior probes. Previously a probe within a block could be silently skipped.
The original CPU source remains unchanged; the diagnostic archive substitutes the
generated object. Registration is before execution, not dynamic after compilation.
A probed instruction executes through the existing interpreter dispatcher, so a
fault there can lack a translated terminal context. Earlier probe observations
must remain distinct from terminal evidence that is actually available.

Original controls reproduce the omission and verify flags, registers, repeated
loop visits and cached decoded blocks. Additional JIT/interpreter fixtures check
first/second invalid array entries, inaccessible arrays, returned members,
register-only observations, all 24 channels and bounded watch admission. Full
synthetic contracts pass without game input. Native pointer corruption remains
unresolved. The owner launcher applies `umask 077`; existing protected reads,
ordering and executable-memory protections remain in place. See
[JIT review](JIT_REVIEW.md) for the function map and external-project comparison.

The following eighteen-target and earlier profile entries are historical.

## SIGSEGV source capture resumed

The owner resumed this investigation after another atomic consumer fault. The
active private profile adds four pointer probes to the preserved fourteen-target
selection: caller entry, before the selected source load/call, after replacement,
and the incoming consumer register. All eighteen signatures and the unchanged
twenty-eight scalar-probe signatures match the actual prepared input. New source
watches use bank zero; admission remains bounded and can fail. The earlier
profile and the allocation-failure alternative are retained separately.

The offline report exposes `fault_pointer_review.terminal_memory_sources` as
protected terminal reads, separate from pre-failure observations. It deduplicates
labels for the same numeric field/value, records matching registers and signed
fault displacement, and bounds output. A terminal source match does not identify
the earlier load, writer, allocation identity or lifetime. Malformed, incomplete
and ambiguous contexts do not supply source evidence.

`test_terminal_memory_sources.py` covers these offline limits.
`test_pointer_release.py` runs ten original controls through the actual JIT and
interpreter: correct old/new atomic counts, a deliberately wrong count, an
already-invalid source slot, a direct overwrite and an imported-copy overwrite.
Actual continuation probes retain source and incoming-operand observations;
interpreter faults outside translated code do not acquire invented terminal
memory snapshots. Native correction remains unresolved. See the current
[checkpoint](SIGSEGV_CHECKPOINT.md). Older profile descriptions below are
historical; adding the pre-enqueue shader journal brings current fixed journal
storage to 1108 KiB for eighteen pointer targets.

## Pre-enqueue shader-prefix observations

The owner launcher enables `SOTC_SHADER_SUBMIT_TRACE=1` alongside diagnostic
journals. The runtime default is off. This diagnostic adds checked CPU backing
reads of eight bytes at fully covered SET_SH_REG origins before enqueue, after
the existing optional command copy. It does not retain guest memory or request
GPU readback. Up to 16 immutable per-submission captures survive coroutine
suspension; nested IB candidates use a checked backing reader with bounded depth
and work. Owned command copies use their existing bounded vector span.

`shader-submit-events.bin` has independent 512-record submission and consumption
tails. Consumption republishes the retained earlier prefix and timestamp, rather
than rereading code at that point. The file adds 132 KiB; old journal sizes stay
fixed. The host pool is below 512 KiB plus bounded shared-pointer control blocks.
Pool exhaustion, output initialization failure and disabled tracing skip new
observations without changing execution. Captures expose partial pairs, unavailable
reads, unrecognized layouts, repeated numeric packets, work/depth/sample limits,
contention and overwritten history. Conditional execution is not simulated.

The combined offline report includes `shader_submission_review` on a metadata
failure. It joins a complete address write to a matching consumed packet and
compares the saved eight-byte head with the lookup head only when numeric origins
agree. Changed addresses, missing/foreign/stale evidence and repeated packets do
not become content-change or writer claims. Neither matching heads nor a prologue
proves correct identity, allocation lifetime or whole-program stability.

`python3 tools/check_shader_submit.py` runs 19 original sanitizer controls;
`python3 -m unittest discover -s tests -p test_shader_submit_analysis.py` checks
original offline joins, absence, changed addresses/prefixes, both compute paths,
foreign PIDs and reader versions. No game, shader execution or GPU is involved.


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

## Bounded host coroutine-frame guard

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

## Persistent field admission and referring objects

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

## Simultaneous tree and pointer-array scenarios

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
Public default heads checked October 23 11:50:11 UTC unchanged; prior PR/release
review limitations retained. Dependencies, renderer/cache and game settings fixed.

## Targeted header and imported-copy tracing

The owner follow-up also enables SOTC_CPU_POINTER_TRACE from a private reviewed
profile. Each of up to fourteen observations is
site:register:offset:inner[:field-only[:watch-policy[:index-register:scale]]][@bank].
Watch policy 0 rotates sixteen fields, 1 requests non-evicting bounded admission,
and 2 records observations without arming a watch. An optional index applies to
the second chain address, with scale 1/2/4/8 and checked unsigned arithmetic;
field-only mode rejects an index. New events explicitly report admission, and
indexed probes save index metadata rather than a copy request.
Default protected reads obtain the header at register+offset and link at header+inner.
Optional field-only=1 requires inner=0 and reads only register+offset; it never
follows the resulting value. Such events carry status bit 512 and header zero.
Only pointer-valued fields belong in that mode; counts or vector data would
make the out-of-domain heuristic meaningless. Signatures
are checked against the actual linked input before game execution; code is not
patched. SOTC_REQUIRE_POINTER_TRACE=1 rejects a missing profile/selection before
launch. The report exposes each target's observed hits, including zero. This
checks capture readiness, not deterministic reproduction of a native crash.

pointer-events.bin adds independent per-target and pre/post-copy tails. Up to
sixteen observed link fields are watched for overlapping imported memcpy/memset
calls, including partial-byte copies. Events retain source pointer or fill byte,
destination, length, before/after values, numeric thread and optional protected
return-address candidate. The pointer snapshot preserves a first link whose low
56 bits exceed the 40-bit guest domain; a top-byte tag alone is ignored for
selection. It is a heuristic, not an invalid-access/type/lifetime verdict. No
execution is skipped, repaired or terminated by the tracker. The focus snapshot
survives rotating tails and subsequent valid values.

Current profile uses four header-chain entries, one persistent source-field
entry, one observe-only indexed reference and two copy channels at 128 records
each: 132 KiB, with all journals totaling **716 KiB**. The persistent table and
page filter add **642 KiB** of static host storage; this is not a total footprint. The observer does not
intercept direct JIT/interpreter scalar/vector stores, GPU writes or other aliases.
Writes before arming or after watch replacement can be missed. Copy records use
registered container/header relations without rereading the source; these are not
ownership identities. Overlap and changed values do not prove exclusive causality
under concurrent writers. Watch replacement/contention, guarded reads, absent
journals and unreached targets are reported. No native writer/correction or
runtime overhead benefit is established. Synthetic fault reproduction uses only
original fixtures; the game is not run by validation.

## Combined pre-failure capture

The earlier allocator-search profile added six observations, reaching the
then-current 24-site limit, with 13 fatal fields and **520 KiB** of mapped
CPU/shader evidence. The current profile above supersedes those totals. Probe reads are protected and are not exhaustive writer tracking.

A reserved CPU-journal header slot retains the first committed terminal register
snapshot. Kind 1 is the guest stack-protector import, kind 2 an available
translated-fault snapshot. Stack-protector imports preserve original exit 22;
translated faults retain their fatal status. The import snapshot reads only an
already initialized active CPU's TLS state. No decoder, stack-return dereference
or assumed canary location is used by snapshot publication. RIP/FS/stack bounds
and opaque thread IDs are numeric observations; caller/frame reads are separate
best-effort diagnostics. Interpreter host faults without translated registers
remain unavailable rather than using stale TLS state as exact fault evidence.

Publication precedes stderr and rich diagnostics; discarded/full/closed output
controls retain journal context. Interrupted publication and foreign producer
PIDs are rejected by the offline collector. No native corruption fix, complete
writer history or tracing-overhead improvement is established.


The current owner launcher enables SOTC_DIAGNOSTIC_JOURNAL=1 and
SOTC_SHADER_PROVENANCE=1 and loads a private combined CPU selection, unless the
corresponding environment variable is explicitly supplied. An empty CPU override
still disables probes. The current selection prioritizes allocator-tree root and
child traversal, with earlier comparator/source-pointer observations retained.
Guest offsets and disassembly remain private. Earlier selection descriptions
below are historical and are not the current launcher defaults.

cpu-events.bin retains 128 observations independently for each configured CPU
site; shader-events.bin retains 512 applied program writes and 512 changed
metadata lookups. Both are fixed MAP_SHARED files created exclusively with mode
0600 in the fresh per-run state directory. Current private selection: 520 KiB
combined. Original stderr dumps are retained. No extra guest reads or driver
calls are made by journal publication. Initialization is ordinary startup work;
no signal handler initializes or queries a shader mutex/file mapping.

The offline diagnostic includes pre_failure_journals with per-channel wraps,
incomplete/stale slots, busy/delayed drops, suppression and retained time spans.
Committed observations survive process termination without a final flush or
hook. Producer-PID mismatches against the child log are excluded. Use a fresh
state directory; PID matching alone cannot distinguish PID reuse. Files are
never overwritten. A requested but absent journal is an explicit evidence gap.
An incomplete CPU text dump is independent from retained pre-failure evidence.

This prepares CPU faults, shader stops and fiber guard failures together; existing
GPU-watchdog reporting remains active. It does not identify exhaustive writers,
unique objects, memory ordering or GPU execution, nor guarantee kernel-failure/
power-loss durability. Monotonic timestamps and numeric matches are observations,
not causality. Native overhead and coverage require the next owner capture.


## Recent evidence under output backpressure

Ring dumps emit newest reservation sequences first, with `newest_first=1` in
metadata. A prefix truncated by a nonblocking write therefore preserves recent
observations. The offline analyzer reorders by reservation ID; that order still
cannot prove actual guest writes or CPU/GPU completion. Older logs keep their
original order/limits. Tests exercise changing values, the newest 128 of 300
observations and partial reverse-order output with explicit missing footer.

A closed-output stress control reproduced SIGPIPE in 6/100 cooperative stops
with worker masking alone. On Darwin, F_SETNOSIGPIPE now suppresses it on the
shared diagnostic output descriptor before ready/worker startup, also in early
fatal output, while retaining worker-local masking. The same control passes
100/100 after the fix and native fixtures verify the descriptor policy. No global
signal disposition changes. It is a diagnostic exit fix, not a corruption fix.

The current private source selection replaces the previous tree probes with
seven reviewed observations before and inside the source-pointer boundary.
Unconditional observations preserve repeated calls/address reuse within the finite
ring. Incoming versus later observed values can differ without identifying a
writer, unique allocation or intended object type. No stored pointer is followed
and no guest data, invalid read or return value is repaired/skipped.

## Early fatal minimum

An unrecovered fatal entry first freezes observations and attempts `CPU fatal
minimum`, before the first-reporter guard and rich mapping/context queries. A
second/nested reporter attempts its own minimum then keeps immediate termination.
It contains signal/address, native PC/LR and POSIX reporter-thread identity, plus
at most one recent committed ring observation found in an eight-slot search.
`sample=0` means no sampled record in that search, not an empty ring. A retained
observation can belong to a different thread. No saved pointer is followed and
no native PC/LR is promoted to an exact guest instruction or validated stack.

A fixed buffer fits PIPE_BUF and uses one nonblocking pipe write without retries,
locks, allocation, guest/provider reads or waiting on another reporter. Sockets
are best effort too. Closed-reader SIGPIPE is blocked only in the terminating
thread. Full/closed/unsupported output can drop the minimum; regular files and
terminals skip it. Supported stderr output stays nonblocking for subsequent fatal
reporting, which may be partial. Normal execution/recovered faults are unchanged.
The combined report preserves minima and identities, flags concurrent/nested
truncation, and separates native-register completeness from guest-report completion.
Synthetic nested-signal, output-backpressure and actual JIT/interpreter fatal
controls preserve exit 139 and original observed field values. This diagnostic
cannot itself repair corruption or guarantee complete evidence.

## Opt-in history at configured code sites

The current owner follow-up reduces the private tree selection from 21 to five
observations at previously reviewed root/child-link entries. The latest broad
selection attempted 95 million records while retaining only 12.879 ms. Removing
redundant observers aims to reduce churn; it sacrifices detail and does not
establish native overhead/coverage improvements before the next comparison.


`SOTC_CPU_FIELD_TRACE` configures at most 28 observations as
`image-offset:register:signed-field-offset[:on-change[:match-register:match-image-offset]]`,
separated by commas.
Numbers use the existing bounded parser; registers are decimal 0..15. Optional
on-change is 0 (default: retain each observation) or 1 (suppress identical
per-thread samples at that site/field). For example, an original synthetic
selection is `0x40:7:24,0xa:3:24:1`, not a recommended game layout.

The optional final pair selects only calls whose `match-register` equals
`bb_image_base + match-image-offset`. Supply on-change explicitly before the
pair, for example synthetic `0x40:7:24:0:6:512`. Register bounds, image bounds
and address overflow are checked before execution. Matching compares the
register itself, without dereferencing it. A rejected observation increments
`filtered` and returns before protected reads, timestamps, cache updates or ring
reservations. Matching is evidence selection, not pointer/lifetime validation;
probe callbacks still incur overhead. Existing three/four-field syntax remains
valid. Filtered counts are separate from attempted ring reservations and loss.

The previous broad formatting selection attempted 3,318,674 observations and
retained only 0.332 seconds. Its replacement privately selects the original
warning's format at four formatter entries, keeps the source block unfiltered
and removes the broad parser entry: 18 observations at five reviewed entry sites.
Four native CPU tests exercise matching/rejection in JIT and interpreter,
including actual loader exits, vector/byte preservation and 100,000 unrelated
callbacks rejected before guest reads without evicting selected history. This
repairs diagnostic selection; no native shader/string crash fix is established.

The shared graphics entry reads optional private `work/cpu-field-trace.txt`
unless the variable is explicitly set; empty disables it. Exact game sites and
fields stay ignored/private. Sites are image-relative and validated against
executable ranges before provider initialization; they must also be reviewed
function entries or basic-block entries. An arbitrary instruction inside a
translated block may not execute a probe. Registration uses the selected CPU's
existing probe API before guest threads. No guest bytes, return slots, field
values, CPU semantics or resource lifetimes are modified. Probes and protected
reads can change timing; their runs are diagnostic evidence, not benchmarks.

Each observation reads one configured 64-bit field through the same protected,
GPU-guard-aware reader, and records site, dynamic object/field addresses, value,
readability status, host-thread trace ID and monotonic observation time. It never
follows the value as a pointer. Status bits distinguish valid address arithmetic
(1), successful read (2) and available time (4); an unreadable zero is not a zero
value read from memory. Host errno is preserved. There is no per-event file I/O.

A fixed 2048-record atomic ring is dumped at a terminating signal, including
interpreter faults without translated register context. The dump takes no locks,
allocates no memory and reads no guest fields. Interrupted/replaced slots are
skipped, not waited on. Ring overwrite, busy drops and skipped slots are explicit.
Optional on-change suppression uses a fixed per-thread cache and a separate
suppressed counter; constructor/destructor sites should retain all observations.
Cache collisions cause additional samples. Changes/ABA between observations,
address reuse and missing earlier history prevent treating this as an exhaustive
writer watch or a complete allocation/lifetime history. Sequence IDs order
observation reservations, not actual writes or CPU/GPU completion.

Before a confirmed fatal signal prints context, an atomic termination flag stops
newly entering observations from reading fields or reserving ring slots. Renderer
assertions and successful cooperative stop notices do the same before dumping.
Recovered speculative/GPU faults return before freezing. This reduces overwrites
by unrelated guest threads while a fatal report is printed. Already-entered
callbacks may still publish: there is no wait, guest-thread suspension or atomic
guest-state snapshot. Metadata reports `frozen=1`; it does not promise zero skipped
slots or repair guest memory. Eight-producer synthetic controls verify retained
pre-stop observations, no protected reads/reservations after freeze, unchanged
errno/registers, and the actual fatal/cooperative paths preserve exits 139/21.

The renderer assertion handler also invokes an optional weak host hook before
its real exit 23. The hook dumps only the already retained atomic history, once
per process for assertions. It does not read guest fields or fabricate a CPU
register context. Standalone hosts without the hook still terminate normally.
Concurrent termination can truncate a dump; this remains best-effort evidence.

Required shader-metadata and native Metal-command stops also invoke the host's
once-only freeze/history hook before further evidence work. The ordinary macOS
renderer error path uses optional loader lookup; it is not entered from signal
handlers. Standalone hosts can omit the hook. These paths keep exit 23 and do not
substitute metadata, retry work or fake command completion. Actual wrapper tests
cover hook present/absent and ensure valid/speculative paths never request a dump.

Cooperative cancellation now attempts the latest **128** ring observations
after its stop notice. The terminating worker first verifies a nonblocking
pipe/socket/terminal output and blocks SIGPIPE in that worker only, preserving
exit 21 even when the output reader closes. A full pipe, failed descriptor setup
or regular-file output skips this history; it never waits for space, flushes
stdio, allocates, locks, accesses the GPU or rereads guest fields. The loop stops
on the first failed/short write. This does not add a cancellation grace period.

`CPU field trace dump` metadata precedes events and names the snapshot end,
event limit, termination `frozen` flag and `budget_omitted` observations outside
that selected tail.
The footer separately reports older-than-ring history, budget omission and
uncommitted/replaced slots inspected in the selected window. Missing metadata
or footer can result from backpressure. Signals/assertions retain the full
2048-record limit; cancellation uses 128 to keep the stop path small. Every
path is a best-effort concurrent snapshot, not a paused guest history.

The offline analyzer reports `dump_state=complete`, `partial` or `not_dumped`.
Zero event lines without a complete footer do not establish an empty ring.
Original native controls use 300 selected observations and verify the 128-record
cooperative budget plus fatal exit behavior with drained, full and closed
output pipes. Ordinary JIT/interpreter field and filter controls remain intact.

The owner-requested private selection covers initialization, loading, destruction
and use of the source field. Static inspection found an initialization path
that legitimately writes zero; its execution by the failing object remains to
be established. The next native capture can distinguish observed empty-after-
loading from an observed later clear, but cannot by itself identify an unobserved
writer. The October 23 06:57:48 UTC owner capture exercises it: 32,715 attempted
records, 2,048 retained, 30,667 outside the ring and 54,864 identical hot-site
samples suppressed; no busy drops, skipped slots or malformed/duplicate records.
The retained window spans about 21.835 seconds. Its sampled nonzero-to-zero
transitions all end at an initialization observation and may include address
reuse. This is not evidence of a corrupting clear. This capture terminates on
a different invalid object pointer, outside the originally targeted null-field
boundary; selected fatal fields have no matching retained history. The later
renderer assertion does not dump history with the old binary. The new assertion
hook has CPU controls, but its native game path remains pending.

Original UBSan/bounds controls use the actual selected probe API in JIT and
interpreter: function-entry and call-continuation observations, an unprobed
control copy, moving objects, retained vector/byte/errno state, protected reads,
overflow, parser limits, concurrent ring wrap and nonblocking incomplete slots.
Actual diagnostic-loader fixtures preserve fatal exit 139 and dump before/after
synthetic field values in both modes; invalid/nonexecutable sites stop before
execution. ASan reserves the normal guest address ranges on this host, so the
native protected-address fixture uses UBSan/bounds rather than remapping the
production guest ranges to accommodate it.

Private offline grouping:

```sh
python3 tools/analyze_cpu_field_trace.py logs/your-capture.log \
  --output work/analysis/field-history.json
```

The analyzer reports sampled transitions and missing histories, preserving
unreadable samples and dropped records as uncertainty. It does not name a writer
or equate a reused field address with one allocation. Outputs must stay in `work/`.

## Captured terminating field evidence

The October 23 06:29:14 UTC owner capture (08:29:14 Madrid) exercises the
configured-field reader. The repeated null-read fault ends the run after
69.639 s, exits 139/139; its earlier source field and neighboring word are zero
in the protected snapshot. This provides source-field evidence, not a writer,
initialization, lifetime or translation diagnosis. Other selected fields stop
safely at a null base for this boundary. Exact layouts/addresses stay private;
see the first handoff for the current result and evidence limits.

The October 23 06:09:33 UTC owner capture (08:09:33 Madrid) stops after 80.855 s
on translated CPU SIGSEGV, exits 139/139. The null argument comes from a preceding
object-field load. Its source lies beyond the four-word snapshot, so no writer,
initialization or lifetime cause is established. Original 1920-case field/call
controls pass in both modes with the pinned archive and local context/vector
bridge. An inactive private template uses the existing configured-field reader
for the missing source; no game offsets enter public code, no null check is
inserted into the game and no runtime correction is claimed. Read the first
handoff for the current memory/shader/audio evidence. Earlier boundaries follow.

The latest owner boundary (21:54:46 UTC, October 22) is a host instruction
SIGSEGV outside translated code after 82.902 s, exits 139/139. The old capture
cannot supply its missing native register/return context. The new bounded
host diagnostic records ARM64 x0..x28, FP/LR/SP/PC without following pointers.
A startup image UUID/load/text-range inventory supports offline candidate
matching with `tools/analyze_host_fault.py`; this is not stack unwinding or a
corruption fix. All addresses and inventories stay in private logs/reports.
See the first handoff checkpoint for current validation. Earlier boundaries below
are historical.

The newest captured CPU boundary (**19:53 UTC on 2026-10-22**) is a translated
SIGSEGV after **118.281 seconds**, with confirmed exits 139/139. Private offline
disassembly relates the invalid loaded pointer to an earlier field load outside
the existing four-word register snapshot. Its source value/writer/lifetime remain
unknown. The preceding capped run still ends on Metal OOM; neither failure is
declared fixed. See the current [handoff](DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

## Optional register-relative field windows

The latest activation route is the shared
`packaging/test-arm64.sh --graphics-game-test` entry. It covers both command
files and direct packaging invocation. The 06:24:04 UTC capture recorded no
selected fields; moving the loader out of the dedicated command closes that
entry-path omission. Nine real-entry stub controls pass without loading a driver
or launching a game/GPU. Explicit environment precedence remains unchanged.

At the owner's request, `prueba_grafica_nativa.command` reads optional private
`work/cpu-fault-fields.txt` as the field selection for the existing fatal reader.
The file is data, never sourced shell code. An explicit `SOTC_CPU_FAULT_FIELDS`
environment value wins; an empty value disables the private selection. Exact
game-specific fields stay ignored/private. Activation does not execute a game,
trace prior writes or change guest behavior; it only adds bounded evidence at
a later terminating translated fault. The inactive template mentioned above
describes the earlier preparation, before owner activation.

`SOTC_CPU_FAULT_FIELDS` adds targeted evidence at a terminating translated fault.
For example, `12:0x130:2,3:-16` reads two 64-bit words at r12+0x130 and one at
rbx-16. These are generic syntax examples, not game layouts or recommended fields.
The default is empty. Register indices are 0..15 in `BbGuestRegs` order (rax,
rcx, rdx, rbx, rsp, rbp, rsi, rdi, r8..r15). At most sixteen comma-separated
entries and eight words per entry are accepted; offsets are signed byte counts
in decimal or `0x` hex. Spaces, incomplete entries and out-of-range values are
rejected before loading input, with configuration exit 24. Parsing happens once
before guest threads; there is no environment access or parsing in the handler.

Each entry prints the chosen register/offset and a `configured_field_candidate`
window through the existing protected reader. Address wraparound is rejected;
unmapped, guarded, non-guest or short reads stay explicit. No automatic pointer
chase, synchronization, memory write, permission change or recovery is added.
Candidate words can race with other threads and are not validated members. The
fatal signal retains its original exit status. Interpreter-mode register state
remains unavailable. Exact owner selections and output stay private under `work`.
The supervisor records the explicit field setting and native admission override
in its private settings header; unrelated environment variables remain excluded.

The original synthetic fixture verifies a field beyond the old snapshot, an
unaligned field, signed-offset and address arithmetic limits, configuration
bounds, GPU-guard exclusion and unchanged source bytes. Actual-loader tests
check JIT field output, interpreter unavailability, invalid startup input and
retained exit 139; the reader is also checked under UBSan.

## Earlier observations

The latest owner run (14:58 UTC on 2026-10-22) stops on Metal OOM after 120.309
seconds without a CPU-fault report. It provides no new fatal context and does
not validate the new candidate frame-local reads or resolve earlier corruption.

The latest captured CPU boundary is a translated-code **SIGBUS after 120.001 seconds**,
with child/supervisor exit 138. The new context completes and captures readable
resource-name text where a guest allocation path expects node metadata. That
word explains the saved index, scaled offset, stored value and failing address.
Eight frame candidates have preceding calls, including two indirect calls whose
targets are not established by the snapshot. This strengthens a corrupted/stale
node hypothesis without identifying the writer or lifetime, or excluding earlier
translation or ordering faults. The preceding 182.664-second SIGSEGV was a load
at a different instruction and lacked these memory observations. See the
[continuation checkpoint](DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

## Additional evidence at a fatal stop

The rebuilt ARM64 diagnostic now emits a bounded `CPU fault context` section
after the existing translated register report. It adapts the pinned
bloodborne-runtime probe's protected field/stack reads, with local bounds and
GPU-page exclusions. It runs only after alignment, GPU-tracking and speculative
read recovery have declined the fault. Normal guest execution has no new reads,
hooks, watchpoints or per-instruction recording.

For a translated fault it records:

- Up to four 64-bit words at each distinct general-register base, excluding the
  stack/frame pointers. This exposes nearby metadata without assuming a game
  allocator or following arbitrary object pointers.
- Up to sixteen words at the guest stack pointer.
- Up to eight frame-pointer candidates, including image-relative return values
  only when they fall within the actual prepared image bounds. Backward/cyclic,
  unaligned, unreadable or excessively distant frame pointers stop the walk.
- Eight protected words immediately before each frame candidate, labeled
  `frame_locals_candidate` (at most 64 additional words). They can retain nearby
  caller-local data, but are not decoded as parameters or validated object layouts.
  This addition passes CPU contracts; native capture is pending.

Reads are restricted to the platform's guest user/low address ranges. The
existing `bb_read_memory` uses protected host reads, and tracked GPU pages are
skipped before that call. A short/unavailable read is explicit; no fabricated
zero words replace missing data. The code does not change memory protection,
resolve GPU contents, wait for GPU completion, acquire a VMA lock or modify
the guest. Formatting and storage are bounded, with no new heap allocation.
A concurrent/nested fatal fault terminates with a truncation marker rather than
waiting for another reporter. The original fatal exit remains `128 + signal`.

These are best-effort observations while other threads may still be running,
not a synchronized snapshot. Frame values are candidates, not a proven call
stack: optimized code and tail calls may omit frame pointers, and values inside
the image can refer to data. No interpreter registers are inferred from ARM64
host registers; outside translated code the additional context says unavailable.
The existing Metal failure/collection journal is not flushed from this CPU
signal handler, because taking renderer locks there could deadlock.

All emitted words, addresses and derived analysis stay in ignored private logs
and `work`. Never commit or publish them. No game-derived constants, executable
bytes or structure layouts are embedded in the implementation or fixtures.

## CPU validation

`tests/test_cpu_indexed.c` runs **8,064 original synthetic cases per CPU mode**:
masked metadata indices, all four scales, positive/negative displacement,
64-bit high bits and modular arithmetic, unaligned metadata, all sixteen data
alignments and exact-width reads beside inaccessible pages. The expected value
and effective address come from independent synthetic inputs. JIT and interpreter
both pass; this rules out no broader concurrency or translation defect.

`tests/test_cpu_fault_diagnostics.py` tests the real native protected reader,
partial/guarded/unmapped reads, untouched memory, duplicate bases, cyclic frame
candidates and a synthetic word in the new preceding-frame region. An original
synthetic image exercises the actual fatal
loader path in both CPU modes: exit 139 is retained, JIT context contains the
synthetic metadata and return candidate, and interpreter context is explicitly
unavailable. No GPU, game input or sound is used.

Both suites are included in `bash packaging/test-arm64.sh --contracts-test`;
the indexed-load suite also runs under `--test`. The ARM64 diagnostic and native
emitter/JIT/vector checks pass. The 14:23 UTC owner run now exercises the added
diagnostics on a native SIGBUS; no correction to the underlying crash is claimed.

### Failed field reads and fault-register history

The offline field analyzer preserves unavailable protected-read samples separately
from readable values, including rejected field-address arithmetic. The combined
private diagnostic includes at most sixteen recent unavailable samples and eight
recent matches per nonzero guest register, with explicit omitted counts. Matches
compare a captured register numerically with observed values, object addresses
or valid field addresses. An unreadable sample's placeholder value is excluded
from value matching. Timed unreadable samples count toward the retained span.
Multiple guest register snapshots are marked ambiguous instead of merged.

These matches help locate candidate root/link sources in a targeted private
capture; they establish neither valid object identity nor the writer or lifetime
cause. A failed protected read remains a failed read. Bounded output, suppression,
address reuse and unobserved changes can hide relevant history. Exact selected
sites remain in ignored local configuration, not public documentation.

### Fiber guard failure capture

A mismatching fiber stack marker retains the real exit 23. Before terminating,
the diagnostic attempts bounded numeric fiber/stack limits, RSP, expected/observed
marker, state/flags, entry/CPU RIP, hex-encoded fixed-length name and register
metadata, then freezes/dumps its existing field history. Capture limits remain:
metadata is not validated ownership, numeric bounds do not prove mapping or a
stack high-water mark, and RSP is sampled at a host-call boundary. Marker damage
with RSP inside bounds is not proof of stack exhaustion. No game stack is enlarged.

The executable owns a strong history callback; standalone contract binaries use
a weak metadata-only fallback. Pipe/socket writes are bounded and nonblocking,
with no retries and Darwin descriptor SIGPIPE suppression. Full/closed/unsupported
output can omit evidence. New fields appear in private diagnostic.json; older
fiber stops are classified with an explicit missing-context gap. Retained history
and in-flight observations do not provide an exhaustive writer trace.

### Non-UTF8 private logs

The standalone field CLI now replaces undecodable log text and records the
replacement-character count, while preserving valid ASCII observation lines.
This matches the combined report's tolerant reading. Replacements can affect
other text; they are neither missing-field reconstruction nor evidence that
guest memory was corrupted. The original log is never rewritten.

### Targeted shader-register provenance

For the current missing shader metadata investigation, the prepared owner command
uses SOTC_SHADER_PROVENANCE=1 and clears SOTC_CPU_FIELD_TRACE, while retaining
fatal-field context. This avoids repeating unrelated high-frequency tree probes.
The independent shader recorder stores bounded CPU mirror observations, emitting
on required metadata failure. It is mutex-based and is not read by signal
handlers. Numeric descriptor or packet pointers are not allocation identities or
original guest-buffer provenance. See the first handoff for limits and controls.
