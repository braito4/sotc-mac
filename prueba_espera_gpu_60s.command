#!/bin/bash
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export SOTC_GPU_WATCHDOG_SECONDS=60
exec bash "$trial_root/prueba_sincronizacion_contador.command" "$@"
