#!/bin/bash
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export BB_RESIDENCY_SET=0
printf 'Memory comparison: BB_RESIDENCY_SET=0; explicit Metal residency set disabled.\n'
exec bash "$trial_root/prueba_memoria_contador_120s.command" "$@"
