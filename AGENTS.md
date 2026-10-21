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
- The owner has visually confirmed game logos/menu and heard the opening introduction audio. Correct introduction video and audiovisual synchronization remain unverified. In-game 3D scenes, gameplay stability, a playable app and save support remain unverified. Keep claims tied to measured tests.
- The latest instrumented run retains 438 complete flip records and its cancellation notice is observed about 1.47 ms after the stop request; exit status is still unknown at the supervisor's deadline. The owner confirms the window closed itself, and a later targeted lookup returns ESRCH for that recorded PID. This establishes later process absence, not its exit code or exact timing. Preserve the earlier first-logo/menu/audio observations as historical evidence and do not infer image correctness from flip records.
- The owner explicitly authorizes gradual low-resolution GPU tests. The prepared next single comparison changes only BB_UPSCALE_PRESET=4: the pinned host-side scene preset requests 640x360 at the default 1920x1080 output. Keep DMA off, async submission off, JIT/statistics on and a 30-second budget. Confirm actual reduced draws in the log. Guest memory pools, native targets/UI and texture residency do not automatically shrink; no pressure or stability benefit is established. Do not use the Bloodborne-specific BB_RENDER_RES guest-patch path. This tool context's Metal precheck exposes zero devices; do not bypass access restrictions or claim it ran a real game/GPU comparison.
- A host watchdog kernel panic has been reported. The owner has since authorized gradual GPU testing: one brief supervised experiment at a time, one renderer setting changed, then inspect the log before another run. Do not reproduce the earlier DMA-enabled hang or run an automatic sequence. Dynamic shader reads remain disabled and nonzero overrides are rejected before GPU loading; a process supervisor cannot guarantee recovery from a kernel/driver hang.
- The combined game and Metal prerequisite launchers use cooperative cancellation through diagnostic_control.h after an OS cleanup refusal. Keep external-signal fallback disabled in cooperative mode. An unknown child exit status is a failed stop; ensure the old diagnostic window has closed before another instance.
- Check the target remote before pushing. Keep private local state and archives ignored.
