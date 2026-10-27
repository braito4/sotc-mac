#!/bin/bash
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export BB_METAL_NATIVE_BOOT=1 BB_GFX_BACKEND=metal BB_POOL_SPLIT=1 BB_RESIDENCY_SET=1
unset BB_POOL_LAYOUT BB_GC_BUDGET_MB
export SOTC_BUFFER_CHAIN=1 SOTC_GPU_WATCHDOG_SECONDS=120 SOTC_DIAGNOSTIC_SECONDS=500
export SOTC_COMPUTE_BIND_TRACE=1
exec bash "$trial_root/prueba_sincronizacion_contador.command"
