# GitHub problem review: 2026-10-22

Public-source review requested by the owner, checked at approximately 17:10 UTC.
This is a research checkpoint, not a dependency update or a new native test.
The local continuation checkpoint and selected source were read. Runtime
b2ff9cd0, CPU b9652d75, reference 411fdd86, installed libraries and cache 24/9
remain selected. The prepared staging-tail and binding-copy overlays remain
separate from these external candidates.

## Highest-priority lifetime and ordering candidates

### MoltenVK #2860: timeline signals and Metal completion

[PR #2860](https://github.com/KhronosGroup/MoltenVK/pull/2860), opened October 5,
is open/non-draft/unmerged in the retrieved page. Reviewed head link:
`53eafc69159adafb7507336171e742338a718085`.
[Issue #2859](https://github.com/KhronosGroup/MoltenVK/issues/2859) describes
`vkQueueSubmit2` -> host timeline wait -> resource destruction triggering Metal
validation. The author's explanation is that a shared-event signal can precede
the Metal command-buffer completion handler. The one-file diff makes active
submission command buffers retain their resource references unconditionally.
It is not an instruction to free resources before Vulkan completion.

Local source relevance: `vk_semaphore.cpp` creates a timeline semaphore and
refreshes its counter; `Scheduler::IsFree` and deferred deletion use that tick.
`DeferOperationPastNext` additionally protects prepared work that reaches the
following submission. This extra application guard must remain: driver retention
and application recording ownership solve different problems. The matching
timeline mechanism makes this a strong review lead, but the installed driver's
exact affected implementation and native reproduction are not verified here.
The pinned v1.4.2 source page was inaccessible. No claim connects this bug to the
guest allocator corruption, current OOM or a specific stalled batch.

Next: inspect the exact v1.4.2 implementation, then prepare an isolated synthetic
buffer/image-view lifetime test with timeline signaling, real Metal completion
observations and an unpatched control. A native comparison requires the existing
owner-supervised GPU guards; this review runs none. Retention may increase memory
held until completion and should be accounted for, not advertised as OOM relief.

### MoltenVK #2853: independent memory and conversion errors

[PR #2853](https://github.com/KhronosGroup/MoltenVK/pull/2853), opened September 30,
is open/non-draft/unmerged; ten-commit head link
`0eeec74ae8fa822bf6f42cbdb880bf25743d55b2`. Relevant diff sections were inspected:
reflection stores member indices instead of pointers invalidated by vector
growth; shader-library variants gain ownership/synchronization fixes; conversion
failure handling is corrected. Other commits concern descriptor templates and
pipeline-creation feedback bounds.

The author reports M1 Pro CTS comparisons against main `52aa21f`, with two tests
newly passing and no new failures, both with and without argument buffers. These
are author results, not an independent test or a result against our installed
driver. Local rendering uses `pushDescriptorSetKHR`; the scan did not establish
use of the template variant, cache merge or creation-feedback trigger paths.
Do not label all ten fixes applicable or attribute our guest heap failure to
host reflection corruption. Next: review the reflection and conversion commits
individually against v1.4.2; use a synthetic interface exceeding 32 flattened
variables, forced reallocation and failure-path checks before considering a build.

### AnyPS5 #6747: pending GPU writes into reused command storage

[PR #6747](https://github.com/boykopovar/AnyPS5/pull/6747), opened October 16,
remains open/non-draft/unmerged at the previously reviewed
`0b5ba4470391f61f53e8e0283fcd8663223c65e8`. The diff checks tracked overlapping
GPU writes after command storage selection and before CPU packet writes.
The author supplies an overlap test that fails with the guard disabled and a
disjoint-range control that leaves work asynchronous. Gameplay reports are from
an M2/Rosetta combined PS5 port, with unrelated shader experiments also present.

Next: audit our PS4 command, label and guest-range ownership, then construct an
original synthetic overlap/disjoint test. This is a provenance and ordering
pattern, not a patch transferable directly from AGC to GNM. It does not prove
who overwrote the allocator metadata in an earlier owner capture.

## Descriptor and shader candidates

| Source / observed revision | How the proposal addresses its problem | Local limit and next review |
|---|---|---|
| [MoltenVK #2873](https://github.com/KhronosGroup/MoltenVK/pull/2873), open/non-draft/unmerged; `6b37a5e86ad0e9f7bd89b542fd75ef7bd150fb5f` | Rechecks the actual argument-buffer binding after helper commands overwrite its Metal slot. Diff inspected. Author reports an unpatched v1.4.2 failure and patched M5 Max passes; maintainer approval October 20 is not a merge. | Relevant to wrong descriptors/textures, not demonstrated guest CPU corruption. Establish whether our descriptor route reaches the same state; reproduce draw/helper/draw and a forced-rebind control synthetically. |
| [AnyPS5 #9746](https://github.com/boykopovar/AnyPS5/pull/9746), open/non-draft/unmerged | Uses an empty continue block to avoid a SPIRV-Cross translation that drops loop-phi updates. Diff adds the single-predecessor latch case and structural fixtures. | Commit list shows `8a1ae6b`, while published controls refer to `b425f0f`; resolve revision correspondence before relying on those tests. Our structurizer differs. Inspect generated synthetic MSL for a bounded loop with memory access; do not assume our GPU stop is this loop bug. |
| [AnyPS5 #9193](https://github.com/boykopovar/AnyPS5/pull/9193), open/non-draft/unmerged; previously reviewed `e0d54227c8a8f6e97a846888557c1decdb21a742` | Reads eligible remaining I/J values from barycentric built-ins, avoiding generated stages. | No general raw per-vertex support. Classify our failing interfaces first; M2/M4 title results do not validate M1 or PS4 shaders. |
| [AnyPS5 #9789](https://github.com/boykopovar/AnyPS5/pull/9789), earlier same-day review `55c2906` | Retypes raw color exports into UINT attachments and supplies bit-pattern tests. | Different from our host image blit path. Our typed same-format nearest copies are already built; do not replace or double-apply them based on a similar error string. |

MoltenVK #2858 and SPIRV-Cross #2699/#2697 could not be freshly retrieved. Preserve
their prior exact heads and draft/merge observations in [PerVertex review](PERVERTEX_REVIEW.md).
No new general PerVertexKHR solution for M1 was established. The public
[MSL 4.0 request #2569](https://github.com/KhronosGroup/SPIRV-Cross/issues/2569)
is closed as not planned; it is not proof of shipped support.

## Memory growth and pipeline pauses

[MoltenVK issue #2600](https://github.com/KhronosGroup/MoltenVK/issues/2600), closed
August 31, 2025, has the same class of Metal insufficient-memory error. Its author
resolved it by destroying image views that kept image resources retained. Our
`TextureCache::DeleteImage` already erases backing views before the image in a
deferred callback; the local proxy overlay also keeps views/images together and
destroys views first. Therefore no missing-destruction fix follows from this
report. An accounting audit should include view/backing/driver ownership in
addition to application image counts, while preserving in-flight resources.

[bmy issue #3](https://github.com/bmy/bbport-mac/issues/3) initially discussed
budgets but later links
[ab73565](https://github.com/bmy/bbport-mac/commit/ab73565a978212fb8886c55ad023c861e1ad3d81):
the author reports that pooling KosmicKrisp Metal command allocators reduced
their retained high-water allocation dramatically. The commit diff updates the
driver pin and adapts object counters; the pooling implementation is upstream.
Our active driver is MoltenVK. This motivates measuring driver command storage,
not switching drivers or claiming that our retained multi-GiB upload is the same
allocation. The initial AI-written budget suggestion is not the confirmed fix.

[SotC PR #238](https://github.com/AjncSystem/ShadowOfTheColossus-PC/pull/238), open,
has code `06e064d` and documentation `3363f7d`. It defers arena readback processing
and rechecks pin/use/write state before eviction. The description says a five-
second deadline drops the pending eviction, but the inspected diff calls
`scheduler.Finish()` at that deadline or on a forced wait. Resolve that material
discrepancy before adaptation. Our selected runtime does not contain that
`MaybeEvictCold` route. RTX measurements do not establish M1 memory savings.

[SotC PR #215](https://github.com/AjncSystem/ShadowOfTheColossus-PC/pull/215), open,
parallelizes cache preload with pipeline workers and preserves objects still
referenced by jobs. The author explicitly describes an earlier use-after-free
from erasing a null-handle pipeline before its worker finished. The inspected
diff waits for compute jobs and retains refused objects. Useful for cache/job
lifetime review; its 107-to-6.5-second RTX loading claim is not our Metal runtime
compile pause and must not weaken our rejected/null-pipeline guards.

The preferred upstream's [0.8.2 notes](https://github.com/bloodborne-native/bloodborne-mac/releases/tag/v0.8.2)
retain GPU-written images that cannot safely be downloaded. This is consistent
with our selected GC preservation guard, not permission to evict those contents.
[Bloodborne issue #3](https://github.com/bloodborne-native/bloodborne-mac/issues/3)
also corrects an initial claim that a release fixed an unconfirmed guest crash;
there is no demonstrated fix for our allocator metadata fault there.

No exact external remedy for our single large fallback upload or audio queue
stop was established. Native savings from our prepared tail allocation remain
pending. Prioritize the driver lifetime audit and original synthetic controls;
keep OOM, CPU corruption, shader rejection and audio failure as distinct evidence.

## Access and validation boundary

Public repository/issue/PR pages, relevant comments and changed-file excerpts
were inspected. Individual commit pages, selected driver source and several
histories/comparisons were inaccessible. Full SHAs above come from public commit
link targets; unavailable pages do not imply empty diffs or unchanged heads.
No complete default-head refresh, dependency qualification, game execution,
GPU test, runtime change or GitHub publication was performed by this review.

## Subsequent local adaptation

After this research checkpoint, the owner requested implementation. The checked
`sotc-metal-retention-v1` overlay now requests retained references at the native
Metal renderer's two batch-creation sites, inspired by #2860's lifetime principle.
It does not modify or upgrade MoltenVK. Production-method CPU doubles pass;
see the [current handoff](DEVELOPMENT_HANDOFF.md#continuation-checkpoint) for build,
contract and pending native validation. Other candidates above remain research.

## SotC pipeline/SRT follow-up, October 22, evening

The 431 pipeline failures and 805 SRT-offset errors are repeated events, not
proven distinct defects. The same owner capture contains 376 PerVertexKHR messages;
their exact correspondence with failed pipelines has not been established.
The public open-PR listing was inspected, with focused conversations/diff excerpts
for #69, #81, #173, #215, #232, #238, #243 and #258.

| Candidate | Observed state/revision | Local applicability |
|---|---|---|
| [#173](https://github.com/AjncSystem/ShadowOfTheColossus-PC/pull/173) | Open, non-draft, 11 commits; visible head `ced601b` | Author reports corrected interpolation on GTX 1080 Ti through a generated geometry shader. That stage is unavailable in our M1/MoltenVK route. Useful semantics for synthetic capture/replay review, not a drop-in fix. |
| [#258](https://github.com/AjncSystem/ShadowOfTheColossus-PC/pull/258/files) | Open, non-draft; visible head `44633f5`, October 22 | Inspected diff adds invariant positions for the barycentric GS route, empty-queue lock avoidance and nonexistent shader-file read avoidance. No general M1 PerVertex or SRT-offset fix established. |
| [#215](https://github.com/AjncSystem/ShadowOfTheColossus-PC/pull/215/files) | Open, non-draft; visible head `5f53a96`, October 21 | Parallel preload and refused/in-progress pipeline lifetime handling. Potential loading improvement, not rejected-shader translation support. |
| [#81](https://github.com/AjncSystem/ShadowOfTheColossus-PC/pull/81) | Open, non-draft, six commits | Intel background compilation requires dynamic vertex input and skips work until ready. Author's Windows fixes/subgroup controls do not establish M1 compatibility. |
| [#232](https://github.com/AjncSystem/ShadowOfTheColossus-PC/pull/232) | Open, non-draft, `ad3a123`; fork merge/main roadmap reference October 22 | Identical descriptor-bind caching with invalidation guards. Performance candidate, not offset reconstruction. Roadmap says integrated; actual default-branch code inclusion remains unverified. |

Local source check: `RegisterShaderBinary` saves directly through
`Storage::DataBase`, without the PR's per-new-shader `.spv` collision read.
Do not transplant that optimization based on Windows timings. The local SRT
adapter already handles branch-independent Phi values by comparing emitted offset
programs, with cycle/depth/work bounds. The 805 errors do not identify their
unsupported opcode or demonstrate that another Phi fix is needed.

[Issue #41](https://github.com/AjncSystem/ShadowOfTheColossus-PC/issues/41) reports
ComputeOffset/Phi errors on Windows/Vega. Later wave32 reports improve rendering
but still describe device loss, not a proven SRT repair. No verified general
ARM64 remedy for our remaining offsets was found. Direct guest-memory shader
access stays disabled.

[MoltenVK #2858](https://github.com/KhronosGroup/MoltenVK/pull/2858) was accessible
on this follow-up: draft, 15 commits, explicitly test-entry-only portable
capture/replay. Its body retains SPIRV-Cross pin
`81739edf516c2bef043ac61f510b4bd78e126a22`. This is the relevant research direction
for genuine per-vertex inputs, with additional scratch/retained-memory costs.
Existing isolated CPU compiler fixtures do not validate GPU execution or SotC.

Next: group failures by pipeline/module/conversion reason; obtain bounded private
IR provenance for unresolved SRT expressions and reproduce each class with
original synthetic inputs. Check distinct per-corner values and indexed draws
before proposing capture/replay integration. Preserve explicit errors and guards.
No runtime, dependency, configuration, game execution or GPU test changed here.

Public commit pages failed and GitHub API requests timed out. Full SHA, merge
ancestry and complete diff refresh remain unverified. Abbreviations above are
observed page links, not qualified pins. This appended review is local only.
