# Dependency and reference watch

## October 24 source refresh

Default heads were resolved from GitHub's repository/commit APIs and compared with
the preceding recorded review, independently of installed pins. Runtime main is
`1dda5fb086cd42e8d52e78c80753257d52b77f04` (27 additional commits, 89 changed paths)
and the app is `a55959674fcfd9c3cff7a5c8dce9dc172a3fde87` (13 commits), whose
runtime gitlink matches that runtime head. A6, Metal resource creation M1 and M2
shared recording step 1 are now documented upstream. Complete Metal command
recording and removal of Vulkan/MoltenVK remain later steps.

The SotC reference, CPU translator, Pink, bmy, MoltenVK, SPIRV-Cross and KosmicKrisp
default heads match the preceding recorded heads. Orbis matches the reviewed tree;
Prosperity matches the exact owner-linked commit. AnyPS5's default head was recorded
without claiming a complete change review against an unavailable prior baseline.
No installed runtime, CPU, driver or library revision was updated. See the latest
[source assessment](UPSTREAM_REVIEW.md#october-24-render-adapter-refresh) and
[CPU testing references](CPU_TRANSLATOR_REFERENCES.md).

The owner requested continuing review of upstream changes. This document keeps
the permanent source list for development-session reviews. Scheduled public
checks are separate; this review does not create or reactivate them. Source
reviews do not automatically install dependencies or publish changes.
Development sessions should also check these sources before treating an old
limitation as current. Resolve each repository's actual default branch, record
the reviewed SHA and inspect the diff and tests.
The owner subsequently explicitly requested integration of all dependency
updates. That reviewed integration is now represented by the
[current dependency inventory](DEPENDENCIES.md) and version lock. Keep this
inventory current after source/version reviews; the daily watch remains a public
review task rather than an automatic installer or publisher.
Also follow audio/NGS2 delivery and scheduling changes: the owner reports audible
cuts, and [metadata tracing](AUDIO_DIAGNOSTICS.md) now measures candidate gaps.

| Repository | Role and review focus |
|---|---|
| [AjncSystem/ShadowOfTheColossus-PC](https://github.com/AjncSystem/ShadowOfTheColossus-PC) | Permanent SotC reference; guest memory writeback, nested task ownership, shaders and loader contracts. Windows results require separate native ARM64 validation. |
| [luizgustavs/Pink-shadPS4](https://github.com/luizgustavs/Pink-shadPS4) | Permanent owner-requested SotC/shadPS4 reference; memory reuse, pending CPU/GPU writes, command recording and GCN floating-point semantics. Do not copy hardcoded heap/stack workarounds or NVIDIA-specific assumptions. |
| [bloodborne-native/bloodborne-mac](https://github.com/bloodborne-native/bloodborne-mac) | Preferred app/reference upstream; follow its runtime links rather than copying app-specific assumptions. |
| [bloodborne-native/bloodborne-runtime](https://github.com/bloodborne-native/bloodborne-runtime) | Permanent direct upstream, explicitly reaffirmed October 23; our pinned renderer/runtime base. Compare selected and last-reviewed SHAs, shader input ownership/decoding, GPU/CPU memory ordering, command reuse, resource lifetime, Metal and host contracts. |
| [bloodborne-native/bloodborne-recomp](https://github.com/bloodborne-native/bloodborne-recomp) | Our pinned native CPU translator; instruction/ABI fixes and interpreter/JIT parity. |
| [ptitSeb/box64](https://github.com/ptitSeb/box64) | Permanent owner-requested reference for suspected JIT defects: interpreter/ARM64 dynarec semantics and range-limited differential checks. Confirm disputed results against native x86 or the ISA; not an installed replacement backend. See [CPU testing workflow](CPU_TRANSLATOR_REFERENCES.md#standing-box64-reference-requested-by-the-owner). |
| [bmy/bbport-mac](https://github.com/bmy/bbport-mac) | Additional Mac reference requested by the owner; native renderer, memory, driver and recording fixes. Not a direct submodule. |
| [KhronosGroup/MoltenVK](https://github.com/KhronosGroup/MoltenVK) | Installed Vulkan-on-Metal driver; native memory, resource lifetime and experimental PerVertexKHR #2858. |
| [KhronosGroup/SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) | Installed SPIR-V-to-MSL compiler; interface lowering and proposals #2699/#2697. |
| [shadexternals/mesa-kosmickrisp](https://github.com/shadexternals/mesa-kosmickrisp) | Alternative driver used by bmy's port; ARM64, sparse buffers and geometry/interface support. Not installed here. |
| [AnyPS5 macOS issue #1](https://github.com/boykopovar/AnyPS5/issues/1) | Owner-requested discussion watch; follow linked memory-ordering, shader, Metal and host-contract fixes. Different PS5/Rosetta implementation; not a dependency. |

## Object lifetime follow-up: 2026-10-23

Track [SotC issue #274](https://github.com/AjncSystem/ShadowOfTheColossus-PC/issues/274):
its older Linux capture reaches the same array-read boundary; no fix, comments
or deterministic reproducer yet. The new AnyPS5 permission-intersection report
and existing guarded-quarantine/readback experiments are contrasted with local
code in [the upstream review](UPSTREAM_REVIEW.md#object-corruption-diagnosis-methods-public-review-2026-10-23).
No runtime/profile change follows from these reports; the pending owner command
is unchanged. A matching instruction alone does not establish matching lifetime.

## Direct runtime review reaffirmed: 2026-10-23

The owner explicitly requested [bloodborne-runtime](https://github.com/bloodborne-native/bloodborne-runtime)
as a permanent direct change source. It was already listed above and in the
lock; no duplicate source or watch is created. Public Git HTTPS refs reconfirm
main at `0d2ffb3470da7e56dd82cc8ef08f58b5e777abbd`, unchanged from the preceding
review, while the selected runtime stays `b2ff9cd0b982aa481fc48d4ce51b3cc7b4cd2548`.
The public compare patch contains 30 commits / 149 distinct paths. Reviewed
candidate decisions and synthetic follow-ups are in
[the upstream review](UPSTREAM_REVIEW.md#direct-runtime-review-2026-10-23).

Already adapted: `dcaf500` instanced Metal tessellation. Future feature candidate:
`1baeb98` FSR Metal shared-event ordering and reacquiring command buffers after
submission, relevant if that backend is integrated. Not adopted: `ab31a4c`
content-losing critical-pressure eviction. Adapter A0-A3 is a separate broad
migration. This diff supplies no decoder/CPU implementation correction for the
latest unknown-format or invalid-string boundary. Author Bloodborne/M3 results
remain distinct from our M1 CPU controls and pending native game validation.

## SIGSEGV/pipeline/SRT review: 2026-10-23

The later owner-authorized dependency update integrates
[Vulkan-Headers 1.4.366](https://github.com/KhronosGroup/Vulkan-Headers/commit/73814d006638aa00bd553215b054f0f02e457e22),
October 23 01:14:01 UTC. Registry/generated C/C++ changes were reviewed; installed
headers/build markers and ARM64/all 21 renderer CPU checks agree. Vulkan-Loader
stays 1.4.365 under its own recipe variable: header/loader release tags need not
advance together. The [inventory](DEPENDENCIES.md) is refreshed with exact SHAs.
Other reviewed stable library/tool channels remain at the selected versions.
Runtime/app FSR 4.1.1 and adapter changes remain separately reviewed references;
the qualified base and local overlays are retained. No native rendering or
source-field corruption benefit is established by a header-only update.

Before the owner-requested targeted-field comparison, public API checks again
resolve SotC main `40ec52a5a246bbaf20402520fe8fc9682f0a30c0` (October 22
16:40:58 UTC) and CPU main `ef153b8d35768aff86fa1643c12e7d0340b16501`
(October 22 16:51:55 UTC). Runtime main is now observed at
[`0d2ffb3470da7e56dd82cc8ef08f58b5e777abbd`](https://github.com/bloodborne-native/bloodborne-runtime/commit/0d2ffb3470da7e56dd82cc8ef08f58b5e777abbd),
October 23 00:22:19 UTC. The newer diff adds Metal FSR 4.1.1 integration and moves
adapter image-barrier state; this is not a qualified migration or an identified
repair for the null/invalid CPU-object writer. The proposed comparison keeps
upscaling off and selected pins/cache unchanged. MoltenVK latest release remains
1.4.2; PerVertex proposals #2858/#2699 remain open drafts at the reviewed heads,
and SotC #219/#173 remain open/non-draft/unmerged at the reviewed heads. Private
API responses and diffs are retained under `work/`; no dependencies are updated.

The owner-requested [follow-up](GITHUB_PROBLEM_REVIEW.md) refreshes reference/CPU
heads, observes the newer runtime adapter series, verifies PerVertex PR heads
and states, and inspects the stale-readback/nested-release/geometry-emulation
changes against our local contracts. Observed heads are separate from qualified
pins. No dependency, driver or automation is changed. A separate checked local
SRT/bitfield safety adaptation now uses cache 28/9; native effects remain pending.

## Owner-requested five-source refresh: 2026-10-22

The owner explicitly requested reviewing and selectively updating from SotC,
Pink-shadPS4 and bloodborne-mac/runtime/recomp, and retaining them as future
references. All five are now recorded in the version lock/inventory, with
selected pins separate from reviewed heads. Public Git resolves exact histories
despite the earlier web-page limits. The
[new source review](UPSTREAM_REVIEW.md#owner-requested-five-source-refresh-and-selected-adaptation)
records dates, exact SHAs, diff scope, author reports, controls and decisions.
The checked Metal instanced-tessellation adaptation is separate from a runtime
base update. The latest reference's nested-coroutine warning is a new local
ownership-review priority; speculative readbacks are absent locally and lossy
critical-pressure image eviction is not adopted.

For future updates, resolve default branches again, compare against both selected
and last-reviewed SHAs, and follow the app's exact runtime/CPU gitlinks. Record
why each candidate is integrated, already present, inapplicable or pending, with
a negative control and validation boundary. Review Pink changes by individual
contract rather than replacing our renderer or importing its entire settings.
The refresh changes neither existing automation schedules nor the broader
driver/compiler watch requirements before a new native comparison.

## Owner-requested problem review: 2026-10-22 17:10 UTC

The [problem review](GITHUB_PROBLEM_REVIEW.md) records newly inspected MoltenVK
#2860/#2853/#2873 lifetime, conversion and descriptor proposals, AnyPS5 ordering
and loop-lowering controls, and SotC #238/#215 eviction/preload proposals.
It contrasts them with our timeline-based retirement, existing view destruction,
typed copies and current overlays. bmy's command-allocator pooling fix belongs
to KosmicKrisp, not our installed MoltenVK. Relevant diffs and comments were
read; unavailable exact-source/history pages remain explicitly unresolved.
No production dependency, reviewed default-branch head, runtime setting or
native validation claim changes. These are candidates for isolated source and
synthetic review, not qualified dependency updates or a completed pre-test gate.

The owner's subsequent implementation request produced a local native-Metal
retained-reference overlay, documented in the
[handoff](DEVELOPMENT_HANDOFF.md#continuation-checkpoint). This applies the #2860
lifetime principle without changing the installed driver, dependencies or cache.
It is not an additional public-head refresh or native comparison.

## Pre-comparison staging review: 2026-10-22 16:57 UTC

Public refresh began with the SotC reference, then bloodborne-mac/runtime/recomp,
bmy, MoltenVK, SPIRV-Cross and KosmicKrisp. Root pages are reachable; the
[reference release](https://github.com/AjncSystem/ShadowOfTheColossus-PC/releases/latest)
still resolves to v0.60.5. The
[bloodborne-mac root](https://github.com/bloodborne-native/bloodborne-mac) now
displays a runtime gitlink abbreviated as **e10eb58**, different from our selected
b2ff9cd0. The linked commit, comparison and requested commit histories cannot be
retrieved. The full SHA, commit date, ancestry and diff are unresolved, so this
is a new review lead, not an approved update or a claim about its memory fixes.
Preserve the last known exact revisions in the dependency inventory.

MoltenVK #2858 and SPIRV-Cross #2699/#2697 remain inaccessible in this refresh;
their prior status is retained, not revalidated. The AnyPS5 macOS issue remains
reachable, but this pre-comparison check is not a complete new/edited-comment
or linked-PR audit. No empty diff or unchanged proposal state is inferred from
access failures. No dependencies are changed.

The local staging-tail candidate is based on the pinned allocation/ownership
code and original synthetic requests. It keeps the same allocation classes and
retained resources, using only unused aligned tails in the current recording
tick. It does not adopt another port's shader, guest-layout or Metal-lifetime
assumptions. The owner's native comparison will test this single local change;
CPU checks do not establish savings, correct pixels or OOM avoidance on M1.

## Pre-comparison integer blit review: 2026-10-22 15:25 UTC

The public refresh started with the
[SotC reference](https://github.com/AjncSystem/ShadowOfTheColossus-PC), then the
preferred [bloodborne-mac](https://github.com/bloodborne-native/bloodborne-mac)
reference and its runtime/CPU links, followed by bmy, MoltenVK, SPIRV-Cross and
KosmicKrisp. Reachable repository pages show `main` except bmy's
`macos-native-0.6`. The reference's
[latest release](https://github.com/AjncSystem/ShadowOfTheColossus-PC/releases/latest)
still resolves to v0.60.5; the runtime page shows its CPU gitlink at b9652d7.

Root pages do not expose exact latest commit SHAs. Requested reference/runtime
histories, MoltenVK #2858, SPIRV-Cross #2699/#2697 and driver/compiler release
pages are unavailable. Targeted public search establishes no additional typed-
blit patch, but this is not proof that none exists. A complete fresh head/diff
comparison is unavailable; preserve the last known exact revisions in the
[dependency inventory](DEPENDENCIES.md), not an assumed empty diff.

The local candidate is independently motivated by the two captured integer-
format pipeline errors and the selected renderer source: its fragment output
was always float for color. The prepared overlay adds exact integer reads and
typed outputs for same-format nearest 2D copies, with explicit unsupported-case
errors. The [Vulkan contract](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBlitImage.html)
requires matching integer signedness and defines conversion/clamping rules;
this first implementation intentionally excludes format conversion. ARM64/CPU
checks are separate from pending native Metal shader/pixel validation. No pins,
installed libraries, cache versions or renderer settings change. This review
starts no game/GPU test and establishes no OOM or corruption fix.

## Pre-comparison audio diagnostic review: 2026-10-22 14:55 UTC

Public refresh again started with the SotC reference, followed by the preferred
bloodborne-mac reference, runtime and CPU. Repository pages show `main` for those
four projects, MoltenVK and KosmicKrisp; bmy shows `macos-native-0.6`.
SPIRV-Cross's repository page is reachable, but this check does not resolve its
exact current head. The reference's
[latest release](https://github.com/AjncSystem/ShadowOfTheColossus-PC/releases/latest)
still resolves to v0.60.5; [SDL's latest release](https://github.com/libsdl-org/SDL/releases/latest)
resolves to 3.4.18, matching the selected library. The installed SDL headers
define all three optional diagnostic getters used by this change.

Requested reference/runtime/CPU commit histories and GitHub API endpoints were
unavailable, as were the refreshed MoltenVK #2858 and SPIRV-Cross #2699/#2697
pages. The rendered root pages omit exact latest SHAs. No fresh complete head
or diff comparison is claimed, and unavailable diffs must not be treated as
empty. The earlier same-session AnyPS5 proposal review below remains a candidate
baseline, not a selected update. No new source change was established as a
prerequisite for these local diagnostic additions. Retain the known inventory,
runtime b2ff9cd0, CPU b9652d75, reference 411fdd86 and cache 24/9.

The prepared owner comparison adds bounded audio backend/queue failure evidence
and fatal CPU frame-local candidates. It preserves 640x360, DMA/async off,
metadata on and pixel capture off. Full offline contracts pass; this review does
not run the native game/GPU or validate a fix for either failure.

## AnyPS5 macOS thread review: 2026-10-22

The [macOS thread](https://github.com/boykopovar/AnyPS5/issues/1) is open.
Its initial and latest visible discussion and the four PRs below were read;
GitHub collapses older comments, so this is not a complete archival review.
Latest visible posts include imported-image write-back/protection observations
and M1 network negative controls. Discussion reports are leads, not SotC results.
The hourly notification task compares new/edited comments and linked changes
against this baseline. It does not post comments, change pins or run games.

| Candidate | Observed state / revision | Relevance and limit |
|---|---|---|
| [#6747](https://github.com/boykopovar/AnyPS5/pull/6747) | Open; `0b5ba4470391f61f53e8e0283fcd8663223c65e8` | Orders already-pending overlapping GPU writes before CPU command allocation. A useful write-provenance/regression pattern; not proof of our guest allocator fault's cause. |
| [#9193](https://github.com/boykopovar/AnyPS5/pull/9193) | Open; `e0d54227c8a8f6e97a846888557c1decdb21a742` | Uses barycentric built-ins for eligible remaining I/J reads while retaining ordinary interpolated attributes. Excludes raw per-vertex parameters, mixed modes and offset/sample reads; not general PerVertexKHR support. |
| [#8612](https://github.com/boykopovar/AnyPS5/pull/8612) | Open; `5e58f2e8af97821ada44f04349c9616f6858816f` | MoltenVK arithmetic forms and signed-zero/separate-rounding regression ideas. Review NaN/denormal behavior and cache identity before considering an adaptation. |
| [#8578](https://github.com/boykopovar/AnyPS5/pull/8578) | Merged in AnyPS5 on October 20; `1b81383137eae9ee0f6854d9aab69cb0b9e88029` | Measures native f32 flush behavior before selecting it; preserves emulation on a mismatch. Different compiler; no local integration. |

Revisions come from each PR's commit links. The rendered changed-file views
were inspected, including the command overlap guard and float-control predicate;
individual commit pages were inaccessible. This is a targeted candidate review,
not full compatibility validation or a refresh of every tracked upstream head.

The thread also separates cross-workgroup visibility from forward-progress
limits and reports intermittent resident-image write-back with watched host
imports. These motivate review against our ownership/download guards; they do
not authorize disabling protection or imports. The proposed exact NaN-region
optimization is still a design, distinct from experiments that relax semantics.
AnyPS5 uses PS5 AGC and the discussed Mac integrations use x86-64/Rosetta;
our PS4 GNM/native ARM64 path needs independent source and synthetic checks.
Do not transfer another title's FPS or another Apple chip's correctness to SotC.
Keep current dependencies and cache 24/9.

## Pre-comparison access-limited review: 2026-10-22 13:54 UTC

Before supplying the consolidated-workspace launch command, public review began
with the SotC reference. Its repository page shows `main` and its
[latest release](https://github.com/AjncSystem/ShadowOfTheColossus-PC/releases/latest)
resolves to v0.60.5. The preferred bloodborne-mac page shows `main`; bmy's page
shows `macos-native-0.6`. Runtime, CPU, SPIRV-Cross and KosmicKrisp repository
pages were also retrieved, but these rendered pages did not establish exact
current heads. GitHub API access was unavailable, local network resolution
failed, and requested commit histories, MoltenVK release and PerVertex PR pages
could not be retrieved. Reference PR #219 was reachable, without a fresh
exact-head/diff comparison in this check.

This is a partial refresh, not confirmation that all heads or proposal states
are unchanged. No new test-relevant candidate could be established. Retain the
previously reviewed revisions below and all selected dependencies/cache 24/9;
do not infer that an unavailable diff is empty. The owner comparison isolates
the already-built busy-queue maintenance correction at 640x360, DMA/async off,
metadata on and pixel capture off. No game/GPU test was started by this review.

## Observed default-branch heads

These heads were resolved through GitHub on 2026-10-22. Observing a head does not
mean its complete diff has been reviewed or imported. Use this baseline to find
new work; production pins remain separate.

| Repository | Branch | Observed head |
|---|---|---|
| `bmy/bbport-mac` | `macos-native-0.6` | [a69f56af2069](https://github.com/bmy/bbport-mac/commit/a69f56af2069df45357e86c27396baa945b53231) |
| `bloodborne-native/bloodborne-mac` | `main` | [ab1f0c57d1e2](https://github.com/bloodborne-native/bloodborne-mac/commit/ab1f0c57d1e25cc130c59be9a874e98099822cc4) |
| `bloodborne-native/bloodborne-runtime` | `main` | [b2ff9cd0b982](https://github.com/bloodborne-native/bloodborne-runtime/commit/b2ff9cd0b982aa481fc48d4ce51b3cc7b4cd2548) |
| `bloodborne-native/bloodborne-recomp` | `main` | [b9652d75edc0](https://github.com/bloodborne-native/bloodborne-recomp/commit/b9652d75edc0a281f06cec0da6ff72e9f5150a70) |
| `KhronosGroup/MoltenVK` | `main` | [5189f8de86ec](https://github.com/KhronosGroup/MoltenVK/commit/5189f8de86ec365860f63bf1da736815cfaea5cd) |
| `KhronosGroup/SPIRV-Cross` | `main` | [300681bbcb83](https://github.com/KhronosGroup/SPIRV-Cross/commit/300681bbcb8365a8d1eacdacdbdd0249bbf1d6b1) |
| `shadexternals/mesa-kosmickrisp` | `main` | [52d87b35f3a4](https://github.com/shadexternals/mesa-kosmickrisp/commit/52d87b35f3a4fc8aff5acb4daaa548c8ff7870f1) |

## bmy review: 2026-10-22

The final public-head refresh advanced the runtime/app heads to those in the
table. Runtime b2ff9cd0's unresolved-GPU-image collection guard is integrated;
it protects valid GPU contents and can retain more memory. Its CPU predicate
regression and a separate retention counter accompany the update. There is no
native SotC/M1 validation of that guard yet.

The later focused source review also finds stale reduced-scene proxy cleanup in
[8185b88d8057](https://github.com/bmy/bbport-mac/commit/8185b88d80573fa300595d5c54450183b4109935),
still present at the current head. Its exact parent/source diff and call site are
verified. A local adaptation now retires stale images/views with past-next
completion and cache invalidation at a drained submission boundary, instead of
the upstream global `Finish`. CPU regression and ARM64 build pass; native SotC/M1
memory savings remain unverified. See the
[focused review](UPSTREAM_REVIEW.md#reduced-scene-proxy-retirement). All watched
default heads and the three PerVertex PR heads/statuses are freshly rechecked and
unchanged; no dependency or compiler/driver pin is updated.

GitHub's repository API resolves the current default branch to
`macos-native-0.6`, head `a69f56af2069df45357e86c27396baa945b53231`.
The cached repository webpage still showed `macos-0.4`; the current API, branch
README and source were checked directly. The old `macos-native` branch returned
404. Do not pin a branch name from a stale cached page.

Its current design uses a separate native ARM64 renderer process with KosmicKrisp;
the game executable still uses Rosetta. Our game CPU translator already runs
natively and must keep doing so. This is a different architecture, not a drop-in
update for our pinned runtime. Its setup selects driver wrapper revision
`52d87b35f3a4fc8aff5acb4daaa548c8ff7870f1`, applies port-specific Mesa patches and
targets macOS 26. Its published gameplay qualification is on M5 Max, not our M1.
Do not run its dependency installer against our prefix: it removes MoltenVK.
KosmicKrisp support/performance with this SotC integration remains untested.

A directly applicable fix was found and adapted from
[5f381d5dd190](https://github.com/bmy/bbport-mac/commit/5f381d5dd190f55b6dae5608a162b6807c688928):
both our production buffer-copy paths also submitted every region through one
`RecordData` call, while the actual scheduler reserves data and its command in
one 128 KiB chunk. Copies now record groups of at most 4096 regions, in order,
with barriers/access tracking outside the grouping as before. A static assertion
includes region storage, alignment and command headroom. The checked overlay
retains the pinned source and original GPL notices; provenance is in its manifest
and source comment.

`python3 tools/check_buffer_copy_chunks.py` compiles the actual pinned/adapted
copy methods and actual scheduler reservation guard against CPU recording
doubles. Oversized cases reproduce the original capacity failure and pass after
splitting; checks preserve region order/data after caller storage changes,
command bounds and barrier/access behavior. The ARM64 library builds. This avoids
a separate CPU recording-capacity failure; it does not resolve our confirmed
Metal out-of-memory error. Both can result in process exit 23 for different reasons.

## Review rules

Before each new owner-supervised native game/GPU comparison, refresh the public
reference repository and whichever tracked upstreams affect the proposed test.
Start with [AjncSystem/ShadowOfTheColossus-PC](https://github.com/AjncSystem/ShadowOfTheColossus-PC),
resolve the actual default branch and compare new commits/releases/updated PRs
with the pins and recorded prior reviews. Inspect relevant diffs, draft/merge
status and exact revisions before deciding whether a backport should precede the
test. Record the observation and preserve an isolated comparison; observation is
not automatic integration. This applies to native game trials, separately from
ordinary offline CPU fixtures and the daily public-source watch. If a source is
temporarily unavailable, retain the known revisions and state the check's limit.

Compare against [current evidence](DEVELOPMENT_HANDOFF.md#continuation-checkpoint)
and [PerVertexKHR research](PERVERTEX_REVIEW.md). Keep draft/merged status, compiler
conversion, native driver acceptance, GPU execution and correct pixels distinct.
Do not transfer Bloodborne object layouts or M4/M5 results to SotC/M1. Evaluate
memory retention as well as CPU savings. Qualify an isolated backport with actual
source regressions and native contracts before changing production dependencies;
preserve source pins and the owner-approved scene/DMA/async settings.
