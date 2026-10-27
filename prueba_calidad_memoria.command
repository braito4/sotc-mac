#!/bin/bash
# Strict shaders, nonoverlapping guest import, frame/device measurements, Sony input.
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export SOTC_QUALITY_MEASURE=1
exec bash "$trial_root/prueba_juego_staging.command"
