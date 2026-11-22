# Source after runtime-settings.sh has detected the machine's memory.
if [[ "${BB_LOW_MEMORY:-0}" == 1 || "${SOTC_RAM_PROFILE:-auto}" == conservative ]]; then
    export BB_GC_BUDGET_CAP_MB="${BB_GC_BUDGET_CAP_MB:-5120}"
fi
if [[ "${SOTC_RAM_DETECTED_GIB:-0}" -ge 24 ]]; then
    export SOTC_TEX_GATHER="${SOTC_TEX_GATHER:-1}"
fi
