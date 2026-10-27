#!/bin/bash
# A/B against selected inputs: additionally disable the existing ConstantRing optimization.
# Applies to all stages; changes snapshot timing and pending-read tracking, not just this shader.
set -euo pipefail
umask 077
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
if [[ $# -ne 0 ]]; then printf 'This trial takes no arguments.\n' >&2; exit 24; fi
mkdir -p "$trial_root/work/copy-argument-trials"
copy_trial=$(mktemp -d "$trial_root/work/copy-argument-trials/trial-XXXXXX")
# Preserve sync copies and ordered indirect commands; add ConstantRing (1<<36).
printf '70437463670784 0\n' > "$copy_trial/toggles.txt"
chmod 400 "$copy_trial/toggles.txt"
export BB_TOGGLE_FILE="$copy_trial/toggles.txt"
export SOTC_ARGUMENT_INPUT_SHADER=7d08eb3e SOTC_INDIRECT_WRITER_TRACE=0
export SOTC_INDIRECT_PREP_TRACE=1 SOTC_INDIRECT_COPY_TRACE=1 SOTC_COMPUTE_BIND_TRACE=0
printf 'Constant-ring comparison: ring disabled; sync copies and ordered indirect commands preserved.\n'
printf 'Confirm runtime disabled optimizations mask=70437463670784; experiments=0; file=%s\n' "$BB_TOGGLE_FILE"
exec bash "$trial_root/prueba_memoria.command" memory-render
