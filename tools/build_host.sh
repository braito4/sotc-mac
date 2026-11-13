#!/bin/bash
# Build the native host with the selected CPU and SotC adapters.
set -euo pipefail
export MACOSX_DEPLOYMENT_TARGET=15.0
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
runtime_key="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["workspace_key"])' "$trial_root/tools/dependencies.json")"
runtime_repo="${SOTC_RUNTIME_SOURCE:-$trial_root/work/gpu-source-$runtime_key}"
cpu_repo="${SOTC_CPU_SOURCE:-$runtime_repo/recomp}"
trial_deps="${BB_DEPS:-$runtime_repo/deps/macos-arm64}"
trial_work="${SOTC_HOST_BUILD_DIR:-$trial_root/work}"
mkdir -p "$trial_work"
trial_work="$(cd -- "$trial_work" && pwd -P)"
trial_probe="$trial_work/sotc-arm64"
fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
[[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]] || fail 'Use native ARM64 macOS.'
python3 "$trial_root/tools/host_build_manifest.py" --runtime "$runtime_repo" --cpu "$cpu_repo" \
    --output "$trial_work/host-inputs.json"
BB_DEPS="$trial_deps" bash "$trial_root/tools/build_cpu.sh" "$cpu_repo"
    [[ -f "$cpu_repo/out/libbbcpu.a" ]] || fail 'Build bbcpu first with --test.'
    [[ -f "$trial_deps/lib/libZydis.a" && -f "$trial_deps/lib/libZycore.a" ]] || fail "Build Zydis 4.1.1 and Zycore in $trial_deps or set BB_DEPS. See docs/BUILD.md."
    mkdir -p "$trial_root/work" "$trial_root/logs"
    trial_flags=(-arch arm64 -O2 -std=gnu11 -Wall -Wextra -Werror -pthread
        -I"$trial_deps/include" -I"$cpu_repo/src" -I"$cpu_repo/src/cpu" -I"$runtime_repo/src" -I"$runtime_repo/gpu"
        -I"$trial_root/runtime/src" -I"$trial_work")
    case "${SOTC_POOL_PROBE:-0}" in
        0) ;;
        1) trial_flags+=(-DSOTC_POOL_PROBE=1) ;;
        *) fail 'SOTC_POOL_PROBE must be 0 or 1.' ;;
    esac
    case "${SOTC_SAVE_TRIAL:-0}" in
        0) ;;
        1) trial_flags+=(-DSOTC_SAVE_TRIAL=1) ;;
        *) fail 'SOTC_SAVE_TRIAL must be 0 or 1.' ;;
    esac
    if grep -q 'void bbcpu_apply_host_fp(' "$cpu_repo/src/cpu/cpu.c"; then
        trial_flags+=(-DSOTC_CPU_HAS_FP_CONTROL=1)
    fi
    cp "$cpu_repo/out/libbbcpu.a" "$trial_work/libbbcpu-context.a"
    trial_archive_members=$(ar -t "$trial_work/libbbcpu-context.a") || fail 'Cannot read bbcpu archive.'
    for trial_member in cpu.c.o interp_vec.c.o interp.c.o jit_arm64.c.o; do
        case $'\n'"$trial_archive_members"$'\n' in
            *$'\n'"$trial_member"$'\n'*) ;;
            *) fail 'Unexpected bbcpu archive layout.' ;;
        esac
    done
    ar -d "$trial_work/libbbcpu-context.a" cpu.c.o interp_vec.c.o interp.c.o jit_arm64.c.o
    python3 "$trial_root/tools/prepare_cpu_scalar_support.py" "$cpu_repo/src/cpu/interp.c" "$trial_work/interp_scalar_diagnostic.c"
    clang "${trial_flags[@]}" -c "$trial_work/interp_scalar_diagnostic.c" -o "$trial_work/interp_scalar_diagnostic.o"
    ar -r "$trial_work/libbbcpu-context.a" "$trial_work/interp_scalar_diagnostic.o"
    python3 "$trial_root/tools/prepare_cpu_probe_support.py" "$cpu_repo/src/cpu/jit_arm64.c" "$trial_work/jit_arm64_probe_diagnostic.c"
    clang "${trial_flags[@]}" -c "$trial_work/jit_arm64_probe_diagnostic.c" -o "$trial_work/jit_arm64_probe_diagnostic.o"
    ar -r "$trial_work/libbbcpu-context.a" "$trial_work/jit_arm64_probe_diagnostic.o"
    python3 "$trial_root/tools/prepare_runtime_support.py" "$runtime_repo/src/runtime_mutex.c" "$trial_work/runtime_mutex_diagnostic.c"
    python3 "$trial_root/tools/prepare_memory_support.py" "$runtime_repo/src/runtime_memory.c" "$trial_work/runtime_memory_diagnostic.c"
    trial_support=("$trial_root/runtime/src/bootstrap_contracts.c" "$trial_root/runtime/src/bootstrap_clocks.c"
        "$trial_root/runtime/src/bootstrap_cpu_context.c" "$trial_root/runtime/src/bootstrap_fibers.c"
        "$trial_root/runtime/src/bootstrap_cpu_vector.c"
        "$trial_root/runtime/src/bootstrap_modules.c" "$trial_root/runtime/src/bootstrap_files.c"
        "$trial_root/runtime/src/bootstrap_discmap.c"
        "$trial_root/runtime/src/bootstrap_dialogs.c"
        "$trial_root/runtime/src/bootstrap_network.c"
        "$trial_root/runtime/src/bootstrap_savedata.c"
        "$trial_root/runtime/src/bootstrap_trophy.c"
        "$trial_root/runtime/src/bootstrap_users.c"
        "$trial_root/runtime/src/bootstrap_share.c"
        "$trial_root/runtime/src/bootstrap_gnm.c"
        "$trial_root/runtime/src/bootstrap_gpu.c"
        "$trial_root/runtime/src/bootstrap_graphics_routes.c"
        "$trial_root/runtime/src/bootstrap_pad.c"
        "$trial_root/runtime/src/bootstrap_audio.c"
        "$trial_root/runtime/src/bootstrap_keys.c"
        "$trial_root/runtime/src/bootstrap_system.c"
        "$trial_root/runtime/src/bootstrap_video.c"
        "$trial_root/runtime/src/bootstrap_event_flags.c"
        "$trial_root/runtime/src/bootstrap_equeues.c"
        "$trial_root/runtime/src/bootstrap_tls.c" "$trial_root/runtime/src/bootstrap_threads.c"
        "$runtime_repo/src/runtime_file.c" "$trial_root/runtime/src/platform_arm64.c"
        "$trial_work/runtime_mutex_diagnostic.c" "$runtime_repo/src/runtime_rwlock.c")
    if [[ "${SOTC_SAVE_TRIAL:-0}" == 1 ]]; then
        trial_support+=("$trial_root/runtime/src/bootstrap_savedata_trial.c")
    fi
    trial_libraries=("$trial_work/libbbcpu-context.a" "$trial_deps/lib/libZydis.a" "$trial_deps/lib/libZycore.a")
    trial_sources=("${trial_support[@]}" "$trial_root/runtime/src/runtime_memory_arm64.c" "${trial_libraries[@]}")
    clang "${trial_flags[@]}" "$trial_root/runtime/src/probe_entry_arm64.c" \
        "${trial_sources[@]}" -Wl,-export_dynamic -o "$trial_probe.pending"
    [[ "$(lipo -archs "$trial_probe.pending")" == arm64 ]] || fail 'Diagnostic is not ARM64.'
    python3 "$trial_root/tools/host_build_manifest.py" --runtime "$runtime_repo" --cpu "$cpu_repo" \
        --captured "$trial_work/host-inputs.json" --executable "$trial_probe.pending" \
        --output "$trial_work/host-build.pending.json"
    mv "$trial_probe.pending" "$trial_probe"
    mv "$trial_work/host-build.pending.json" "$trial_work/host-build.json"
