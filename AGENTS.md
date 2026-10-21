# sotc-mac

This repository develops a native ARM64 macOS diagnostic for Shadow of the Colossus PS4.
Read README.md and docs/STATUS.md before changing the runtime. Public documentation is English.
Use docs/DEVELOPMENT_HANDOFF.md for the current reproduction steps, stopping boundary and debugging priorities; keep it current when those change.
Use https://github.com/bloodborne-native/bloodborne-mac/ as the user's preferred upstream reference when resolving doubts. Follow its runtime/recomp links and review relevant changes against our pins; do not assume Bloodborne-specific layouts apply to SotC or silently update dependencies.

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
- The newest no-deadline native run exits after about 160.93 seconds at sched_yield,
  with child/supervisor exit 20 and no GPU-watchdog stop. Scoped native POSIX yield
  bindings now pass full JIT/interpreter contracts. The checked shader overlay keeps
  wave64 scratch in one U32 view and applies only the exact vertex-zero Flat rewrite
  where provoking vertex is fixed to the first vertex,
  before new/cached MoltenVK module creation. Nine original compute fixtures reproduce
  the missing declaration; nine corrected fixtures convert. Other vertex selections
  remain explicitly unsupported. The ARM64 library is rebuilt incrementally; a fresh
  full build needs restoration of the missing external glslang executable. No game
  run validates these corrections yet. Read the first handoff checkpoint before the
  historical run/duration summaries below.
- Read the handoff's Continuation checkpoint first: it supersedes the older run summaries below. The owner now sees the 3D animation with defects. The latest preset-4 run reaches its 120-second wall deadline without a GPU-watchdog/import/CPU-fault report, detects a DualSense, and continues recording activity after connection. Cooperative child exit is unconfirmed (supervisor exit 1); a later targeted lookup finds the process absent. The authorized removal of the automatic game-duration limit is implemented: zero disables both game-duration timers, and the combined launcher defaults to zero and passes it to both layers. Keep GPU-progress cancellation, DMA protection, cooperative stopping and the independent prerequisite budgets. Headless probes still default to 30 seconds; explicit 1..1800-second game runs remain available. No real GPU experiment validates this change yet.
- The owner has visually confirmed game logos/menu, opening introduction audio and now defective 3D animation. Correct scene rendering and audiovisual synchronization remain unverified. Gameplay stability, a playable app and save support remain unverified. Keep claims tied to measured tests.
- The newest owner-run capture omits the low-resolution preset and uses a 30-second budget. It stops on the GPU watchdog after about 19.49 seconds, with unknown child status and supervisor exit 1 despite a cancellation notice; a later targeted lookup finds the PID absent. Its awaited batch retains 96 of 164 metadata notes, including eight guest compute dispatch candidates, transfers and one three-vertex draw. The last compute candidate also has 31 buffer-size clamp warnings. Its queue call returned success and its Metal chunk returned from commit, without scheduled/completed callbacks observed; the preceding chunk completed. These are CPU candidates, not the precise executing GPU command. The CSV is empty. No reduced-scene transition or physical-controller connection is logged. Preserve this changed configuration separately from the preceding complete low-resolution capture.
- The preceding complete owner-run low-resolution test uses a 120-second budget, detects a DualSense and applies 640x360 scene targets. It stops on the GPU watchdog after about 114.91 seconds, with confirmed child exit 21 and supervisor exit 31. Its retained 96-note tail contains indexed indirect draws from two shader pairs that also appear in PerVertexKHR pipeline errors. The full batch has 28632 metadata notes, not draws. No proof of the exact stalled pipeline variant exists. Absence of later shader/blit failures in the newest earlier stop does not validate those paths.
- The checked submission overlay has now produced a native trace. The combined launcher enables SOTC_GPU_SUBMIT_TRACE=1 by default; 0 disables it. A second local overlay rejects empty-handle cached pipelines and prevents null-handle Metal registration. The original source guards reproduce the bug and the adapted guards pass CPU fixtures; the ARM64 library builds. This fixes invalid state acceptance, not PerVertexKHR support or the black scene. Do not register real shader state under a null handle or claim cache acceptance proves drawable support. See the handoff's black-scene analysis and pending-submission section.
- The owner explicitly authorizes gradual low-resolution GPU tests. The configuration is BB_UPSCALE_PRESET=4, DMA off, async submission off and JIT/statistics on. Keep experiments brief and supervised; the latest 120-second-budget run stalls with DMA off. Imported memory remains 6592 MiB; the latest sampled Vulkan allocation/image maxima are 5304/1318 MiB, versus an earlier 5740/1418 MiB. These are not comparable macOS pressure measurements. Integer-format Metal copies, PerVertexKHR pipelines, SRT/Phi offsets, clamps and incomplete GPU-mapping warnings remain. Investigate typed copies and the stalled submission before another game run; do not suppress errors, fake fence completion or extend timeouts as a remedy. Do not use the Bloodborne-specific BB_RENDER_RES guest-patch path. This tool context exposes zero Metal devices; do not bypass access restrictions or claim it executed the owner's native GPU comparison.
- A host watchdog kernel panic has been reported. The owner has since authorized gradual GPU testing: one brief supervised experiment at a time, one renderer setting changed, then inspect the log before another run. Do not reproduce the earlier DMA-enabled hang or run an automatic sequence. Dynamic shader reads remain disabled and nonzero overrides are rejected before GPU loading; a process supervisor cannot guarantee recovery from a kernel/driver hang.
- The combined game and Metal prerequisite launchers use cooperative cancellation through diagnostic_control.h after an OS cleanup refusal. Keep external-signal fallback disabled in cooperative mode. An unknown child exit status is a failed stop; ensure the old diagnostic window has closed before another instance.
- Check the target remote before pushing. Keep private local state and archives ignored.
