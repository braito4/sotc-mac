# Shared macOS memory policy; works in portable bundles without Python.
# Retention threshold per staging ring, not an allocation cap or up-front reserve.
sotc_select_staging_memory() {
    local detected_bytes="$1" selected_mode="$2" ram_gib=0 keep_mb=512
    case "$detected_bytes" in ''|*[!0-9]*) detected_bytes=0 ;; esac
    # Reject implausible lengths before shell integer arithmetic.
    if [ "${#detected_bytes}" -gt 15 ]; then detected_bytes=0; fi
    ram_gib=$((10#$detected_bytes / 1073741824))
    case "$selected_mode" in
      auto)
        if [ "$ram_gib" -ge 64 ]; then keep_mb=1536
        elif [ "$ram_gib" -ge 32 ]; then keep_mb=1024
        elif [ "$ram_gib" -ge 24 ]; then keep_mb=768
        fi ;;
      conservative) ;;
      *) echo 'SOTC_RAM_PROFILE must be auto or conservative.' >&2; return 24 ;;
    esac
    export BB_STAGING_KEEP_MB="$keep_mb"
    export SOTC_RAM_DETECTED_GIB="$ram_gib" SOTC_RAM_PROFILE="$selected_mode"
}
sotc_apply_staging_memory() {
    local detected_bytes
    detected_bytes="$(/usr/sbin/sysctl -n hw.memsize 2>/dev/null)" || detected_bytes=0
    sotc_select_staging_memory "$detected_bytes" "${SOTC_RAM_PROFILE:-auto}" || return $?
    printf 'Memory profile: %s; RAM=%s GiB; staging retention=%s MiB per ring (allocated on demand).\n' "$SOTC_RAM_PROFILE" "$SOTC_RAM_DETECTED_GIB" "$BB_STAGING_KEEP_MB"
}
