# SIGSEGV investigation checkpoint

## Reader/index capture prepared after five base controls

The fifth base run (21:56:49 UTC, 187.698 seconds) ends on a PM4 parser assertion,
not shader metadata or a CPU fault. The five base results are two SIGSEGV, one
SIGBUS, one shader rejection and one PM4 assertion, independently archived.

The new [reader/index capture](INDEX_CAPTURE.md) retains fast input, returned index,
saved-value stages and reader context. All original pointer targets remain; eleven
scalar targets are reassigned and two numeric-only targets added. The old profile
and executable are preserved. CPU/native thread identities are now separately
retained in existing payload space. Full synthetic contracts and final targeted
controls pass; native coverage remains pending. The default comparison is now
base. No index clamp, discarded command, permission change or game launch is
part of this preparation. Earlier series used the preceding profile.

## Fourth base and index acquisition follow-up

The 21:47:26 UTC owner run validates base and stops after 193.590 seconds on
compute-shader metadata rejection (23), without a terminal CPU fault. The sampled
eight-byte prefix already lacks a recognized prologue before enqueue and matches
lookup about 1.141 seconds later. This does not establish whole-program stability
or a shared cause with the index failure. Four base runs now contain two SIGSEGV,
one SIGBUS and one shader rejection; captures remain independently archived.

The preceding SIGBUS index is followed statically to a four-byte buffered reader,
its output local, unsigned register load, saved slot and scaling. A captured local
also retains the candidate reader argument. Its cursor, buffer and input bytes are
not captured. Fast reads copy the four bytes directly; a separate refill path also
exists. Fourteen fixed-stride initialized positions strengthen the anomalous-index
lead without proving the complete runtime type/format or original index history.
Reader-position/content versus refill/transformation or later overwrite remains
unresolved. Checked slices and an observation proposal are saved privately; no
active probes or selections were changed during this investigation.

## Third base control: terminal indexed address reconstructed

The 21:44:33 UTC owner run validates base and exits 138 after 153.780 seconds,
with SIGBUS on a translated read from a range reported without permissions. Two
captured stack slots retain an object base and a scaled value. Checked code's
scaling/address formula reconstructs the fault address exactly from those values.
This is stronger than an unassociated invalid pointer, but remains terminal
consistency rather than an exhaustive history or proof of the original index.

Static review connects index preparation to a four-byte reader and an output
local. Its runtime cursor/input bytes and the original writer are not retained.
Parsing position, content, transformation and prior corruption remain open; no
index clamp or protection change is justified by this alone. The first three base
results are two SIGSEGV and one SIGBUS. Capture and checked slices are archived;
analysis leaves runtime selection unchanged and excludes the newer incomplete log.

## Second base control: invalid fixed-stride iterator

The 21:41:07 UTC owner run validates `base` and exits 139 after 191.572 seconds.
A translated load uses an invalid iterator plus a member displacement. Checked
static code obtains begin/end from object fields and advances by a fixed stride;
the relevant source fields are outside retained terminal reads. Original writer
and iteration remain unknown. A candidate return has a checked preceding call,
without turning frame candidates into a validated stack.

The first two base runs both stop with SIGSEGV at different sites, with matching
validation and settings apart from frame-log paths. Continue comparable base
repeats before drawing an improvement or shared-cause conclusion. The complete
capture is independently archived; analysis changes no runtime selection. A newer
run without a completion footer at review start is excluded from this result.

## First base control: guest libc string-argument fault

The 21:38:07 UTC owner run validates `base`, with no selected interpreter range,
then exits 139 after 170.869 seconds. It faults on the first byte load through an
invalid incoming argument in linked guest libc, distinct from the earlier host
renderer-copy fault. Exact prepared bytes and module/export metadata are retained.
A candidate caller loads the argument from an object member, but that source field
is outside the terminal captured words; no original writer is established.

Prepared-input hashes match the five caller runs. One base control is insufficient
for an improvement claim; collect comparable repeats. Analysis leaves the default
selection unchanged, so base uses its explicit launcher argument. A newer log
without a completion footer at review start is excluded from this result.

## Five caller runs collected; base control series next

The 21:33:10 UTC owner run still uses `caller`, validates and enters its interpreter
interval, then ends after 184.040 seconds on the GPU watchdog (child 21 /
supervisor 31). Two scheduled buffers remain without completion after about ten
seconds, with no explicit native error. This resembles the earlier three-buffer
stall without proving the same cause or a precise failing GPU instruction.

