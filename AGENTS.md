# sotc-mac

This repository develops a native ARM64 macOS diagnostic for Shadow of the Colossus PS4.
Read README.md and docs/STATUS.md before changing the runtime. Public documentation is English.
Development is paused at the owner's request because of unresolved native memory
exhaustion. Read docs/DEVELOPMENT_PAUSE.md first. Latest owner capture (21:16 UTC)
ends on Metal OOM 8 after 185.769 s, exits 23/23, 12.170 GiB; 417 pipeline failures
and 805 SRT errors persist. Do not automatically resume game/device comparisons.
The following checkpoints describe historical evidence and prepared local work.
Cache is now 26/9: checked finite-choice SRT data-read expansion preserves GPU
selection and protects descriptor/pointer dependencies. CPU/build checks pass;
native effects are pending. PerVertex conversion passes Apple syntax for 16 real
pairs in isolation; renderer integration is still absent. Latest owner run
(20:57 UTC, before this adaptation) stops on zero-filled compute-shader metadata
after 78.854 s, exits 23/23; preserve that failure. Read the first handoff.
Newest owner capture (20:53 UTC) verifies native 1920x1080 scene work, no reduced
draws/proxies, 1,325 presentations, then Metal OOM 8 after 123.887 s (23/23),
11.966 GiB. Preset 4 is inherited but upscaler none disables reduction. SRT remains
271; PerVertex failures persist. Image journal drops 8,861 records. Read first handoff.
The owner now selects original scene resolution: preset 0, upscaler none.
Use prueba_grafica_nativa.command to clear inherited 640x360 overrides. DMA and
async stay off. Prior instructions/captures retaining 640x360 are historical;
no native result has yet validated this resolution change.
Prepared SRT correction accepts ReadFirstLane only of proven portable uniform
expressions; 24 synthetic installed-pass cases pass, including induction/lane
negative controls. Cache is now 25/9; pins remain fixed. Staging statistics refresh
on repeated large-buffer reuse without freeing storage. Read the first handoff;
native SRT/pixel/OOM effects remain unverified.
Newest owner capture (20:40 UTC) ends on Metal OOM 8 after 65.352 s, exits 23/23,
11.573 GiB fresh allocation and 680 presentation results. Native SRT provenance
now records 52 samples/eight shaders/six bounded shapes, including Phi induction
recurrences. Do not resolve these by choosing an edge or enabling DMA. Read the
first handoff checkpoint; earlier 20:08 evidence below remains historical.
Pipeline/SRT classification is prepared: 431 failures / 386 shader pairs;
375 have adjacent PerVertexKHR errors, 56 remain unattributed. The checked SRT
diagnostic emits bounded shader/stage/site/IR shapes without guest reads or
resolution changes. CPU tests pass; native samples remain pending. See the
first handoff checkpoint; no cache purge or direct-memory enablement follows.
The consolidated checkout is the only active project. Read
`docs/guide/workspace.md` and the continuation checkpoint before resuming.
The latest 20:08 UTC owner capture ends on Metal OOM 8 after 122.026 s,
confirmed exits 23/23, fresh native allocation 12.469 GiB. Admission is active
(limit/peak 8), but four reserved/inflight commands remain at failure. The
3 GiB upload persists; 485 live images / 1,163.47 MiB and 23 proxies remain.
There are 1,190 presentations: count present_result, not all three JSON lifecycle
events. The preceding 19:53 capture had 1,115 presentations, not 3,345. Audio has
a 15.012 s delivery gap without queue failures. No CPU/decoder/watchdog stop
recurs; their causes remain open. Read the newest handoff; no native fix claimed.
The newest 19:53 UTC owner capture ends on translated SIGSEGV after 118.281 s,
confirmed exits 139/139; the native admission cap is exercised (peak 8). The
preceding 19:49 UTC capped capture still ends on Metal OOM after 121.100 s.
An invalid loaded pointer's source field is outside the previous four-word
snapshot; no writer/lifetime/translation cause is proven. The prepared optional
SOTC_CPU_FAULT_FIELDS reader parses bounded register-relative windows before
threads, preserves guest/GPU guards, rejects address overflow and keeps fatal
exit behavior. Default empty; owner selections/disassembly remain private.
Read the first handoff; do not hardcode game fields, skip loads or claim an OOM
or corruption fix. No new game test is performed for this preparation.
The owner confirms reaching the first gameplay image on Apple M1; the port is not
playable yet. The integer-blit overlay is built and CPU-contract-tested, with
same-format nearest 2D uint/int copies and explicit unsupported-case errors.
The native comparison no longer reports the two float-output/integer-target
errors; exact GPU copy pixels and full-case coverage remain unverified.
This is not an OOM/corruption fix.
Read the first handoff checkpoint and preserve the existing testing guards.
The newest 18:22 UTC owner comparison ends on Metal OOM 8 after 100.149 s,
confirmed exits 23/23 and fresh native allocation 12.434 GiB; the decoder failure
is not reproduced. The checked native admission overlay defaults to eight
reserved/inflight commands in CommitCommandBuffer, releasing slots on actual
completion only; no early split, fake fence or resource eviction. CPU/Foundation
bridge and blocked cooperative-cancellation controls pass, as do ARM64/seventeen
renderer regressions/25 builder tests, full JIT/interpreter contracts and the
46/46 host-symbol audit. Native
memory/performance effects are unverified; eight is not a measured optimum.
SOTC_METAL_INFLIGHT=0 is the uncapped control, 1..64 are explicit limits. Keep
pins/cache, retained references, scene 640x360 and the nested-consumer review.
The checked shader-decode overlay adds supplied-slice bounds checks, safe 64-bit
reads and main/fetch/copy failure provenance, with real exit 23 preserved.
Thirteen installed-decoder CPU cases reproduce a pinned guarded-page overread
and verify explicit adapted rejection. ARM64, sixteen renderer regressions and
25 builder tests, full JIT/interpreter contracts and 46/46 symbol audit pass;
native validation is pending.
SMRD slot 15 is unsupported by current tables, not automatically a new valid
instruction. Fetch synthetic ends/cache memcmp and borrowed-code ownership are
not fixed by these finite-slice guards. Do not claim the last assertion or OOM
resolved. Read the prepared checkpoint before another native comparison.
The earlier 18:06 UTC owner comparison records:
shader-decoder assertion at 95.239 s, confirmed exits 23/23, unknown opcode
1601 in SMRD family 0xc0000000. No recorded Metal OOM/watchdog/CPU fatal signal;
11.447 GiB sampled native peak, final pressure unknown. Preserve explicit decode
failure; unsupported versus corrupt/stale shader input is not distinguished.
Buffer journal saturation limits missing-event claims. Read the first handoff.
The preceding 17:57 UTC owner comparison records:
Metal OOM 8 after 77.670 s, confirmed exits 23/23, native allocation 11.603 GiB.
The weak report-time probe is exercised with status 5/error 8 and real callbacks;
it is diagnostic validation, not an OOM fix. The owner specifies 16 GB unified
memory and wants reduced graphics to lower total pressure. Scene work is 640x360
but original-size images/output and the 3 GiB upload remain; do not equate Metal
allocations with additional dedicated VRAM. Read the first handoff checkpoint.
The new native-state probe adds weak, report-time status/error/device allocation
observations alongside callbacks. CPU journal and actual ARC weak-bridge fixtures
pass, together with the ARM64 build/full contracts; the error path is exercised
in the latest owner comparison. Stall-time probing remains unverified. Never
interpret missing callback status=0 as a sampled NotEnqueued state, or unavailable
native memory as zero pressure. No timeout/order/ownership behavior changes.
The newest 17:29 UTC owner comparison supersedes older boundaries below: GPU
watchdog after 116.824 seconds, confirmed child/supervisor exits 21/31. The awaited
batch has a 26,020-note CPU summary; commit returned, scheduled/completed callbacks
were not observed. Native sampled peak is 11.485 GiB, final pressure unknown.
Image replay records 448 live / 1,111.45 MiB, 817 proxies destroyed / 23 live.
No large staging-tail/shared-copy event, OOM, CPU or audio queue failure occurs.
Prior memory/corruption causes remain unresolved. Do not identify an executing
shader from those CPU notes. Read the current handoff first.
The native Metal retention overlay now requests retained references for initial
and split batches. Production-method Foundation CPU doubles check explicit-resource
lifetime, independent completion, reset and scratch return. ARM64, fourteen
renderer regressions, 25 builder tests and full JIT/interpreter contracts pass;
native validation is pending. This does not fix the installed MoltenVK driver or prove indirect/BDA
ownership, OOM avoidance, corruption resolution or stall recovery. Preserve the
past-next, residency, staging, watchdog and failure-reporting guards.
The submission journal now retains full-batch note summaries and first/last
guest draw/dispatch candidates, with monotonic host observation timings.
ARM64 build and full contracts pass; native validation is pending. Counts are CPU
metadata, not draw totals or GPU execution progress. Do not treat this as a stall fix.
The busy-queue maintenance overlay is integrated, with dependencies/cache 24/9
unchanged and native proxy destruction observed. The latest owner run (15:27 UTC)
stops on GPU progress; the supervisor ends after 136.225 seconds, exit 1, with
child exit unconfirmed despite its cancellation notice. Process-status lookup is
denied; confirm the old process/window is closed before another comparison.
Image replay records 1,037 proxies destroyed / 26 live and 464 total live images /
1,135.69 MiB. Native allocation peaks at 11.584 GiB before the stop; final pressure
is unknown. The 3 GiB upload and registered-image growth remain. The preceding
Metal OOM, SIGBUS allocator corruption and audio queue failure remain unresolved;
none recurs in this capture. Read the current handoff first.
Bounded all-port audio failure snapshots and candidate frame-local CPU reads
are now built and contract-tested; native validation of these additions is pending.
Keep input, selected sources,
installed libraries and new diagnostics under this root. Previous build/source
snapshots are historical; do not launch from an older folder or generated cache.
If moving this checkout, run `tools/relocate_workspace.py` before rebuilding.
The private `work/consolidation/state.json` records the actual root and validation.
The current dependency integration supersedes older pin/cache instructions below:
runtime b2ff9cd0, CPU b9652d75 and reference 411fdd86 are selected, with shader cache
24/9. Read docs/DEPENDENCIES.md and tools/dependencies.json for every library,
latest reviewed version, exact source SHA and open proposal. Keep the inventory
current whenever dependencies or reviewed upstream versions change. Runtime,
CPU and reference updates are integrated at the owner's explicit request;
unmerged PerVertex proposals and alternative driver architectures remain tracked
separately. Preserve the local scene-proxy, coroutine, shader/SRT, audio and staging
corrections when updating upstream. The new GC guard preserves unresolved GPU
contents; record those retention decisions separately from no-pressure downloads.
The versioned workspace key is an integration identifier, not the required HEAD;
the builder verifies the exact selected b2ff9cd0 revision independently.
Use docs/DEVELOPMENT_HANDOFF.md for the current reproduction steps, stopping boundary and debugging priorities; keep it current when those change.
Use https://github.com/bloodborne-native/bloodborne-mac/ as the user's preferred upstream reference when resolving doubts. Follow its runtime/recomp links and review relevant changes against our pins; do not assume Bloodborne-specific layouts apply to SotC or silently update dependencies.
The owner also permanently tracks AjncSystem/ShadowOfTheColossus-PC and
luizgustavs/Pink-shadPS4, alongside bloodborne-mac/runtime/recomp. Resolve current
branches and compare selected and last-reviewed SHAs in tools/dependencies.json;
docs/UPSTREAM_REVIEW.md records the five-source review and decisions. The new
dcaf500 instanced Metal tessellation correction is a checked local adaptation,
not a base-pin update. SotC 0127e36 now questions completed child-frame release;
review queued consumer ownership before another native comparison. Existing CPU
reclamation tests do not establish asynchronous consumer safety. Do not import
Pink's hardcoded heap/stack workarounds or runtime ab31a4c's lossy image eviction.
Also review bmy/bbport-mac, MoltenVK, SPIRV-Cross and KosmicKrisp as requested by
the owner. Read docs/DEPENDENCY_WATCH.md and record branch/SHA changes; the daily
public-source watch does not update pins and is currently paused. The owner also
requested close tracking of https://github.com/boykopovar/AnyPS5/issues/1;
its separate hourly public-source watch is enabled. Review linked PRs against
our PS4/native ARM64 contracts; PS5/Rosetta results are not SotC/M1 validation.
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
