#!/bin/bash
# Isolated build of the pinned bloodborne-native / bbport / shadPS4 GPU library.
set -euo pipefail
sotc_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
exec python3 "$sotc_root/tools/build_gpu.py" "$@"
