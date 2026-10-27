#!/bin/bash
# A/B against origin sync: disable only the existing indirect-command pipeline bit.
# The upstream bit affects BOTH DrawIndirect and DispatchIndirect CPU recording.
set -euo pipefail
umask 077
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
if [[ $# -ne 0 ]]; then printf 'This trial takes no arguments.\n' >&2; exit 24; fi
mkdir -p "$trial_root/work/copy-argument-trials"
copy_trial=$(mktemp -d "$trial_root/work/copy-argument-trials/trial-XXXXXX")
# DeferredStreamCopies (16384) remains disabled; add PipelinedIndirectDraws (1<<46).
printf '70368744194048 0\n' > "$copy_trial/toggles.txt"
chmod 400 "$copy_trial/toggles.txt"
export BB_TOGGLE_FILE="$copy_trial/toggles.txt"
export SOTC_INDIRECT_PREP_TRACE=1 SOTC_INDIRECT_COPY_TRACE=1 SOTC_COMPUTE_BIND_TRACE=0
printf 'Indirect ordering trial: sync copies; indirect draw/dispatch CPU pipeline disabled.\n'
printf 'Confirm runtime disabled optimizations mask=70368744194048; experiments=0; file=%s\n' "$BB_TOGGLE_FILE"
exec bash "$trial_root/prueba_memoria.command" memory-render
