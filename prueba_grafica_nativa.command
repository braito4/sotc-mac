#!/bin/bash
# Owner-supervised original-resolution comparison, using the existing guarded launcher.
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
# Override inherited low-resolution presets; do not use Bloodborne's guest resolution patch.
unset BB_RENDER_RES BB_OUTPUT_RES
export BB_UPSCALE_PRESET=0 BB_UPSCALER=none
export BB_FULLSCREEN=0
export BB_DIRECT_MEMORY_ACCESS=0 BB_ASYNC_SUBMIT=0 BB_JIT=1 BB_FRAME_STATS=1
exec bash "$trial_root/packaging/test-arm64.sh" --graphics-game-test
