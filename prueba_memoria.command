#!/bin/bash
# Mutually exclusive memory investigation profiles; every invocation resets probes.
set -euo pipefail
umask 077
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
# Empty selections prevent the shared launcher from reloading private probe files.
export SOTC_CPU_POINTER_TRACE='' SOTC_CPU_FIELD_TRACE='' SOTC_CPU_FAULT_FIELDS=''
unset SOTC_CPU_POINTER_PROFILE SOTC_CPU_EXPERIMENT_PROFILE SOTC_CPU_INTERPRET_RANGE
unset SOTC_STARTUP_PROFILE SOTC_SKIP_INTRO_SITE BB_JIT_DENY
export SOTC_CPU_EXPERIMENT=base SOTC_REQUIRE_POINTER_TRACE=0 SOTC_SKIP_INTRO=1
export SOTC_STARTUP_PROFILE="$trial_root/work/startup-no-intro-profile.json"
export SOTC_DIAGNOSTIC_JOURNAL=0 SOTC_GPU_SUBMIT_TRACE=0 SOTC_GPU_ENCODER_PROGRESS=0
export SOTC_SHADER_PROVENANCE=0 SOTC_SHADER_SUBMIT_TRACE=0 SOTC_SHADER_BIND_TRACE=0 SOTC_SHADER_COPY_TRACE=0
export SOTC_MEMORY_TRACE=0 SOTC_VISUAL_TRACE=0 SOTC_VISUAL_CAPTURE=0 SOTC_AUDIO_TRACE=0
export SOTC_TASK_GUARD=0 BB_FRAME_STATS=0 BB_WRITE_LOG=0
unset BB_FRAME_LOG BB_RECT_DEBUG BB_GS_DEBUG BB_LOG_LONG_CB_MS
export BB_VK_VALIDATION=0 BB_VK_VALIDATION_SYNC=0 BB_GPU_LOG=critical
# Preserve the functional configuration for an instrumentation-only comparison.
unset BB_RENDER_RES BB_OUTPUT_RES
export BB_UPSCALE_PRESET=0 BB_UPSCALER=none BB_FULLSCREEN=0
export BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 BB_JIT=1 BB_SUBMIT_AHEAD=0
# Keep the proven single-admission comparison constant across observation profiles.
export SOTC_METAL_INFLIGHT=1
memory_profile="${1:-memory-baseline}"
case "$memory_profile" in
    memory-baseline) ;; # Runtime fatal/status messages only.
    memory-resources) export SOTC_MEMORY_TRACE=1 ;; # Allocation/lifetime journal only.
    memory-render) export SOTC_GPU_SUBMIT_TRACE=1 SOTC_GPU_ENCODER_PROGRESS=1 ;; # Pass completion only.
    *) printf 'Expected memory-baseline, memory-resources or memory-render.\n' >&2; exit 24 ;;
esac
if [[ "$memory_profile" == memory-render ]]; then
    export SOTC_INDIRECT_PREP_TRACE="${SOTC_INDIRECT_PREP_TRACE:-0}"
    export SOTC_INDIRECT_COPY_TRACE="${SOTC_INDIRECT_COPY_TRACE:-0}"
    export SOTC_INDIRECT_WRITER_TRACE="${SOTC_INDIRECT_WRITER_TRACE:-0}"
    export SOTC_COMPUTE_BIND_TRACE="${SOTC_COMPUTE_BIND_TRACE:-1}"
else
    unset SOTC_ARGUMENT_INPUT_SHADER SOTC_ARGUMENT_FILL_TRACE
    export SOTC_INDIRECT_WRITER_TRACE=0
    export SOTC_INDIRECT_PREP_TRACE=0 SOTC_INDIRECT_COPY_TRACE=0 SOTC_COMPUTE_BIND_TRACE=0
fi
printf 'Memory experiment profile: %s; Metal admission=1; intro skipped.\n' "$memory_profile"
# Keep cooperative shutdown and the existing renderer watchdog.
export SOTC_DIAGNOSTIC_SECONDS="${SOTC_DIAGNOSTIC_SECONDS:-500}"
if [[ "${SOTC_QUALITY_MEASURE:-0}" == 1 ]]; then
    export BB_FRAME_STATS=1 SOTC_GPU_UTILIZATION=1
    # Single import removes overlapping pool windows; retain all lifetime rules.
    export BB_POOL_SPLIT=0 BB_MSL_SAFE_MATH=1
    export SDL_JOYSTICK_HIDAPI=1 SDL_JOYSTICK_HIDAPI_PS4=1 SDL_JOYSTICK_HIDAPI_PS5=1
    export SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1
fi
printf 'Memory experiment budget %s s.\n' "$SOTC_DIAGNOSTIC_SECONDS"
exec bash "$trial_root/packaging/test-arm64.sh" --graphics-game-test
