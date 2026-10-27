#!/bin/bash
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
if [[ $# -eq 0 ]]; then set -- deferred; fi
export SOTC_INDIRECT_PREP_TRACE=1 SOTC_INDIRECT_COPY_TRACE=1 SOTC_COMPUTE_BIND_TRACE=0
exec bash "$trial_root/prueba_copia_argumentos.command" "$@"
