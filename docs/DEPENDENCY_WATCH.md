# Dependency and reference watch

The owner requested continuing review of upstream changes. A daily public-source
watch is configured to notify only about relevant new commits, releases or PR
updates. It is a review/notification task, not an automatic dependency updater.
Development sessions should also check these sources before treating an old
limitation as current. Resolve each repository's actual default branch, record
the reviewed SHA and inspect the diff and tests.
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

## Observed default-branch heads

These heads were resolved through GitHub on 2026-10-22. Observing a head does not
mean its complete diff has been reviewed or imported. Use this baseline to find
new work; production pins remain separate.

| Repository | Branch | Observed head |
|---|---|---|
| `bmy/bbport-mac` | `macos-native-0.6` | [a69f56af2069](https://github.com/bmy/bbport-mac/commit/a69f56af2069df45357e86c27396baa945b53231) |
| `bloodborne-native/bloodborne-mac` | `main` | [6b23bd27663e](https://github.com/bloodborne-native/bloodborne-mac/commit/6b23bd27663e6a36338433abb9f03cde60b14fa8) |
| `bloodborne-native/bloodborne-runtime` | `main` | [22923f080229](https://github.com/bloodborne-native/bloodborne-runtime/commit/22923f08022909b7e22f7ae8c02ad4c3f6554bfd) |
| `bloodborne-native/bloodborne-recomp` | `main` | [b9652d75edc0](https://github.com/bloodborne-native/bloodborne-recomp/commit/b9652d75edc0a281f06cec0da6ff72e9f5150a70) |
| `KhronosGroup/MoltenVK` | `main` | [5189f8de86ec](https://github.com/KhronosGroup/MoltenVK/commit/5189f8de86ec365860f63bf1da736815cfaea5cd) |
| `KhronosGroup/SPIRV-Cross` | `main` | [300681bbcb83](https://github.com/KhronosGroup/SPIRV-Cross/commit/300681bbcb8365a8d1eacdacdbdd0249bbf1d6b1) |
| `shadexternals/mesa-kosmickrisp` | `main` | [52d87b35f3a4](https://github.com/shadexternals/mesa-kosmickrisp/commit/52d87b35f3a4fc8aff5acb4daaa548c8ff7870f1) |

## bmy review: 2026-10-22

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