The five-run series contains two SIGSEGV, one SIGBUS and two GPU stalls. Every run
validates and enters the same interval; input/function hashes and recorded settings
match apart from the per-run frame-log path. These are descriptive counts, not an
improvement rate. A labelled base series with the same route/settings is the next
control. All captures are privately archived; the default selection remains caller
and explicit `base` is required to select the comparison.

## Fourth caller run: SIGBUS in renderer copy work

The 21:31:23 UTC owner run validates and enters `caller`, then exits 138 after
70.990 seconds with SIGBUS in host libc copy code. The fault is a read from a
range reported without access permissions; translated guest context is absent.
The UUID-matched renderer associates LR with a ParallelFor task-call continuation,
not a direct memcpy call. A nearby read-only-copy warning covers the fault address,
but exact task identity, original arguments and a full stack remain unproven.
Mapping/protection/lifetime versus source-size issues need separate investigation.

Four caller runs now contain two distinct translated SIGSEGV sites, one GPU
watchdog stop and this renderer-copy SIGBUS. All validate and enter the selected
interval. Memory failures persist; neither a common cause nor a translation fix
is established. Captures are independently archived. A comparable base series is
the next useful control; analysis leaves the current selection unchanged.

## Third caller run: another invalid iterator read

The 21:28:18 UTC owner run validates and enters `caller`, then exits 139 after
168.041 seconds at another translated instruction outside the selected interval.
An invalid RBX directly supplies the faulting read address. Exact-input static
review identifies a fixed-stride traversal whose initial iterator and end come
from object fields; neither relevant field is retained in terminal reads. The
loop includes an indirect callback. Its target, iteration and original writer
remain unknown. Candidate repeated returns match a static recursive call, which
does not establish stack overflow or a validated runtime call stack.

Three caller runs now give two SIGSEGV sites and one GPU stall, with matching
input/function validation and actual interval entry in all three. There is no
established correction or common cause. The active selection is unchanged for
the owner's planned repeats; preserve this capture independently before base
comparisons. A new observed site alone does not establish a variant regression.

## Second caller run: GPU watchdog ends observation

The 21:24:55 UTC owner run validates and enters the same `caller` interval, with
identical input/function hashes. It ends after 187.466 seconds with child 21 /
supervisor 31, without a recorded SIGSEGV. Three buffers in the awaited submission
have scheduled callbacks and no completion after about ten seconds; the previous
submission completed. Native status remains scheduled, with no explicit Metal
error. Recorded command candidates do not isolate a failing GPU instruction.

The two caller results are now one SIGSEGV and one GPU stall. The watchdog ends
CPU-fault observation, so this is not evidence of correction. Keep caller for the
owner's planned repeats and compare base later; the active selection is unchanged.
The complete capture and comparison are preserved separately as a GPU stall.

## First native caller comparison: entry confirmed, SIGSEGV persists

The owner's 21:21:08 UTC run stops with SIGSEGV after 187.376 seconds. Exact-input
and function hashes pass, and the runtime confirms actual entry into `caller`'s
interpreter interval. The terminal fault lies outside it, in a translated
allocator-like traversal reading an invalid node pointer plus a member offset.
The same instruction and displacement failed in the earlier 14:41:04 UTC run;
prepared-input hashes also match. This route predates the comparison.

Static input bytes and the candidate caller's preceding call are checked. Initial
root versus loop-link loading remains ambiguous, with no retained original writer
or matching terminal source field. Watch admission and journal coverage remain
bounded. This one run establishes neither a global fix nor innocence of the
selected function. Keep the profile and compare repeated `caller`/`base` runs
before attributing timing or a changed failure to translation. The capture and
comparison are preserved privately; analysis changed no runtime selection.

## First controlled function comparison activated

Three reversible variants are implemented: indexed-argument caller, local-buffer
owner and table generator. Each switches one reviewed lexical interval to the
existing interpreter; callees outside it retain the JIT. `base` retains the
ordinary diagnostic JIT. The private default is `caller`, with exact prepared
input and whole-interval hash checks before execution. The prior executable and
profiles are retained. Pointer/scalar probes and protections are unchanged.

