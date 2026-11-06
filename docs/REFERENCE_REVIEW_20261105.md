# Reference review — 5 November 2026

Scope: source and open-PR review against the actual local renderer and the memory
source used to build the current diagnostic host. No executable, dependency pin,
game input or launch setting changed. GitHub API responses and diffs are retained
privately under work/analysis/reference-review-20261105 and shader-origin-20261105.
Large compare responses and the latest/open PR pages are not an exhaustive audit.

## Recommended order

| Priority | Candidate | Local evidence and decision |
|---|---|---|
| 1: stability | SotC physical-page ownership on pool release | Absent from the built host memory source. Investigate and adapt separately; upstream ties it to reused pages being released again. Similar symptoms are not proof of our cause. |
| 2: graphics correctness | shadPS4 #5627, final image backing and subresource offsets | Our code retains intermediate overlap mip/slice values and the older array-mode/slice rules. Adapt with synthetic mip/layer cases, including our extra descriptor cache. |
| 3: performance | Bloodborne lazy texture usages | Absent locally. Avoid unnecessary storage/format-view capabilities until needed; evaluate Metal memory/texture behavior on M1. Must migrate usage-upgrade copies and image lifetime together. |
| 4: performance | Bloodborne #11 interval sets for copy hoisting | Exact-read toggle was already tested here without a convincing win; interval-set implementation is absent. Isolate the data-structure optimization with coarse tracking retained. |
| Later, separately | Bloodborne #12 encoder ordering/pass preservation | Absent locally. Changes synchronization, so require dedicated fence/order tests after memory correctness. Do not stack this onto the current fault capture. |

## Memory ownership is the strongest new lead

