#!/bin/bash
set -euo pipefail
umask 077
cd -- "$(dirname -- "$0")"
sotc_root="$PWD"
export SOTC_GAME_ROOT="${1:-${SOTC_GAME_ROOT:-$sotc_root/game/CUSA08809/SotC-PC/Game/CUSA08809}}"
sotc_modules="${SOTC_SYSTEM_MODULE_ROOT:-$SOTC_GAME_ROOT/../sys_modules}"
[[ -f work/sotc-arm64 && -f work/gpu-build-73672843f513/libbbgpu.dylib ]] || { echo 'Run bash build.sh first.' >&2; exit 1; }
python3 tools/prepare_game.py
python3 tools/prepare_linked_arm64.py --system-gesture-module "$sotc_modules/libSceSystemGesture.sprx" --ngs2-module "$sotc_modules/libSceNgs2.sprx" --out work/sotc-graphics-arm64
mkdir -p work logs
sotc_state=$(mktemp -d "$sotc_root/work/game-state-XXXXXX")
source tools/runtime-settings.sh
export SOTC_DIAGNOSTIC_SECONDS="${SOTC_RUN_SECONDS:-180}" SOTC_GPU_UTILIZATION=1
exec python3 tools/run_gpu_diagnostic.py --seconds "$SOTC_DIAGNOSTIC_SECONDS" --log "$sotc_root/logs/game-$(date -u +%Y%m%dT%H%M%SZ).log" --cooperative-stop -- "$sotc_root/work/sotc-arm64" "$sotc_root/work/sotc-graphics-arm64/boot-linked.bin" --bootstrap --game-root "$SOTC_GAME_ROOT" --graphics-library "$sotc_root/work/gpu-build-73672843f513/libbbgpu.dylib"
