#!/bin/bash
# Focused GPU stall/memory investigation; no CPU field/pointer or shader writer probes.
set -euo pipefail
umask 077
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
# Empty selections prevent the shared launcher from reloading private probe files.
export SOTC_CPU_POINTER_TRACE='' SOTC_CPU_FIELD_TRACE='' SOTC_CPU_FAULT_FIELDS=''
unset SOTC_CPU_POINTER_PROFILE SOTC_CPU_EXPERIMENT_PROFILE SOTC_CPU_INTERPRET_RANGE
unset SOTC_STARTUP_PROFILE SOTC_SKIP_INTRO_SITE BB_JIT_DENY
export SOTC_CPU_EXPERIMENT=base SOTC_REQUIRE_POINTER_TRACE=0 SOTC_SKIP_INTRO=1
export SOTC_STARTUP_PROFILE="$trial_root/work/startup-no-intro-profile.json"
export SOTC_DIAGNOSTIC_JOURNAL=0 SOTC_GPU_SUBMIT_TRACE=1 SOTC_GPU_ENCODER_PROGRESS=1
export SOTC_SHADER_PROVENANCE=0 SOTC_SHADER_SUBMIT_TRACE=0 SOTC_SHADER_BIND_TRACE=0 SOTC_SHADER_COPY_TRACE=0
export SOTC_MEMORY_TRACE=1 SOTC_VISUAL_TRACE=0 SOTC_VISUAL_CAPTURE=0 SOTC_AUDIO_TRACE=0
export SOTC_TASK_GUARD=0 BB_FRAME_STATS=1 BB_WRITE_LOG=0
unset BB_FRAME_LOG BB_RECT_DEBUG BB_GS_DEBUG BB_LOG_LONG_CB_MS
export BB_VK_VALIDATION=0 BB_VK_VALIDATION_SYNC=0 BB_GPU_LOG=critical
# Preserve the functional configuration for an instrumentation-only comparison.
unset BB_RENDER_RES BB_OUTPUT_RES
export BB_UPSCALE_PRESET=0 BB_UPSCALER=none BB_FULLSCREEN=0
export BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 BB_JIT=1 BB_SUBMIT_AHEAD=0
# Compare startup memory pressure with only one native command buffer admitted.
export SOTC_METAL_INFLIGHT="${SOTC_METAL_INFLIGHT:-1}"
# Keep cooperative shutdown and the existing renderer watchdog.
export SOTC_DIAGNOSTIC_SECONDS="${SOTC_DIAGNOSTIC_SECONDS:-500}"
printf 'Focused GPU render/checkpoint/memory capture; intro skipped; CPU and shader writer probes off; budget %s s.\n' "$SOTC_DIAGNOSTIC_SECONDS"
exec bash "$trial_root/packaging/test-arm64.sh" --graphics-game-test
