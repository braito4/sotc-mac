#!/bin/bash
# Diagnostic comparison only: single imported guest pool may affect geometry.
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export BB_POOL_SPLIT=0
unset BB_POOL_LAYOUT
printf 'Memory comparison: BB_POOL_SPLIT=0; single guest pool import; geometry correctness not assumed.\n'
exec bash "$trial_root/prueba_base_sin_residencia.command" "$@"
