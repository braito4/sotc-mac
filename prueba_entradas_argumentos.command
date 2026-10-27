#!/bin/bash
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export SOTC_ARGUMENT_INPUT_SHADER=7d08eb3e
# Replace the broad writable-descriptor journal with the selected shader metadata.
export SOTC_INDIRECT_WRITER_TRACE=0
exec bash "$trial_root/prueba_indirectos_ordenados.command" "$@"