Full synthetic contracts pass in JIT and interpreter modes, including selected
range entry, block/cache boundaries, preserved state and invalid-access controls.
No game/GPU run was launched for this preparation. Native correctness and
performance remain untested; a different stop or timing alone cannot establish a
fix. Review the per-run validation and first actual range-entry marker before
interpreting results. See [comparison instructions](CPU_FUNCTION_COMPARISONS.md).
The candidate-count and previous-run sections below preserve earlier findings.

## Latest native-copy fault and bounded candidate inventory

Three subsequent owner runs stop with SIGSEGV. A store through an invalid pointer
has the same value in a protected terminal read of its source member; no earlier
value or writer is retained. Another run repeats a null-object member read seen
in older logs. The latest stops inside host libc, outside translated code.
Its saved LR matches the diagnostic memcpy wrapper's call continuation in the
UUID-matched binary; preserved host registers and disassembly support a null
destination with 101,776 requested bytes. The guest caller and producer of that
destination remain unknown. No guest terminal context is invented for this case.

The selected investigation inventory now contains 20 unique code points across
nine failure routes: 15 identified function entries and five interior/candidate
return sites. This is not an exhaustive writer inventory or 20 proven defects.
The first five points covered only two earlier chains. Priority for a focused
first round is the indexed-argument caller, buffer owner and table generator
connected by static calls. No corrective variants are applied yet: each change
needs a defined invariant and synthetic comparison, followed by native repeats.
Skipping consumers or changing timing alone would not establish correction.
Watch coverage remains partial, not fundamentally impossible.

## Four subsequent runs: two native SIGSEGV comparisons

Four new owner runs start at 20:33–20:41 UTC. Two reproduce SIGSEGV (186.300 and
165.137 seconds, 139/139); the intervening vertex and final fragment metadata
failures remain separate exit-23 captures.

The first SIGSEGV repeats a scalar consumer from an earlier sealed run. Both
captures retain the same candidate immediate return; exact-input instruction
bytes agree. Static caller code loads the bad argument from an indexed member.
Its numeric source address can be reconstructed from terminal registers, but
that source word was not among the protected reads and no earlier writer is
retained. The same static caller subsequently invokes the buffer-owning function
from the stack-protector investigation. This is a useful code relationship, not
proof of a shared cause or of that caller's execution in the stack case.

The other SIGSEGV is a byte read from null RDI plus a displacement. The candidate
caller passes the result of a query and a routine with explicit null-return
paths into the consumer. Distinguish missing input, failed type checks, earlier
state/lifetime changes and CPU behavior before proposing a correction. Terminal
RCX matches in stack locals are unrelated to the actual RDI-based operand.

Both shader candidates have metadata explained by later prologues. The vertex
run retains an unchanged eight-byte prefix over 19.319 ms; the fragment run lacks
the earlier join. No metadata relocation, skipped consumer, dummy object or JIT
semantic correction is applied. Four verified captures preserve the separate
evidence. The next useful probes target the indexed member before loading and,
separately, the query/filter arguments and return values of the null case.

## Latest five owner runs: separate failure families

The five runs starting October 23 at 20:19–20:30 UTC contain no SIGSEGV. They
comprise a stack-protector stop (170.488 seconds, 22/22), vertex metadata stop
(177.208 seconds, 23/23), two compute metadata stops (98.437 and 168.812 seconds,
23/23), and a graphics command-parser assertion (92.722 seconds, 23/23).
All five have the same validated 22-target profile, native hits on the four added
probes and incomplete writer-watch coverage due to admission refusals.

The stack case supplies a candidate immediate return matching a static call to
the failure stub. Exact-input static review identifies the function owning a
local buffer, a table generator receiving it and a following consumer. The guard
slot is among the retained terminal words; its value differs from terminal RAX,
but guard source memory was not independently readable. An overwrite by the
generator and a shared cause with earlier SIGSEGV remain unproved. The next
focused observation should compare destination, requested count, guard source and
local slot before/after this chain. Do not infer runtime count from unrelated
terminal caller-saved registers.

One compute case retains the same unrecognized eight-byte prefix before enqueue
and at lookup 35.615 ms later. A later prologue points to the scanner's candidate
metadata, so that candidate does not justify relocating the supplied origin.
The vertex case retains an unchanged sampled prefix over 1620.418 ms; the other
compute case lacks an earlier write join. These are distinct evidence limits,
not whole-shader stability or a common writer finding.

