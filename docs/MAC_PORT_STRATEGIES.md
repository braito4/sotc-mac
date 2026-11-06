# PS4-to-Mac strategies and review reference

Reference survey from 2026-10-29; continuation priorities updated 2026-11-06. This is a reference for future manual dependency reviews, not an automated subscription. Repository claims and benchmarks are not SotC validation. No reliable count establishes which strategy is most widely used.

| Project | Approach | What to track for SotC |
| --- | --- | --- |
| [bloodborne-native](https://github.com/bloodborne-native/bloodborne-mac) | ARM64 JIT, ahead-of-time recompilation, native Metal; experimental engine-level draw interception | CPU translation costs; shader identity; Metal submission and lifetime fixes. Engine hooks require SotC-specific reverse engineering. |
| [Wooloomooloo2](https://github.com/Wooloomooloo2/bloodborne_mac_port) | Rosetta game process, separate ARM64 Metal renderer sharing memory | Attachmentless render bounds, upload hoisting, pass splits, persistent pipeline caches. Our renderer is already ARM64, so process separation has no automatic translation saving. |
| [bmy/bbport-mac](https://github.com/bmy/bbport-mac) | Rosetta game; separate ARM64 Vulkan renderer through KosmicKrisp | Upstream compatibility, transfer batching, native renderer process design; changing our backend requires controlled comparison. |
| [shadPS4](https://github.com/shadps4-emu/shadPS4) | General PS4 compatibility and Vulkan video core | Shared video-core correctness, PM4, texture overlap and synchronization fixes; adapt instead of replacing our Metal overlay. |
| [PS4Recomp](https://github.com/wie-project/PS4Recomp) | Ahead-of-time x86-to-ARM64 assembly translation | Function discovery, indirect branches and memory ordering. Documentation alone does not establish SotC compatibility. |
| [orbis-on-macos](https://github.com/nacimoualla/orbis-on-macos) | Mac-first JIT, HLE and Metal runtime | Architectural reference; roadmap distinguishes synthetic tests from remaining retail-game support. |
| [Prosperity](https://github.com/Force67/prosperity) | LLE, native AMD64 or FEX on ARM64, Vulkan | Original system-module behavior; published supported platforms do not establish a macOS replacement. |

## Current integration policy

Keep native ARM64 CPU execution and Metal rendering. The component layout follows
bloodborne-native: CPU execution, recompiler tools, platform services and renderer
have separate ownership and explicit pinned revisions. The source extraction and
optional standalone CPU build are implemented; an extraction alone is not a speedup.
See [repository workflow](REPOSITORY_WORKFLOW.md) for acceptance gates and rollback.

Review upstream changes against `tools/dependencies.json` before updating a pin.
An upstream gameplay report does not validate SotC's ABI, memory ownership or Metal
behavior. Keep synchronization and resource-lifetime corrections when adapting
performance work. Do not combine a dependency update with a new memory experiment.

## Candidates and limits

| Area | Local position | Next evidence needed |
| --- | --- | --- |
| Shader caching | Reuse compatible persisted pipelines; preserve exact renderer/cache identity | Compare matched cold and warm runs; cache hits alone do not explain steady-state FPS |
| CPU graphics preparation | Preparation workers already exist; the extra texture-binding worker regressed on M1 and remains off | Worker busy time, queue waits, bandwidth and frame timings before increasing concurrency |
| Copy scheduling | Exact reads and interval overlap searches remain separate experiments | Demonstrate the eligible copy-hoisting path, preserve dependencies and compare the same scene |
| Staging reuse | Optional bounded size classes; RAM-based retention thresholds | Peak memory and completed reuse on each RAM tier; additional RAM is not a total allocation guarantee |
| Vegetation and quality | Optional generic shader parameter scaling and engine options | Derive private shader selections locally; validate appearance and FPS per feature |
| Texture aliases | Isolated correctness trial; a prior trial stalled without an established cause | Lifetime, mip/layer identity and GPU progress before promotion |
| Shared buffers | Direct and fragmented paths differ in observed input validity and output propagation | Trace initialization, descriptor identity, mapping generations and completed writes |

Concrete review references and their status at review time are recorded in
[November 5 review](REFERENCE_REVIEW_20261105.md) and
[November 6 review](REFERENCE_REVIEW_20261106.md). They are historical reviews,
not claims about the current state of every upstream branch or pull request.
The [prepared strategies](STRATEGIES_READY_20261105.md) describe earlier isolated
comparisons; their private launchers are not included in a public checkout.

## Comparison protocol

Change one variable, retain the known control build and record source fingerprints,
CPU/renderer versions, RAM tier, resolution, worker counts and cache state. Compare
the same route and report frame-time/FPS distributions, actual feature activation,
memory pressure, visual defects and termination reason. Diagnostic synchronization
and readback can alter performance and timing. A normal close in one run is not
proof of a crash fix; a passing synthetic check is not gameplay validation.

The [contributor handoff](DEBUGGING.md) records the latest bounded observations,
remaining gaps and the next investigation. Game addresses, decoded instructions,
raw captures and session logs remain private.
