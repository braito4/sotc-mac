# ARM64 CPU investigation and external JIT review

Reviewed October 23, 2026. Native SIGSEGV remains unresolved. A faulting guest
instruction identifies a consumer of an invalid pointer; it does not establish
that the translator generated the pointer incorrectly. Private captures retain
the instruction slices and source observations separately from this public review.

## Local findings

The selected CPU revision is `b9652d75edc0a281f06cec0da6ff72e9f5150a70`.
The following functions are in its `src/cpu/jit_arm64.c`:

| Function/path | Relationship to the investigation | Evidence and next check |
|---|---|---|
| `translate()` | Confirmed diagnostic omission: probes inside a translated block could be skipped. | A checked local generator ends translation before an interior probe so the existing dispatcher runs it. Original fixtures reproduce the omission and verify registers, flags, loop visits and cached decoded blocks after correction. This does not establish a native corruption fix. |
| `translate_insn()` MOV → `read_op()` / `write_op()` → `address()` / `load()` / `store()` | Scalar pointer, array-entry and returned-member loads used by the failing consumers. | Some captures already contain the bad value in readable source memory before the failing access. Compare address calculation and results against the interpreter using original inputs; find the earlier writer. No incorrect MOV translation is demonstrated. |
| `vector()` → `vload()` / `vstore_reg()` / `vstore_mem()` | Full-width MOVUPS/VMOVUPS uses two 16-byte halves for a 32-byte operand. | One retained array element is already invalid before the displaced vector access. Review displacement, upper-half addressing, page boundaries and ordering in differential controls; the fault alone does not implicate vector translation. |
| `atomic_rmw()` | Earlier failure family consumes an invalid pointer in a locked decrement. | Original valid old/new-count controls pass through JIT and interpreter. Investigate the pointer's source/lifetime separately; passing these controls does not prove every atomic instruction or concurrent execution correct. |

Existing `BB_JIT_DENY` selectors can send chosen instruction classes through the
interpreter. Use a controlled comparison after a reproducible synthetic difference
or a newly captured source transition. Native success under a selector can also
reflect different timing and is insufficient by itself to identify a CPU defect.
Keep the existing memory-ordering and memory-protection behavior.

## Bloodborne upstream

The reviewed CPU head is `ef153b8d35768aff86fa1643c12e7d0340b16501`.
The [comparison from the selected revision](https://github.com/bloodborne-native/bloodborne-recomp/compare/b9652d75edc0a281f06cec0da6ff72e9f5150a70...ef153b8d35768aff86fa1643c12e7d0340b16501)
contains three CI/test-tooling commits and no CPU semantic correction to import.

There is a relevant historical precedent:
[interpreter CMPXCHG correction](https://github.com/bloodborne-native/bloodborne-recomp/commit/871b8cf1f336ab6d6a99ef45d44e19af75488365).
A failed compare-exchange must report the actual observed value, rather than a
separate load that another thread may change. The old behavior could let two
threads acquire the same lock and corrupt the heap. That correction is already
an ancestor of our selected CPU revision. Reapplying it is not a new fix.
The upstream [ARM64 notes](https://github.com/bloodborne-native/bloodborne-recomp/blob/ef153b8d35768aff86fa1643c12e7d0340b16501/docs/ARM64_NATIVE.md)
also document differential verification, instruction selectors and timing limits.

[Bloodborne mac issue 3](https://github.com/bloodborne-native/bloodborne-mac/issues/3)
reports a GXRenderThread SIGSEGV with a null resource member. A maintainer
distinguishes it from the GXWorker fix in a later comment. It supplies another
example of tracing an argument back to its source, but neither its null operand
nor its different game establishes a shared cause with our invalid-pointer cases.
Do not import proposed null-skipping behavior as a correction.

## orbis-on-macos

Reviewed tree: `d964c541cb0de4191f6275723a082efb84da131b` (last reported push
October 18, 2026), MIT license. The primary sources are its
[README](https://github.com/nacimoualla/orbis-on-macos/blob/d964c541cb0de4191f6275723a082efb84da131b/README.md),
[JIT implementation](https://github.com/nacimoualla/orbis-on-macos/blob/d964c541cb0de4191f6275723a082efb84da131b/src/cpu/jit_compiler.cpp),
[executor](https://github.com/nacimoualla/orbis-on-macos/blob/d964c541cb0de4191f6275723a082efb84da131b/src/cpu/executor.cpp)
and [retail-readiness roadmap](https://github.com/nacimoualla/orbis-on-macos/issues/1).

The README explicitly places the move from synthetic samples to commercial titles
in remaining phases 16–22. Its own C++ JIT uses a different CPU context, emitter
and memory interface. In the reviewed `JitCompiler::compile_block_locked()`, the
MOV load opcode case supports register operands; memory forms do not provide an
equivalent implementation to copy for our scalar consumers. The reviewed JIT
does not contain an equivalent full-width VMOVUPS or locked XADD/CMPXCHG path.
Unsupported instructions stop compilation and use the executor fallback.
These are static-source findings, not a claim that every operation is unsupported
by the project's interpreter. Its architecture note describes a broader embedded
core; the reviewed implementation is the evidence used for this assessment.

No matching pointer-corruption correction was identified in the reviewed public
issues or recent commits. Test organization, compiled/interpreted transitions and
execution counters are useful references. The reviewed phase-13/15 fixtures
exercise block boundaries, caching, ALU and executor integration; they do not
provide a differential reproduction of our pointer failure. Replacing our CPU backend
would require a substantial integration and would not resolve the observed cause
on current evidence. Dummy HLE objects or early returns on missing resources
would change program behavior without identifying the original defect.
No external code was executed or imported for this review.

## Prepared continuation

The current private profile has 22 pointer probes plus the unchanged 28 scalar
probes. New observations cover the first array entry before use, a returned
object member before argument loading, and the incoming numeric consumer argument.
Persistent selections share an existing bounded watch bank: admission refusal,
overwritten history and writes outside imported-copy instrumentation remain gaps.
Registration occurs before execution; dynamic registration after JIT compilation
is not covered by the interior-probe correction.

All synthetic CPU contracts pass in JIT and interpreter modes. The launcher
creates private output with `umask 077`; reads and journal storage remain bounded.
Subsequent owner runs validate native hits on the added probes; bounded watch
admission still leaves writer coverage incomplete. No native fix is established. See
[CPU diagnostics](CPU_DIAGNOSTICS.md) and the
[SIGSEGV checkpoint](SIGSEGV_CHECKPOINT.md) before the next owner capture.
