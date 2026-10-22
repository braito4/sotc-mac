# sotc-mac

This repository develops a native ARM64 macOS diagnostic for Shadow of the Colossus PS4.
Read README.md and docs/STATUS.md before changing the runtime. Public documentation is English.
Use docs/DEVELOPMENT_HANDOFF.md for the current reproduction steps, stopping boundary and debugging priorities; keep it current when those change.
Use https://github.com/bloodborne-native/bloodborne-mac/ as the user's preferred upstream reference when resolving doubts. Follow its runtime/recomp links and review relevant changes against our pins; do not assume Bloodborne-specific layouts apply to SotC or silently update dependencies.
Also review bmy/bbport-mac, MoltenVK, SPIRV-Cross and KosmicKrisp as requested by
the owner. Read docs/DEPENDENCY_WATCH.md and record branch/SHA changes; the daily
public-source watch notifies about relevant changes but does not update pins.
Before each new native game/GPU comparison, refresh public upstream state,
starting with AjncSystem/ShadowOfTheColossus-PC and the relevant tracked runtime,
compiler and driver sources. Resolve current default branches and compare new
commits/releases/PR diffs with the pins and prior reviews. Record the check and
any test-relevant candidates before proposing or running the comparison. Do not
silently update dependencies. This pre-test review is separate from the daily watch.

- Maintain code in runtime/src, tests, tools and packaging. work, game and logs are private
  local output/input; never promote their generated content into Git.
- Never publish game binaries, assets, extracted/generated game code, game addresses,
  execution logs, screenshots, recordings or saves. Original synthetic fixture sources are
  allowed. The game input must remain unchanged.
- Use the pinned upstream submodules. Read their instructions before editing them. Preserve
  credits to bloodborne-native, deadinside28/bbport, shadPS4 and the SotC reference project.
- Prefer real runtime contracts with explicit error behavior. Unknown imports stop; missing
  modules remain unavailable. Do not add blanket-success stubs to claim startup progress.
- Keep JIT/interpreter comparisons and meaningful guest ABI tests. Run contracts-test after
  runtime changes and native CPU checks when changing the CPU context bridge.
- The visual journal defaults on in the combined launcher; pixel readback remains
  opt-in via SOTC_VISUAL_CAPTURE=1. Read docs/VISUAL_DIAGNOSTICS.md for limits and
  evidence semantics. Scene stays 640x360. ARM64 build/CPU checks pass; owner runs
  exercise metadata, but pixel capture remains untested and memory still fails.
  bmy's 4096-region copy batching is adapted
  with an original-vs-adapted CPU capacity regression. PerVertex proposals remain
  isolated: docs/PERVERTEX_REVIEW.md records draft status and synthetic conversion
  results, not runtime integration or correct M1 pixels.
- Read the new scene-proxy checkpoint first: the checked retirement overlay removes
  stale UID/mip proxies during OnSubmit, with the draw pipe drained. Images AND
  views remain owned through DeferOperationPastNext (real completion at current
  tick + 1). Clear recent pointers, tracked UIDs and increment scene generation.
  Role/UID/mip evidence distinguishes active/pending/destroyed proxies. The
  production-method CPU regression retains 204 proxies before versus 2 active/202
  destroyed after correction; this is synthetic, not native memory savings.
  The ARM64 incremental build, seven renderer source regressions, 28 builder tests,
  symbol audit and full JIT/interpreter contracts pass. Native validation is pending.
  This adapts bmy's 8185b88d stale-proxy detection without its GPU-wide Finish.
  Keep the coroutine leak candidate separate and the 2.5/3 GiB staging guards intact.
  Read docs/UPSTREAM_REVIEW.md and docs/DEVELOPMENT_HANDOFF.md.
- The combined launcher also defaults SOTC_MEMORY_TRACE=1. Read
  docs/MEMORY_DIAGNOSTICS.md: bounded private buffer origins, real image
  allocation/destruction, queued deletion and GC summaries now accompany the
  existing visual report. The ARM64 build, six renderer source regressions,
  27 builder tests and full JIT/interpreter contracts pass. The newest 09:41Z
  owner run stops after 101.864 seconds on Metal OOM 8, child/supervisor exit 23.
  Its single cross-window copy allocates 2.5 GiB staging, not 3 GiB. The complete
  uncapped image history proves 981 unassociated images/806.35 MiB remain live
  without any such destruction; all are 640x360 or 320x180. Registered inventory
  is 223 images/670.28 MiB. No image deletion is queued at failure. Retained
  per-UID SceneTargets proxies are a strengthened candidate; allocation roles
  are still untagged. Keep cache 23/9 and all resource/fence/import semantics.
  The last GC sample is 35.14 seconds old; read the current handoff checkpoint.
  GC kept/freed upstream samples reset with frame statistics: never interpret
  them as cumulative totals. Reject untracked renderer source/header copies.