[Reference commit f2732678](https://github.com/AjncSystem/ShadowOfTheColossus-PC/commit/f2732678e5624c4bc929a5326f812151efb3fad3)
records the last mapping address of each physical page. During a particular guest
pool-release query, it suppresses committed status for pages mapped elsewhere
since. This is a game-specific workaround, not a general VirtualQuery fix.
The upstream implementation selects that caller using a native return address and
a game-specific hook. Our ARM64 JIT/import bridge cannot copy that host-PC check:
a guest call-site identity must be verified against our prepared version, with
private signatures kept out of public code. Legitimate aliases must continue to
work. First validate release/remap ownership with synthetic sequences and an
observational guest-call trace; do not globally hide stale mappings.

A related flexible-memory split fix already exists in our built host: splitting
both DIRECT and FLEXIBLE regions advances the backing offset. Reimporting the
reference's corresponding fix would add no new correction.

The reference also releases the batch-map lock between entries. Our host holds it
across the whole batch. This is a plausible CPU-contention candidate, but requires
measurement and ordering/atomicity review, including our deferred GPU hooks.
Do not infer its benefit from the reference's Windows figures.

## Texture changes

[shadPS4 #5627](https://github.com/shadps4-emu/shadPS4/pull/5627), open at
`d85003474ea8`, changes tiled mip matching, layer stride/bounds, and derives view
offsets from the final resolved image. The local descriptor cache also retains
these offsets, so modifying only the upstream slow path would be incomplete.
Test odd layers, nonzero descriptor bases, mixed mip tiling, overlap chains and
both cold/cache-hit paths before gameplay. This candidate addresses texture
selection; it does not directly fix an invalid shader-code pointer.

[Bloodborne commit 316e0eb2](https://github.com/bloodborne-native/bloodborne-runtime/commit/316e0eb2cdecc8b452760f3d499fc64ac15b3297)
creates eligible textures with fewer usage requirements and upgrades when an
actual view needs more. The patch excludes stencil, multisample and block-image
cases and coordinates helper-thread restrictions with copy/upgrade operations.
This is a performance candidate for our Metal path, not a measured M1 gain.

The upstream preservation of unresolved GPU-only image contents under critical
GC pressure is already present locally (including the GpuDirty exception).

## Synchronization and shader provenance

[shadPS4 #5628](https://github.com/shadps4-emu/shadPS4/pull/5628), open at
`339feb0a2fcc`, honors explicit cache visibility requests and preserves requested
GPU writes before CPU access. Its dirty-page machinery is absent here. Our native
unified-memory path and fallback copies differ: importing the Vulkan page tracker
alone cannot recover output from a staging copy that is never copied back.
Use its synthetic partial-write/coherency tests as a reference for a dedicated
native implementation. Keep this separate from speculative shader repair.

[Bloodborne #11](https://github.com/bloodborne-native/bloodborne-runtime/pull/11)
remains at `683602c64c3a`, the previously reviewed head. Exact reads and interval
sets are separable changes. Our exact-read experiment did not establish a fix;
repeating it unchanged is not the next recommendation.

[Bloodborne #12](https://github.com/bloodborne-native/bloodborne-runtime/pull/12)
remains open at `872ee9bd3376`: pass preservation, explicit Metal fence ordering
and small compute copies. Require independent validation with our single-pool
imports, index rebasing and transient copy ownership before enabling.

## Other references and exclusions

- Pink main `4a3511ba9aa3`: recent constant-ring/label ordering and producer/shared
  packet separation warrant a subsequent actual-source comparison. Latest commit
  summaries alone do not establish applicability or justify import.
- bmy main `a69f56af2069`: recent work adopts KosmicKrisp and reviews driver fixes.
  Our active path is native Metal; switching drivers would confound this test.
- shadPS4 main `98ef621decb6`: reviewed PRs above are still open. #5586 adjusts image
  refresh ordering, but its reported driver-specific black-image case requires
  tracing our own binding order before adapting. #5575 avoids repeated small
  buffer uploads; our unified-memory bindings already bypass many such uploads,
  so measure fallback traffic before adopting it.
- Bloodborne app `5e6e8e714550`, runtime `42736cf7ca77`, recomp `c60ccf8be752`:
  many changes are app/game annotations or compilation planning. Do not transplant
  Bloodborne shader catalogs or guest-specific precompile plans into SotC.
- Standalone bbcpu stays at `3f9b6c1a11d5`, already available as an unpromoted trial.
- MoltenVK and SPIRV-Cross head comparisons were refreshed in the preceding review;
  no driver/compiler update is needed for this diagnostic profile.

Next decision: finish the prepared shader-origin capture, then prioritize page
ownership verification. Texture alias correction and lazy usages should each have
their own patch/test series. None of these upstream claims proves our crash fixed.

## Follow-up: SotC 0.72 grass and current main

Refreshed main remains ed5c721ba47a; the substantive changes reviewed are
[a2479f79 (0.72)](https://github.com/AjncSystem/ShadowOfTheColossus-PC/commit/a2479f79eece0d3db0e90530321eb5dce99cd7cb)
and [32ca3825 (0.72.1)](https://github.com/AjncSystem/ShadowOfTheColossus-PC/commit/32ca38257ef2978111cdb0943f46b14977a67b0b).
Read the actual launcher and renderer sources, not just release summaries.

Grass: 0.72 restores console values in all preset play modes; Custom retains
controls. Previously Pro/Extreme forced fuller vegetation, and Extreme extended
vegetation and shadows farther. The detail scale is inverse:1 is PS4, smaller
values retain fuller grass at distance. Existing density bands and late appearance
near the player are described upstream as console behavior; this does not explain
our unverified flicker or colored corruption. The change is primarily a preset
reset, not a new vegetation-renderer repair.

Our active native renderer has no SOTC_GRASS parser or the corresponding shader
parameter overrides. It is not inheriting the Windows launcher's enhanced grass
presets, so merely exporting those variables or resetting them to1 would not
implement a change. A useful adaptation would be optional per-shader distance/LOD
controls plus independently validated shadow/fade settings, preserving neutral
values by default. Identify our actual shader variants and validate buffer bounds;
do not transplant guest signatures or game-version-specific offsets into public
source. Start with lower shadow cost or shorter vegetation range as separate
quality/performance trials; increasing detail is a visual enhancement with a
potential GPU cost. No such adjustment has been enabled in this review.

Other candidates, checked against local behavior:

- Staging reserves/power-of-two size classes may smooth streaming bursts. Our local
  large-buffer path already reuses best-fit completed buffers beyond the old1.25x
  bound, so that part is not a new benefit. Any reserve needs a small explicit
  unified-memory budget and existing completion/ownership checks on the16GB M1;
  do not blindly copy separate host/device reserve totals from a discrete GPU.
- Whole-small-image hashing already exists locally. Review the reference's
  post-upload baseline maintenance separately; do not advertise whole-image
  hashing as a missing improvement. Its upload deferral can temporarily show
  incomplete textures and should not be mixed into the corruption investigation.
- The lighter0.72.1 memory profile changes launcher texture mip/retention choices
  for sub8GB cards. Its discrete-VRAM autodetection is not an appropriate M1
  unified-memory capacity test. The policy idea is useful, not a direct launcher
  transplant; engine LOD controls would first need a native adaptation.
- Fast readbacks, finer texture mips, longer texture-bank retention and heap guard
  shortcuts were enabled upstream after its pool-owner fix. We have not established
  equivalent ownership behavior, so defer that bundle and verify ownership first.
- The Vulkan FSR4 SDK integration loads a Windows runtime library and uses that
  renderer's resource path. It is not a drop-in optimization for native Metal.
  The reported PC FPS gain from fast readbacks is not a grass or M1 result.
- PrefetchVirtualMemory is Windows-specific. Consider a macOS equivalent only if
  measurements show matching backing-page fault bursts, not by API-name substitution.

Order after the already-prepared archive scheduling comparison: optional neutral
vegetation controls and a conservative shadow-cost trial; measured staging reuse
or allocation smoothing; ownership validation before aggressive readbacks/LOD.
Keep each candidate isolated, and preserve the still-unresolved memory/stall fault
as a separate investigation. This review changes no runtime or launch behavior.
