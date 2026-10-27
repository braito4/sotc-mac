#!/bin/bash
# Compare only pool geometry against the updated native-renderer baseline.
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export BB_METAL_NATIVE_BOOT=1 BB_GFX_BACKEND=metal BB_POOL_SPLIT=0 BB_RESIDENCY_SET=1
unset BB_POOL_LAYOUT BB_GC_BUDGET_MB
printf 'Updated native runtime: single pool import; residency/default GC unchanged; resource observations; GPU wait 120s.\n'
exec bash "$trial_root/prueba_memoria_contador_120s.command" "$@"