The latest graphics parser rejects a type-3 opcode it does not implement. Its
position is within the submitted span, but the retained neighboring words cannot
distinguish unsupported commands, incorrect framing/origin or modification.
Do not skip that command or bypass metadata/stack checks. Five independent
captures preserve complete logs, journals, matching binaries and checksums.

## First native captures with the interior-probe correction

The latest owner run stops after 183.596 seconds on the GPU watchdog, child 21 /
supervisor 31, with no SIGSEGV or terminal guest fault. The awaited submission
contains two command buffers with scheduled callbacks and no completed callbacks
after about ten seconds. This does not identify an exact shader or a CPU cause.
An immediately preceding 170.984-second run exits 23 on fragment-shader metadata;
it is a separate capture and also does not reproduce SIGSEGV.

The signed 22-target profile is validated in both runs. All four added entry/member
probes have real hits and retained observations; the latest current-array-entry
target has 20,859,739 hits and the pre-argument member target 16,911. Hit counts
do not imply writer coverage: bank zero is full and these targets respectively
record 19,376,119 and 16,269 admission refusals. Refusals count attempts, not
necessarily distinct fields. Direct guest/GPU writes and allocation lifetime
remain outside the imported-copy observer's coverage.

Both complete captures and matching binaries are saved separately with their
actual failure classifications. These runs validate native probe reachability,
but do not reduce the five initial analysis points to a writer or demonstrate a
corruption fix. Keep the current profile for the next SIGSEGV comparison; do not
change the GPU watchdog or accept neighboring shader metadata to force progress.

## Current continuation: entry capture prepared

The latest reviewed owner capture stops after 141.614 seconds on a scalar read
through an invalid incoming argument. A candidate caller loads that argument from
a returned object's member, but the old terminal memory window did not include
that member. Its value was not observed; unrelated register matches in stack
locals do not establish its source. This capture, matching pre-rebuild binaries
and exact-input static slices are sealed separately from the earlier array cases.

The diagnostic JIT could skip probes located inside a translated block. A checked
local generator now ends translation before such a probe, preserving the existing
dispatcher behavior. Synthetic regression controls reproduce the old omission and
verify the correction; this is a diagnostic fix, not a native SIGSEGV fix.

The active profile now has 22 pointer targets and unchanged 28 scalar probes;
four additions observe array entries, a returned member before loading, and an
incoming argument. Fixed journals use 1172 KiB. The prior profile is preserved.
Watch banks remain bounded and shared selections may be refused. All synthetic
CPU contracts pass in JIT and interpreter modes; no game/GPU run has been made
for this preparation. New files default to private permissions. See
[CPU diagnostics](CPU_DIAGNOSTICS.md) and [JIT review](JIT_REVIEW.md).

The following run summaries and eighteen-target descriptions are historical.

## Resumed October 23, 2026

The later owner capture stops after 136.143 seconds in the same array routine,
at the following vector load through an element pointer. This time the object's
array address is readable; its first entry retains the invalid terminal pointer.
The protected entry read, terminal operand and 196-byte displacement agree.
Retained count/destination operands are internally consistent with the static
code, unlike several object fields in the prior example. Exact prepared-input
hash and consumer bytes match across both captures. No earlier value or writer
of this element word is retained, and a common cause remains unproved. This case
is saved separately with a comparison. An intervening vertex-shader metadata
rejection is also archived separately and is not a SIGSEGV example. Collection
continues with the same profile and runtime.

The subsequent owner capture stops after 137.864 seconds in an ordinary pointer
array consumer. A retained indexed table entry refers to the numeric object; two
microseconds later its source-field probe already observes the terminal invalid
pointer. The static load and terminal register agree. Several neighboring
operands are also inconsistent with the consumer's expected pointer/count use.
No earlier value or responsible writer of that source field is retained.
Those two probes are observation-only and did not arm writer watches.

The eighteen-target profile loaded successfully. The added caller entry and
post-replacement targets were observed 279/275 times, the selected pre-load
branch had zero hits, and the register-only atomic consumer had 2,578,898 hits.
Configured targets must not be reported as reached coverage. This later stop
does not reproduce the preceding atomic fault. Its complete evidence, matching
binaries and exact-hash static slices are saved in another verified private
archive. Profile and runtime are unchanged by this review.