- The SotC reference is still pinned at 2250f328. On 2026-10-22 main is observed
  at 411fdd86, 72 commits ahead; v0.60.5 is not integrated as a whole. Its c9be8142
  nested-coroutine leak correction is absent here and is a candidate for a
  separate adaptation. The associated PR 219 remains open/non-draft/unmerged
  despite incorporation into reference main. Read docs/UPSTREAM_REVIEW.md;
  CPU heap measurements from the author are not Apple M1 or Metal OOM evidence.
- The owner now reports audio much improved. Combined diagnostics enable SOTC_AUDIO_TRACE=1
  for bounded per-port timing/queue metadata and output errors. Read
  docs/AUDIO_DIAGNOSTICS.md; input-queue zero is not hardware-underrun evidence.
  The 07:23Z run measures about 47,684 input frames/s per port, up from 23,912,
  and confirms the actual 1024-frame/48 kHz device quantum. Gaps still reach
  121.85 ms; complete playback and audiovisual alignment remain unverified.
  Keep real PCM/error contracts.
- The earlier 08:50Z owner comparison after rollback restores visible output,
  according to the owner, but ends after 94.305 seconds on Metal OOM code 8,
  child/supervisor exit 23. Dedicated upload is one retained 3,072 MiB buffer,
  first observed by 7.784 seconds and unchanged through failure. From the first
  sample after 10 seconds, all 483.55 MiB of tracked Vulkan growth is image
  growth; this is not proof of a leak. Native allocation reaches 11.66 GiB.
  Keep the withdrawn experiment inactive, cache 23/9, scene 640x360, DMA/async
  off and the audio/staging corrections. Read the newest handoff checkpoint.
- The preceding 08:36Z owner run ends after 160.610 seconds on Metal OOM code 8,
  with confirmed child/supervisor exit 23. The owner reports no logos or visible
  scene despite working input. The guarded-buffer-bound experiment records zero
  applied reductions and is withdrawn, including its metadata/cache layout changes.
  The ARM64 renderer is rebuilt from the prior checked overlays, binary/meta
  versions 23/9; source regressions and full JIT/interpreter contracts pass.
  Keep the audio and staging-reuse corrections. At that checkpoint restored
  visible output was unverified; the 08:50Z owner comparison supersedes that limit.
- The earlier 07:23Z run confirms native OOM after 137.642 seconds, child/supervisor
  exit 23, at 11.70 GiB native allocation. It exercises four oversized staging
  reuses; dedicated upload inventory is 2,560 MiB versus 3,072 MiB previously.
  Total staging is 2,972 versus 3,500 MiB, but image allocations grow later and
  memory exhaustion remains. The last staging snapshot is 31.8 seconds old:
  reported reusable bytes are not permission to free live resources. Investigate
  the single 2.5 GiB upload allocation and image lifetimes without changing
  ownership/tick/import guards. Read the newest checkpoint; earlier summaries
  below are historical. Keep all pins and settings.
- An earlier owner-run preset-4 capture confirms Metal out-of-memory failure after
  83.45 seconds: actual NSError domain/code/description, 11.65 GiB native allocation
  versus 11.84 GiB recommended, and a complete two-compute-candidate command tail.
  The completion-error handler self-exits; child/supervisor exit 23 is confirmed,
  without a GPU-watchdog stop. This validates failure handling, not the memory fix.
  Three later samples now contain reduced scene draws; correct pixels remain
  unverified. Keep scene 640x360, DMA/async off and all pins/cache version 23.
  Track allocation classes and safe lifetimes; the culprit resource is unknown.
  Imported windows overlap and device recommendations are not hard physical limits.
  Do not disable split imports or prematurely free retained large staging buffers:
  upstream records rendering/argument-buffer lifetime failures with those changes.
  General PerVertexKHR, dynamic SRT and clamps remain. Read the current handoff
  before the historical summaries below; tool GPU access/glslang are still blocked.
