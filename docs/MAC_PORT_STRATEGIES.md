# PS4-to-Mac strategies and review reference

Reviewed 2026-10-29. This is a reference for future manual dependency reviews, not an automated subscription. Repository claims and benchmarks are not SotC validation. No reliable count establishes which strategy is most widely used.

| Project | Approach | What to track for SotC |
| --- | --- | --- |
| [bloodborne-native](https://github.com/bloodborne-native/bloodborne-mac) | ARM64 JIT, ahead-of-time recompilation, native Metal; experimental engine-level draw interception | CPU translation costs; shader identity; Metal submission and lifetime fixes. Engine hooks require SotC-specific reverse engineering. |
| [Wooloomooloo2](https://github.com/Wooloomooloo2/bloodborne_mac_port) | Rosetta game process, separate ARM64 Metal renderer sharing memory | Attachmentless render bounds, upload hoisting, pass splits, persistent pipeline caches. Our renderer is already ARM64, so process separation has no automatic translation saving. |
| [bmy/bbport-mac](https://github.com/bmy/bbport-mac) | Rosetta game; separate ARM64 Vulkan renderer through KosmicKrisp | Upstream compatibility, transfer batching, native renderer process design; changing our backend requires controlled comparison. |
| [shadPS4](https://github.com/shadps4-emu/shadPS4) | General PS4 compatibility and Vulkan video core | Shared video-core correctness, PM4, texture overlap and synchronization fixes; adapt instead of replacing our Metal overlay. |
| [PS4Recomp](https://github.com/wie-project/PS4Recomp) | Ahead-of-time x86-to-ARM64 assembly translation | Function discovery, indirect branches and memory ordering. Documentation alone does not establish SotC compatibility. |
| [orbis-on-macos](https://github.com/nacimoualla/orbis-on-macos) | Mac-first JIT, HLE and Metal runtime | Architectural reference; roadmap distinguishes synthetic tests from remaining retail-game support. |
| [Prosperity](https://github.com/Force67/prosperity) | LLE, native AMD64 or FEX on ARM64, Vulkan | Original system-module behavior; published supported platforms do not establish a macOS replacement. |

## Local decisions

- Keep native ARM64 + Metal and the existing SotC corrections.
- Keep texture binding serial: the M1 helper trial fell to approximately 0.3 FPS.
- Reuse compatible shaders: observed archive hits and lower compilation counts. Cache identity includes the renderer binary; rebuilding starts a fresh cache.
- Existing runtime already bounds maximum-sized Apple passes by the screen scissor. Do not claim the full upstream attachmentless optimization was missing.
- Next experiment: use the combined screen/window/generic bottom-right bounds, with window offsets, for attachmentless passes. Preserve the old behavior by default. Include dimensions in the render-state memo key so a changed scissor cannot reuse a stale area.
- Then investigate upload-induced pass splits, CPU recording waits versus real GPU timings, and only then CPU AOT work if profiling supports it.

## Specific upstream changes

- Applied: [ca36c7e / #5524](https://github.com/shadps4-emu/shadPS4/commit/ca36c7e31e9edb958db8d2b94a0721e58eca9daf), compare mip and layer coverage independently.
- Adapted as an opt-in experiment: [0cd6e4f / #5452](https://github.com/shadps4-emu/shadPS4/commit/0cd6e4f2cb6617af12eb8267cca848cc41ad20d7), attachmentless render-area bounds. Our existing screen-scissor optimization and render-state memo require local adaptation.
- Pending review: [7ea677e](https://github.com/shadps4-emu/shadPS4/commit/7ea677e56d75bf3d90ace2b0a5144a665d1f38b6), snapshot packet length before guest-visible completion.
- Pending review: [1b9d159](https://github.com/shadps4-emu/shadPS4/commit/1b9d1591cc624df74866f26a29ab703f257738f8), chained indirect-buffer semantics; preserve our task-lifetime fixes.

## Comparison protocol

Use one compiled renderer for control and experiment, and the same scene, output, CPU workers and shader-cache policy. Report successful archive reuse, FPS and frame-time distribution, actual attachmentless area reductions, memory, and visual regressions. A successful build or synthetic check is not a gameplay result. Revisit the linked commits and upstream measurements before updating this reference.

## 2026-11-02: validate the upstream bbcpu separation

Verified runtime commit `e1b89553e2b0b52204c3344bb35eef4fec22713f` introduces
`https://github.com/bloodborne-native/bbcpu` alongside `recomp`. The reviewed runtime
pins bbcpu `3f9b6c1a11d5a443ee2e556a0d1f3aec4a73f0ca` and recomp
`bc2b17231420353f8727bf94353ec3b0e62dd5cf`. The CPU owns `src/cpu` and the
`src/recomp/rc.h` execution ABI; the recompiler owns analysis and generation tools.
Upstream adjusts include paths, CMake targets, packaged headers and compatibility
history. Their commit reports a Bloodborne comparison; this is not SotC evidence.

### Staged plan (not implemented yet)

| Stage | Change | Acceptance evidence | Rollback |
| --- | --- | --- | --- |
| A: fixed control | Keep current CPU/runtime and the November 2 small backports | Same-scene warm-cache M1 run; record artifacts, CPU/GPU timing, memory | Saved previous library and manifest |
| B: layout only | Introduce independent CPU/recompiler source paths and manifest entries, first using the exact current CPU contents | Host and renderer build; no accidental second CPU library; identical exported ABI; CPU contracts and same-scene behavior | Old source selection |
| C: extracted CPU | Pin standalone bbcpu at the reviewed commit; port each SotC CPU adaptation explicitly | Compare headers and archive members; instruction/fault/TLS/fiber/host-call tests in JIT and interpreter; no silent patch-hash bypass | Previous CPU source and host executable |
| D: optional AOT | Integrate separate recomp tools and ABI tracking, initially a small verified SotC function set | Differential results against interpreter/JIT; validate indirect targets and guest/host returns; quantify translator fallbacks and CPU cost | JIT-only host, independent AOT artifact |
| E: shaders | Review upstream offset/binding and shader-key changes separately | Pipeline correctness, texture/geometry regression checks and cold/warm cache runs | Previous renderer and versioned cache |

Our `tools/build_host.sh` already accepts `SOTC_CPU_SOURCE`, but `tools/build_gpu.py`
still inventories `recomp/src/cpu/bbcpu.h`; packaging and diagnostic scripts must
also be audited. Test scripts that verify CPU source hashes must be adapted with
explicit before/after fingerprints, not bypassed. The new CPU extraction alone
is an organizational change: do not promise higher FPS. QoS preferences are not
hard affinity to performance cores.

Do not mix stages B/C with shader rewrites or AOT. Use a separate build directory
and keep the currently playable host intact. AOT libraries and game-derived
artifacts stay private. Validate ABI compatibility from actual headers and test
calls, not just upstream commit ancestry. No automatic monitoring is configured.

## 2026-11-03: M1 control result after selected backports

User reports substantially better behavior and no visible artifacts in the second
run, `graphics-game-20261102T165305Z.log` (November 2). This is a scene-limited
visual observation, not proof that all artifact cases are fixed. Keep this build
as the control for the CPU/recompiler layout work.

- Normal window close after 69.117 seconds, exit 0.
- Last six FPS reports: 2.0, 2.0, 2.1, 2.2, 2.2, 2.1; median 2.1 FPS.
- Previous cold run's last six reports have median 0.95 FPS. Durations, cache
  state and scene coverage differ; this does not isolate a backport speedup.
- Latest archive report: 1,150 reused pipelines, 179 compiled this session.
- No reported Metal pipeline failures; final memory report 10,762 MiB out of
  a 12,124 MiB working set.
- Keep QoS enabled, two recording workers, four preparation workers, texture
  helper off, attachmentless experiment off, compatible shader cache reuse.
- Stage A now has a positive short gameplay observation. Longer late-game
  texture-pressure coverage remains unverified. Next change is stage B only.
- Host SHA-256: `8a11fa15377b48c9988d0ab55a953b1e827956007cdc27614e55b0f936070c42`.
- Renderer SHA-256: `49a92f56b86451b5a516290d5169cec52f98da60c58abf616721b4046ee99501`.

## 2026-11-03: CPU layout isolation built

Stage B host portion implemented: CPU-only archive builder, independent source
snapshot and isolated host output. Snapshot source files and execution ABI are
byte-identical to pinned `4d9c38444cac9b58c088685042665b8dd5314b54`; no new CPU
behavior is enabled. Recompiler tools are no longer a host build prerequisite.
The renderer remains unchanged and still uses the legacy, identical ABI headers.
Its source-path migration and the standalone newer CPU remain subsequent work.

Validation: native host compiled; ARM64 emitter test passed; native/host calls,
guards and probes passed in JIT and interpreter; translated fault recovery and
watched load/store test passed. Exported host symbol sets match the control;
control host hash is unchanged. Layout gameplay comparison is pending.

New upstream runtime `749daa2501a5cb27bc833824188cc98dfcfbe1c8` was reviewed:
most intervening changes update Bloodborne recompilation metadata; shader-plan
fixes depend on newer pipeline architecture and remain deferred. The background
gamepad setting from `970a0a6` is already enabled in our game launcher. Standalone
bbcpu remains at the previously reviewed revision. Recent shadPS4 rename, mount,
user-color and JPEG changes were inventoried, not applied to this CPU experiment.

### Layout trial: startup timeout correction

`graphics-game-20261103T125307Z.log` selected the isolated host but stopped after
60.680 seconds with `STOP_RENDERER_TIMEOUT`, before guest FPS were reported.
Last pipeline report: 1,000 successful, zero failed. Archive reuse was active
(last report: 774 reused, 10 compiled). This run cannot validate CPU gameplay or
performance. The fixed 60-second renderer-only alarm was also armed for game
initialization and expired during pipeline preparation. Restrict that alarm to
renderer-only checks; game startup remains covered by the launcher's 300-second
budget and cooperative cancellation. Rebuilt the isolated host; control host and
renderer remain unchanged. The corrected game run is pending.

### Corrected layout trial: 20261103T125608Z

The isolated host reached gameplay and closed normally at 158.480 seconds, exit
0, with no STOP marker. Latest pipeline report: 1,500 successful, zero failed.
Gameplay initially reports 1.8–2.3 FPS, then drops to 0.8/0.6/0.4 FPS alongside
129/101/141 shader-pipeline compilations taking 3,894.6/3,555.2/3,991.2 ms in
their respective reporting windows. Final six FPS reports have median 1.25.
The previous control lasted only 69 seconds, so its final 2.1 FPS median is not
a matched-scene comparison and does not establish a layout regression.

Metal allocation peaks at 11,549 MiB; texture-cache usage crosses its 11,517 MiB
critical threshold late in the run. Thermal state remains nominal. Whole-device
GPU samples during sustained gameplay are roughly 75–83%, including other apps;
this does not isolate the game's GPU occupancy or prove an exclusive bottleneck.
Latest archive report has 1,318 reused pipelines and 69 compiled, an earlier
snapshot than the late compilation bursts, not final session totals.

Startup timeout fix is validated by gameplay. Visual correctness still requires
user observation. Next useful performance comparison is this same longer route
with its newly populated cache, before changing CPU behavior or shader semantics.

### Repeated layout trial: 20261103T125958Z

Normal close after 171.748 seconds, exit 0, no STOP markers; latest pipeline
report 1,550 successful, none failed. Last-six FPS median 1.75 versus 1.25 in
the previous 158.480-second run; minimum reported FPS 1.3 versus 0.4. Across
periodic frame reports, compilation totals fall from 544 / 19,589.4 ms to
128 / 6,324.6 ms. These report sums exclude startup and are not the binary
archive's session counters. Cache reuse is active (latest archive: 1,351 hits).
Different route coverage and durations prevent attribution of a precise speedup.

Peak reported Metal allocation 11,377 MiB, final 10,413 MiB; no reported
texture-cache critical-threshold crossing in this run. Eviction remains active.
Whole-device GPU samples after 50 seconds: 76–84%, median 81%, including other
applications. Thermal state remains nominal, up to 79.9 C in the last report.
The repeat supports reduced compilation stalls and basic layout compatibility;
visual correctness remains user-observed. Keep this as the isolated-host control
for stage C rather than requesting another identical baseline run.

## Stage C candidate: standalone bbcpu prepared on 2026-11-03

Candidate `3f9b6c1a11d5a443ee2e556a0d1f3aec4a73f0ca` is materialized independently
and compiled into `work/host-bbcpu-3f9b6c1a11d5/sotc-arm64`. Both the original
and layout-control executables are retained. No renderer rebuild or AOT library
is part of this experiment. Public CPU and RcApi layouts are retained; existing
host exports are a subset of the candidate's, with `bbcpu_trap`,
`bbcpu_block_uncached` and `bbcpu_guest_return` added.

Reviewed changes: interpreter trap handling extracted into a shared helper;
iOS-only JIT disable; x86-only floating-point-control support; uncached decoding
for AOT hashes; hooked-function and guest-return recovery for AOT transitions.
The vector interpreter and host-call assembly are byte-identical. SotC's scalar
and JIT fixes still apply at exactly checked sites; new input and adapted-output
hashes are recorded in the dependency lock. CPU context/fiber and vector wrappers
compile against the candidate headers and source. No AOT-path performance claim.

Validation: 18 local instruction/fiber/thread/TLS test executions (nine fixtures
in interpreter and JIT), four upstream emitter/hooks/fault executions, native
renderer load, retained host exports and modified-input rejection passed. The
local instruction fixtures cover string comparisons, SSE4a, widening, indexed
operations, zero-count shifts and XADD aliasing. Game run remains pending.
Use `bash prueba_cpu_nueva.command`; the launch header identifies the candidate
CPU revision. Revert with `bash prueba_cpu_separada.command`.

### Standalone CPU gameplay: memory failure and next comparison

Run `20261103T131015Z` selected standalone bbcpu and reached gameplay. Latest
pipeline report: 1,600 successful, none failed. Last-six FPS median 2.1 is not a
matched-scene speedup. At 203.163 seconds Metal reported command-buffer error 8,
Insufficient Memory, and exited 23. Allocation was 12,583,862,272 bytes against
a recommended working set of 12,713,115,648 bytes. The recommendation is not a
guaranteed available allocation limit. CPU-update causality is not established.

Prepare `prueba_cpu_memoria.command`: same candidate CPU, renderer and inflight2,
with the existing `BB_GC_BUDGET_MB=11264` applied after launcher resets. This
advances collection thresholds to approximately 7,885 / 9,574 / 10,701 MiB
(trigger / pressure / critical), instead of 8,486 / 10,305 / 11,517 MiB. It is
not a hard memory cap and does not guarantee prevention of another OOM. The
GPU-written valid-image retention predicate remains intact. Earlier eviction
may increase uploads and reduce FPS; validate the same longer route and visuals.
Launcher validation checked default, experiment and invalid settings without
launching the GPU; pool-single and inflight2 remained unchanged. Gameplay pending.

### Earlier collection run: 20261103T181953Z

Verified BB_GC_BUDGET_MB=11264 with standalone CPU and inflight2. Normal window
close at 198.664s, exit 0; no STOP marker, latest 1550 pipelines successful/0
failed. Metal peak reported 11531 MiB, final 9952 MiB (previous OOM run: peak
periodic report 11878 MiB, last 11878 MiB). The collector frees textures, but
this shorter run does not prove elimination of the failure observed at 203s.

Last-six FPS median 1.5 versus 2.1 in the OOM run, with different scene coverage.
This was a cold archive: last report 0 archive hits / 1527 compiled. GC budget is
part of cache identity, explaining why the changed profile did not inherit the
previous archive. Normal-close cache completion marker exists. Next comparison:
same command and longer route (240–270s) using this profile's populated cache.
No renderer/CPU change or relaxed cache compatibility is warranted by this log.

### User clarification: both sessions were closed manually

The user confirms manually closing both 20261103T131015Z and
20261103T181953Z. This supersedes the earlier interpretation that the former
ended spontaneously because of memory exhaustion. Its Metal out-of-memory
message and exit 23 remain real log observations, but their ordering relative
to the user's close action is unknown; they may coincide with shutdown. The
second log records window closure and exit 0. Do not use 203 seconds as a
reproducible failure threshold or require a longer run to cross it. Earlier
collection reduced reported memory in this comparison; neither prevention of
a crash nor the cause of the Metal error has been established.
