#!/bin/bash
# Compare Metal safe/fast math with the established staging-fixed baseline.
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
case "${1:-strict}" in
    strict) export BB_MSL_SAFE_MATH=1 ;;
    baseline) export BB_MSL_SAFE_MATH=0 ;;
    *) printf 'Usage: bash prueba_calidad_shader.command [strict|baseline]\n' >&2; exit 24 ;;
esac
printf 'Shader comparison: BB_MSL_SAFE_MATH=%s; fresh per-run cache; staging fix enabled.\n' "$BB_MSL_SAFE_MATH"
exec bash "$trial_root/prueba_juego_staging.command"
