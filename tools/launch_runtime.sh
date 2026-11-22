#!/bin/bash
set -euo pipefail
umask 077
bundle="$(cd -- "$(dirname -- "$0")" && pwd -P)"
host="$bundle/bin/sotc-arm64"
renderer="$bundle/bin/libbbgpu.dylib"
state="$(mktemp -d "${TMPDIR:-/tmp}/sotc-arm64-XXXXXX")"
if [[ "${1:-}" == --check ]]; then
    BB_GPU_USER_DIR="$state" exec "$host" --gpu-load-check "$renderer"
fi
game="${1:-${SOTC_GAME_ROOT:-}}"
if [[ -z "$game" ]]; then
    for candidate in "$bundle/Game" "$bundle"/../game/CUSA08809/SotC-PC/Game/CUSA08809 "$PWD"/game/CUSA08809/SotC-PC/Game/CUSA08809; do
        if [[ -f "$candidate/eboot.bin" ]]; then game="$candidate"; break; fi
    done
fi
if [[ -z "$game" ]]; then
    game="$(/usr/bin/osascript -e 'POSIX path of (choose folder with prompt "Selecciona CUSA08809, la carpeta que contiene eboot.bin")')"
fi
[[ -f "$game/eboot.bin" ]] || { echo 'Selecciona la carpeta de tu copia descifrada de CUSA08809 (1.01).' >&2; exit 2; }
export SOTC_GAME_ROOT="$(cd -- "$game" && pwd -P)"
boot="${SOTC_LINKED_BOOT:-}"
if [[ -z "$boot" ]]; then
    modules="${2:-${SOTC_SYSTEM_MODULE_ROOT:-$SOTC_GAME_ROOT/../sys_modules}}"
    if [[ ! -f "$modules/libSceSystemGesture.sprx" || ! -f "$modules/libSceNgs2.sprx" ]]; then
        modules="$(/usr/bin/osascript -e 'POSIX path of (choose folder with prompt "Selecciona la carpeta con libSceSystemGesture.sprx y libSceNgs2.sprx")')"
    fi
    export SOTC_PREPARE_SCRIPTS="$bundle/support/preparation" SOTC_PREPARE_OUTPUT="$state/original"
    export SOTC_LINK_SCRIPTS="$bundle/support/linker" SOTC_ORIGINAL_IMAGE="$state/original/boot.bin"
    python3 "$bundle/support/tools/prepare_game.py"
    python3 "$bundle/support/tools/prepare_linked_arm64.py" --out "$state/linked" \
        --system-gesture-module "$modules/libSceSystemGesture.sprx" --ngs2-module "$modules/libSceNgs2.sprx"
    boot="$state/linked/boot-linked.bin"
fi
[[ -f "$boot" ]] || { echo 'Falta la imagen de arranque preparada.' >&2; exit 2; }
sotc_state="$state"
source "$bundle/support/runtime-settings.sh"
source "$bundle/support/reference-render-settings.sh"
export SOTC_GPU_UTILIZATION=1 BB_DRAW_PIPE=0 BB_HOIST_COPIES=0
export SOTC_TEX_GATHER="${SOTC_TEX_GATHER:-1}" BB_GC_BUDGET_CAP_MB="${BB_GC_BUDGET_CAP_MB:-5120}"
printf 'Registros de esta sesión: %s\n' "$state"
if command -v python3 >/dev/null; then
    exec python3 "$bundle/support/tools/run_gpu_diagnostic.py" --seconds 0 --cooperative-stop \
        --log "$state/game.log" -- "$host" "$boot" --bootstrap --game-root "$SOTC_GAME_ROOT" --graphics-library "$renderer"
fi
exec "$host" "$boot" --bootstrap --game-root "$SOTC_GAME_ROOT" --graphics-library "$renderer"
