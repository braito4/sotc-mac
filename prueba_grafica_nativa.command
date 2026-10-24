#!/bin/bash
# Owner-supervised original-resolution comparison, using the existing guarded launcher.
set -euo pipefail
# New diagnostic logs and captures are private to the local user.
umask 077
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
# One immutable comparison per process. Private selections remain data.
if [[ $# -gt 1 ]]; then
    printf 'Usage: %s [base|caller|buffer|table]\n' "$0" >&2
    exit 24
fi
trial_variant="${1-}"
if [[ $# == 0 ]]; then
    trial_variant=base
    if [[ -f "$trial_root/work/cpu-experiment-selection.txt" ]]; then
        trial_variant="$(< "$trial_root/work/cpu-experiment-selection.txt")"
    fi
fi
case "$trial_variant" in base|caller|buffer|table) ;; *)
    printf 'Unknown CPU comparison; expected base, caller, buffer or table.\n' >&2
    exit 24 ;;
esac
unset SOTC_CPU_INTERPRET_RANGE SOTC_CPU_EXPERIMENT_PROFILE BB_JIT_DENY
unset SOTC_SKIP_INTRO_SITE
export SOTC_SKIP_INTRO="${SOTC_SKIP_INTRO:-1}"
case "$SOTC_SKIP_INTRO" in 0|1) ;; *) printf 'SOTC_SKIP_INTRO must be 0 or 1.\n' >&2; exit 24;; esac
if [[ "$SOTC_SKIP_INTRO" == 1 ]]; then
    export SOTC_STARTUP_PROFILE="$trial_root/work/startup-no-intro-profile.json"
fi
export SOTC_CPU_EXPERIMENT="$trial_variant"
if [[ "$trial_variant" != base || -f "$trial_root/work/cpu-experiment-profile.json" ]]; then
    export SOTC_CPU_EXPERIMENT_PROFILE="$trial_root/work/cpu-experiment-profile.json"
    trial_range=$(python3 "$trial_root/tools/check_cpu_experiment.py" \
        --profile "$SOTC_CPU_EXPERIMENT_PROFILE" --variant "$trial_variant" --print-range)
    if [[ "$trial_variant" != base ]]; then export SOTC_CPU_INTERPRET_RANGE="$trial_range"; fi
fi
printf 'CPU comparison: %s (one supervised run; base keeps the normal JIT path).\n' "$trial_variant" >&2
# Override inherited low-resolution presets; do not use Bloodborne's guest resolution patch.
unset BB_RENDER_RES BB_OUTPUT_RES
export BB_UPSCALE_PRESET=0 BB_UPSCALER=none
export BB_FULLSCREEN=0
export BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 BB_JIT=1 BB_FRAME_STATS=1
# Capture all reviewed failure families in the same owner-supervised run.
# Selections contain private guest offsets, loaded only as data and never eval'd.
export SOTC_DIAGNOSTIC_JOURNAL="${SOTC_DIAGNOSTIC_JOURNAL:-1}"
export SOTC_SHADER_PROVENANCE="${SOTC_SHADER_PROVENANCE:-1}"
export SOTC_SHADER_SUBMIT_TRACE="${SOTC_SHADER_SUBMIT_TRACE:-1}"
export SOTC_SHADER_BIND_TRACE="${SOTC_SHADER_BIND_TRACE:-1}"
export SOTC_SHADER_COPY_TRACE="${SOTC_SHADER_COPY_TRACE:-1}"
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
