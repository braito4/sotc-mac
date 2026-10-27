#!/bin/bash
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export SOTC_UPLOAD_BATCH_TRIAL=1
exec bash "$trial_root/prueba_cadena_buffers.command"
