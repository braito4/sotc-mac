# Development and debugging handoff

Snapshot: 2026-10-22. This is the starting point for the next developer. Read
[AGENTS.md](../AGENTS.md), this page and [Build instructions](guide/building.md)
before changing code. [Status](STATUS.md) and [Graphics bring-up](GRAPHICS_BRINGUP.md)
contain chronological investigations; their earlier “latest run” sections describe
older boundaries, not the present state.

## Continuation checkpoint


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
