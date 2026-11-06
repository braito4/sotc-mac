# Shared-memory failures in related PS4 ports

Reviewed 2026-11-06. Similar symptoms are not proof of an identical bug. The local
scatter captures establish invalid input indices; neither their original writer
nor the ownership of the affected physical pages has been established.

| Reference | Evidence in the project | Implication for SotC/Metal |
| --- | --- | --- |
| [Bloodborne PC: parallel GPU notes](https://github.com/deadinside28/bloodborne_pc/blob/master/docs/parallel_gpu.md) | The fixed heap-corruption section describes GPU-idle notification preceding deferred fence writes. Objects containing labels could be freed while writes were still pending. | The selected local runtime already drains the pipe and waits for deferred signals. Its latest game log also confirms completion-based GPU fences are enabled. Reapplying that fix is not a new experiment. |
| [SotC-PC visibility experiment](https://github.com/AjncSystem/ShadowOfTheColossus-PC/commit/497cb99a7da7f534053b0a180ccb0b206f2d44de) | The author disabled direct visibility-buffer writes after ending crashes. The documented hypothesis is guest reuse before GPU completion. The passing comparison disabled that feature while retaining other optimizations. | Supports investigating allocation lifetime; does not establish our index producer. Do not enable a comparable direct-write optimization without preserving completion order. |
| [SotC-PC releases](https://github.com/AjncSystem/ShadowOfTheColossus-PC/releases) | Earlier speculative readbacks overwrote reused memory. Version 0.72.2 separately retains uploaded texture contents during ending-time pool reuse. | Readback ordering and texture ownership are distinct from index-list correctness. The texture workaround is not a general scatter fix. |
| [SpartanPort troubleshooting](https://github.com/FelipePrado0/SpartanPort#troubleshooting) | Disabling deferred GPU readbacks is suggested for incorrect texture colors and lighting, with possible stutters. | A useful coherence precedent, not evidence of identical native Metal behavior. |

## PS4Recomp

Inspected [main at 4617028b2263](https://github.com/wie-project/PS4Recomp/tree/4617028b22631bbd9130e4fcabd037af3bdede2e),
recent commits, the three returned issues/PRs, and the relevant Metal resource,
draw, pending-write and wait-queue sources. Its README reports Bloodborne
gameplay with artifacts and lists other titles at different compatibility stages;
it does not demonstrate SotC compatibility or the same corrupted-index failure.

Concrete reference points, in priority order:

1. [GuestAccess and RefusedGuestBuffer](https://github.com/wie-project/PS4Recomp/blob/4617028b22631bbd9130e4fcabd037af3bdede2e/pkg/runtime/modules/libSceGnmDriver/ps4_gnm_draw.mm):
   the fallback distinguishes reads from writes. A read-only snapshot cannot
   preserve GPU writes to guest memory. Writable refused spans are not handled
   by that snapshot fallback. This closely matches the weakness in our
   fragmented `UnifiedCopy` path, but their refusal behavior is not itself a
   complete solution for us. We need correct writable mapping/coherence or an
   explicit diagnostic stop, not silently missing output.
2. The same source tracks overlapping aliases of guest windows. Metal buffer
   identities can differ while their backing bytes overlap. Read-after-write,
   write-after-read and write-after-write dependencies require ordering across
   aliases; object-local tracking alone is insufficient.
3. [GnmPendingWrites](https://github.com/wie-project/PS4Recomp/blob/4617028b22631bbd9130e4fcabd037af3bdede2e/pkg/runtime/modules/libSceGnmDriver/ps4_gnm_pending_writes.h)
   associates ranges with write epochs and command completion sequences. Its
   spatial index avoids repeated full scans. Consider this optimization only
   after correctness and physical-alias coverage are established locally.
4. [PR 5](https://github.com/wie-project/PS4Recomp/pull/5) discusses TLOU runtime,
   logging, CPU scheduling and pipeline-cache work. It is closed, draft and
   unmerged according to the API; portions of graphics work appear separately
   in main. Its game measurements are author-reported, not SotC/M1 benchmarks.
5. Render-pass merging was [reverted](https://github.com/wie-project/PS4Recomp/commit/8ad4c1101a224f80bd4eb9c7803239e184b7a5aa)
   and a [later implementation](https://github.com/wie-project/PS4Recomp/commit/4049cc733575df2758b8850f6fa2ed42d7a82237)
   adds attachment-hazard checks and deferred-copy conditions. Do not copy the
   initial optimization or assume the revised one is validated for our backend.

Public reference heads checked for this comparison: SotC-PC `1fe46aa3ff6a`,
native runtime `42736cf7ca77`, native mac `5e6e8e714550`, native recomp
`c60ccf8be752`, shadPS4 `0d5f2739d254`, Bloodborne PC `0fc2921c45f8` and
PS4Recomp `4617028b2263`. The recomp head is naming/analysis work; the shadPS4
head concerns MP3 parser reset, not this scatter input failure. Dependency pins
are unchanged; this review is not an exhaustive audit of all upstream history.

No code from these references is imported in this review. PS4Recomp is an
additional reference, not a replacement CPU/compiler/renderer dependency.

## Local next experiment

The isolated scatter guard waits for the GPU's input copy to complete, validates
its count and indices, and binds those same immutable inputs for the selected
dispatch. Invalid or incomplete coverage saves a private capture and exits 23
before encoding that dispatch. It does not clamp indices, omit work and continue,
or claim to fix the original producer. Valid indices can still address memory
whose owner has changed; this experiment does not solve allocation lifetime.

After the first capture, correlate the index-list binding with its producer,
fragmented writable fallbacks and mapping generations. Do not copy entire stale
temporary buffers back over potentially reused guest pages. Preserve the existing
baseline and compare one coherence change at a time.
