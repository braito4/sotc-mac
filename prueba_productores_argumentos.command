#!/bin/bash
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export SOTC_INDIRECT_WRITER_TRACE=1
exec bash "$trial_root/prueba_indirectos_ordenados.command" "$@"
