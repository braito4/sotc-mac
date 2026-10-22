# Dependency and reference watch

The owner requested continuing review of upstream changes. The broader daily
public-source watch exists but is paused as checked on 2026-10-22. A separate
hourly watch of the owner-requested AnyPS5 macOS thread is now enabled, notifying
only about relevant new findings. Neither task automatically updates dependencies.
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
| [bloodborne-native/bloodborne-mac](https://github.com/bloodborne-native/bloodborne-mac) | Preferred app/reference upstream; follow its runtime links rather than copying app-specific assumptions. |
| [bloodborne-native/bloodborne-runtime](https://github.com/bloodborne-native/bloodborne-runtime) | Our pinned renderer/runtime base; shader, memory, synchronization and lifetime changes. |
| [bloodborne-native/bloodborne-recomp](https://github.com/bloodborne-native/bloodborne-recomp) | Our pinned native CPU translator; instruction/ABI fixes and interpreter/JIT parity. |
| [bmy/bbport-mac](https://github.com/bmy/bbport-mac) | Additional Mac reference requested by the owner; native renderer, memory, driver and recording fixes. Not a direct submodule. |
| [KhronosGroup/MoltenVK](https://github.com/KhronosGroup/MoltenVK) | Installed Vulkan-on-Metal driver; native memory, resource lifetime and experimental PerVertexKHR #2858. |
| [KhronosGroup/SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) | Installed SPIR-V-to-MSL compiler; interface lowering and proposals #2699/#2697. |
| [shadexternals/mesa-kosmickrisp](https://github.com/shadexternals/mesa-kosmickrisp) | Alternative driver used by bmy's port; ARM64, sparse buffers and geometry/interface support. Not installed here. |
| [AnyPS5 macOS issue #1](https://github.com/boykopovar/AnyPS5/issues/1) | Owner-requested discussion watch; follow linked memory-ordering, shader, Metal and host-contract fixes. Different PS5/Rosetta implementation; not a dependency. |

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
