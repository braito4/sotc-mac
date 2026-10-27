#!/bin/bash
# Diagnostic only: single pool can change geometry; collect resource lifetimes.
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export BB_POOL_SPLIT=0
unset BB_POOL_LAYOUT
printf 'Resource capture: single imported pool (BB_POOL_SPLIT=0); geometry correctness not assumed.\n'
exec bash "$trial_root/prueba_memoria_sin_residencia.command" "$@"
