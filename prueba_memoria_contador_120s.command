#!/bin/bash
# Resource lifetimes with the same functional settings as the reduction trial.
set -euo pipefail
umask 077
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
if [[ $# -ne 0 ]]; then printf 'This trial takes no arguments.\n' >&2; exit 24; fi
mkdir -p "$trial_root/work/copy-argument-trials"
copy_trial=$(mktemp -d "$trial_root/work/copy-argument-trials/trial-XXXXXX")
printf '70437463670784 0\n' > "$copy_trial/toggles.txt"
chmod 400 "$copy_trial/toggles.txt"
export BB_TOGGLE_FILE="$copy_trial/toggles.txt"
export SOTC_TEST_REDUCTION_BARRIERS=1
export SOTC_GPU_WATCHDOG_SECONDS=120 SOTC_DIAGNOSTIC_SECONDS=500
printf 'Resource lifetime trial: barriers ON; mask=70437463670784; GPU wait=120s; global=500s.\n'
exec bash "$trial_root/prueba_memoria.command" memory-resources
