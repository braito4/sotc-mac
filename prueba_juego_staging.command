#!/bin/bash
# Functional staging fix with optional chain/render diagnostics disabled.
set -euo pipefail
umask 077
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
trial_dir=$(mktemp -d "$trial_root/work/copy-argument-trials/staging-fixed-XXXXXX")
printf '70437463670784 0\n' > "$trial_dir/toggles.txt"
chmod 400 "$trial_dir/toggles.txt"
export BB_TOGGLE_FILE="$trial_dir/toggles.txt"
export BB_METAL_NATIVE_BOOT=1 BB_GFX_BACKEND=metal BB_POOL_SPLIT=1 BB_RESIDENCY_SET=1
unset BB_POOL_LAYOUT BB_GC_BUDGET_MB
export SOTC_UPLOAD_BATCH=1 SOTC_UPLOAD_BATCH_TRIAL=0 SOTC_BUFFER_CHAIN=0
export SOTC_TEST_REDUCTION_BARRIERS=1 SOTC_GPU_WATCHDOG_SECONDS=120 SOTC_DIAGNOSTIC_SECONDS=500
exec bash "$trial_root/prueba_memoria.command" memory-baseline
