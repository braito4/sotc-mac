#!/bin/bash
# Diagnostic collector budget; this is not a hard Metal allocation limit.
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export BB_GC_BUDGET_MB=9216
printf 'Collector comparison: BB_GC_BUDGET_MB=9216 MiB; earlier collection thresholds, not a memory cap.\n'
exec bash "$trial_root/prueba_recursos_pool_unico.command" "$@"
