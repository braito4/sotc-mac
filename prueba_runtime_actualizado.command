#!/bin/bash
# New renderer baseline, with resource accounting and the existing shader trial.
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export BB_METAL_NATIVE_BOOT=1 BB_GFX_BACKEND=metal BB_POOL_SPLIT=1 BB_RESIDENCY_SET=1
unset BB_POOL_LAYOUT BB_GC_BUDGET_MB
printf 'Updated runtime a27f0ca477bd: native Metal boot; split pool; default GC/residency; resource observations.\n'
exec bash "$trial_root/prueba_memoria_contador_120s.command" "$@"
