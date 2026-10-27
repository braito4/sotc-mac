#!/bin/bash
# Single-variable comparison of the existing deferred stream-copy optimization.
set -euo pipefail
umask 077
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
if [[ $# -gt 1 ]]; then printf 'Usage: %s [sync|deferred]\n' "$0" >&2; exit 24; fi
copy_mode="${1:-sync}"
case "$copy_mode" in
    sync) copy_mask=16384 ;;
    deferred) copy_mask=0 ;;
    *) printf 'Expected sync or deferred.\n' >&2; exit 24 ;;
esac
mkdir -p "$trial_root/work/copy-argument-trials"
copy_trial=$(mktemp -d "$trial_root/work/copy-argument-trials/trial-XXXXXX")
printf '%s 0\n' "$copy_mask" > "$copy_trial/toggles.txt"
chmod 400 "$copy_trial/toggles.txt"
# Unique file per run; preserve it as evidence and never edit a running trial.
export BB_TOGGLE_FILE="$copy_trial/toggles.txt"
printf 'Stream-copy comparison: mode=%s disabled_mask=%s experiments=0 file=%s\n' "$copy_mode" "$copy_mask" "$BB_TOGGLE_FILE"
printf 'Confirm in the runtime log: Runtime: disabled optimizations mask=%s\n' "$copy_mask"
exec bash "$trial_root/prueba_memoria.command" memory-render