The owner has resumed SIGSEGV work. Three further completed captures are saved
with matching binary UUIDs, five diagnostic journals, exact launch selections,
verified prepared-input hashes, static instruction slices and SHA-256 archives.
The preceding one fails after 105.418 seconds during an atomic decrement through an
invalid pointer. A protected terminal read retains that pointer in a readable
caller slot; static caller code and a stack return candidate are compatible with
loading the slot before the call. No original writer, allocation lifetime or
shared cause with shader failures has been established.

The offline review now lists protected terminal memory sources separately from
earlier probe history. Empty pre-failure matches must not erase a terminal source
with a displaced access. Incomplete, malformed and ambiguous terminal reads do
not provide source evidence.

The active private profile now has eighteen pointer probes: the original fourteen
plus caller entry, before the selected load/call, after replacement, and a
register-only consumer entry. The two new persistent selections share bank zero
with the original header targets; saturation/refusal remains explicit. The
previous fourteen-probe selection and the separate allocation-failure alternative
are preserved. The twenty-eight scalar probes are unchanged. With shader submit
tracing, fixed journal storage is 1108 KiB, versus 1044 KiB with fourteen probes.

Original CPU fixtures check valid atomic decrement results and distinguish a
pre-existing bad slot, direct corruption and imported-copy corruption in JIT and
interpreter modes. These controls validate the observations and the tested CPU
behavior; they do not establish a native fix. No game/GPU run is performed for
this preparation. The next owner capture is needed to look for an earlier change
of the actual failing source field.

## Historical paused checkpoint

Paused at the owner's request on October 23, 2026. Development continues with
shaders. The native memory fault is unresolved; pausing does not imply a fix.

## Evidence and progress

The latest reviewed capture began October 23 at 14:41:04 UTC and stopped after
140.710 seconds, with child and supervisor exit 139. Saved instruction bytes,
terminal registers and the reported fault agree on a read through an invalid
pointer with an 88-byte displacement. The original diagnostic exactly matches
offline reconstruction; all sixteen terminal registers agree with the log.

Checked control flow admits two sources: a pointer loaded through the initial
root entry, or one loaded through a following entry during traversal. Terminal
data does not distinguish those paths. No retained earlier observation identifies
the offending source word or responsible writer in this capture.

Earlier captures localized array, returned-member and allocator-link consumers.
One allocator source word has earlier different and later invalid observations,
404.567 ms apart. This narrows an observation interval; it does not establish
that the earlier value was valid, the allocation remained the same, or a specific
write caused corruption. No common cause with shader, Oodle or audio failures
has been established.

## Diagnostic work retained

- Bounded CPU field and pointer histories, protected terminal reads and offline
  source/referrer review; offset faults are distinct from exact-address matches.
- Persistent imported-copy observations with explicit per-probe admission refusals.
  Four independent banks reserve 2048 slots each, with no borrowing or eviction.
  Three banks still refused many admissions in the latest capture.
- Bounded coroutine-frame quarantine, with explicit fallback and lifecycle events.
  Missing range matches do not exclude guest object lifetime errors.
- Full register/history capture for instruction failures, preserving real failure
  behavior; register-only observations remain separate from pointer evidence.
- Original CPU fixtures and JIT/interpreter contracts passed at the preceding
  checkpoint. These controls validate instrumentation, not a native game fix.

The active fourteen-probe/nine-scenario profile and scalar selections are
snapshotted with hashes in private analysis, along with the latest review.
The separate eighteen-probe allocation-failure alternative is preserved.
Neither profile is switched by this pause. One interior-site probe had zero hits;
its presence must not be reported as achieved coverage.

## Pending work when resumed

The proposed initial-entry, following-entry and selected-register capture was
**not implemented or tested** before the owner changed focus. Do not describe
it as ready. Validate reached basic-block sites and both paths using original
CPU fixtures, check signatures against the unchanged prepared input, review
bank competition, and refresh upstream state before a new native comparison.
No guarantee exists that another run reproduces this fault or captures its writer.

Private logs, journals, addresses, disassembly and profile snapshots remain
under ignored work/log directories. None belong in a public commit.
See [memory-error workflow](MEMORY_ERROR_WORKFLOW.md) for evidence semantics.
