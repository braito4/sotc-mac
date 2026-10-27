#!/bin/bash
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
unset BB_GC_BUDGET_MB
printf 'Native buffer accounting: default GC budget; single pool; resource journal only.\n'
exec bash "$trial_root/prueba_recursos_pool_unico.command" "$@"