- Read the handoff's Continuation checkpoint first: it supersedes the older run summaries below. The owner now sees the 3D animation with defects. The latest preset-4 run reaches its 120-second wall deadline without a GPU-watchdog/import/CPU-fault report, detects a DualSense, and continues recording activity after connection. Cooperative child exit is unconfirmed (supervisor exit 1); a later targeted lookup finds the process absent. The authorized removal of the automatic game-duration limit is implemented: zero disables both game-duration timers, and the combined launcher defaults to zero and passes it to both layers. Keep GPU-progress cancellation, DMA protection, cooperative stopping and the independent prerequisite budgets. Headless probes still default to 30 seconds; explicit 1..1800-second game runs remain available. No real GPU experiment validates this change yet.
- The owner has visually confirmed game logos/menu, opening introduction audio and now defective 3D animation. Correct scene rendering and audiovisual synchronization remain unverified. Gameplay stability, a playable app and save support remain unverified. Keep claims tied to measured tests.
- The newest owner-run capture omits the low-resolution preset and uses a 30-second budget. It stops on the GPU watchdog after about 19.49 seconds, with unknown child status and supervisor exit 1 despite a cancellation notice; a later targeted lookup finds the PID absent. Its awaited batch retains 96 of 164 metadata notes, including eight guest compute dispatch candidates, transfers and one three-vertex draw. The last compute candidate also has 31 buffer-size clamp warnings. Its queue call returned success and its Metal chunk returned from commit, without scheduled/completed callbacks observed; the preceding chunk completed. These are CPU candidates, not the precise executing GPU command. The CSV is empty. No reduced-scene transition or physical-controller connection is logged. Preserve this changed configuration separately from the preceding complete low-resolution capture.
- The preceding complete owner-run low-resolution test uses a 120-second budget, detects a DualSense and applies 640x360 scene targets. It stops on the GPU watchdog after about 114.91 seconds, with confirmed child exit 21 and supervisor exit 31. Its retained 96-note tail contains indexed indirect draws from two shader pairs that also appear in PerVertexKHR pipeline errors. The full batch has 28632 metadata notes, not draws. No proof of the exact stalled pipeline variant exists. Absence of later shader/blit failures in the newest earlier stop does not validate those paths.
- The checked submission overlay has now produced a native trace. The combined launcher enables SOTC_GPU_SUBMIT_TRACE=1 by default; 0 disables it. A second local overlay rejects empty-handle cached pipelines and prevents null-handle Metal registration. The original source guards reproduce the bug and the adapted guards pass CPU fixtures; the ARM64 library builds. This fixes invalid state acceptance, not PerVertexKHR support or the black scene. Do not register real shader state under a null handle or claim cache acceptance proves drawable support. See the handoff's black-scene analysis and pending-submission section.
- The owner explicitly authorizes gradual low-resolution GPU tests. The configuration is BB_UPSCALE_PRESET=4, DMA off, async submission off and JIT/statistics on. Keep experiments brief and supervised; the latest 120-second-budget run stalls with DMA off. Imported memory remains 6592 MiB; the latest sampled Vulkan allocation/image maxima are 5304/1318 MiB, versus an earlier 5740/1418 MiB. These are not comparable macOS pressure measurements. Integer-format Metal copies, PerVertexKHR pipelines, SRT/Phi offsets, clamps and incomplete GPU-mapping warnings remain. Investigate typed copies and the stalled submission before another game run; do not suppress errors, fake fence completion or extend timeouts as a remedy. Do not use the Bloodborne-specific BB_RENDER_RES guest-patch path. This tool context exposes zero Metal devices; do not bypass access restrictions or claim it executed the owner's native GPU comparison.
- A host watchdog kernel panic has been reported. The owner has since authorized gradual GPU testing: one brief supervised experiment at a time, one renderer setting changed, then inspect the log before another run. Do not reproduce the earlier DMA-enabled hang or run an automatic sequence. Dynamic shader reads remain disabled and nonzero overrides are rejected before GPU loading; a process supervisor cannot guarantee recovery from a kernel/driver hang.
- The combined game and Metal prerequisite launchers use cooperative cancellation through diagnostic_control.h after an OS cleanup refusal. Keep external-signal fallback disabled in cooperative mode. An unknown child exit status is a failed stop; ensure the old diagnostic window has closed before another instance.
- Check the target remote before pushing. Keep private local state and archives ignored.
