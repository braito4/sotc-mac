#!/bin/bash
# Owner-supervised original-resolution comparison, using the existing guarded launcher.
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
# Override inherited low-resolution presets; do not use Bloodborne's guest resolution patch.
unset BB_RENDER_RES BB_OUTPUT_RES
export BB_UPSCALE_PRESET=0 BB_UPSCALER=none
export BB_FULLSCREEN=0
export BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 BB_JIT=1 BB_FRAME_STATS=1
# Capture all reviewed failure families in the same owner-supervised run.
# Selections contain private guest offsets, loaded only as data and never eval'd.
export SOTC_DIAGNOSTIC_JOURNAL="${SOTC_DIAGNOSTIC_JOURNAL:-1}"
export SOTC_SHADER_PROVENANCE="${SOTC_SHADER_PROVENANCE:-1}"
# Bounded CPU-frame quarantine for this owner diagnostic; explicit 0 opts out.
export SOTC_TASK_GUARD="${SOTC_TASK_GUARD:-1}"
if [[ -z ${SOTC_CPU_POINTER_TRACE+x} && -f "$trial_root/work/cpu-pointer-trace.txt" ]]; then
    export SOTC_CPU_POINTER_TRACE="$(< "$trial_root/work/cpu-pointer-trace.txt")"
    export SOTC_CPU_POINTER_PROFILE="$trial_root/work/cpu-pointer-profile.json"
fi
if [[ ${SOTC_REQUIRE_POINTER_TRACE:-0} == 1 ]]; then
    if [[ -z ${SOTC_CPU_POINTER_TRACE:-} || ! -f ${SOTC_CPU_POINTER_PROFILE:-} ]]; then
        printf 'Pointer capture required: missing private selection or reviewed profile; game not launched.\n' >&2
        exit 24
    fi
fi
if [[ -z ${SOTC_CPU_FIELD_TRACE+x} && -f "$trial_root/work/cpu-combined-trace.txt" ]]; then
    export SOTC_CPU_FIELD_TRACE="$(< "$trial_root/work/cpu-combined-trace.txt")"
fi
if [[ -z ${SOTC_CPU_FAULT_FIELDS+x} && -f "$trial_root/work/cpu-combined-fault-fields.txt" ]]; then
    export SOTC_CPU_FAULT_FIELDS="$(< "$trial_root/work/cpu-combined-fault-fields.txt")"
fi
exec bash "$trial_root/packaging/test-arm64.sh" --graphics-game-test
