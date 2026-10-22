# Private CPU fault diagnostics

The newest captured CPU boundary (**19:53 UTC on 2026-10-22**) is a translated
SIGSEGV after **118.281 seconds**, with confirmed exits 139/139. Private offline
disassembly relates the invalid loaded pointer to an earlier field load outside
the existing four-word register snapshot. Its source value/writer/lifetime remain
unknown. The preceding capped run still ends on Metal OOM; neither failure is
declared fixed. See the current [handoff](DEVELOPMENT_HANDOFF.md#continuation-checkpoint).

## Optional register-relative field windows

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
