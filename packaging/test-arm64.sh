#!/bin/bash
# Native ARM64 SotC diagnostics; graphics and game execution require explicit modes.
set -euo pipefail
trial_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
runtime_repo="${SOTC_RUNTIME_SOURCE:-$trial_root/work/gpu-source-a27f0ca477bd}"
cpu_repo="$runtime_repo/recomp"
reference_repo="$trial_root/work/sotc-reference-75b48137bc33"
trial_deps="${BB_DEPS:-$trial_root/work/deps-arm64}"
trial_mode="${1:---check}"
trial_game="${SOTC_GAME_ROOT:-$trial_root/game/CUSA08809/SotC-PC/Game/CUSA08809}"
fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
need() { command -v "$1" >/dev/null 2>&1 || fail "Missing tool: $1"; }
require_arm64() {
    [[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]] || fail 'Run in a native ARM64 macOS Terminal.'
}
configure_gpu_driver() {
    # Native Metal now boots without a Vulkan loader/ICD. Keep the old bridge
    # available explicitly for comparison and the dedicated Vulkan check.
    if [[ "$trial_mode" != --vulkan-check && "${BB_METAL_NATIVE_BOOT:-1}" == 1 ]]; then
        # Native startup has no VMA allocator: resources must use Metal too.
        export BB_METAL_FRAME=1 BB_METAL_NATIVE_BOOT=1 BB_GFX_BACKEND=metal
        unset VK_DRIVER_FILES VK_ICD_FILENAMES SDL_VULKAN_LIBRARY
        printf 'Renderer startup: native Metal adapter; no Vulkan loader/ICD.\n'
        return
    fi
    export BB_METAL_NATIVE_BOOT=0
    trial_gpu_prefix="$trial_root/work/gpu-source-a27f0ca477bd/deps/macos-arm64"
    [[ -f "$trial_gpu_prefix/lib/libvulkan.1.dylib" && -f "$trial_gpu_prefix/lib/libMoltenVK.dylib" &&
       -f "$trial_gpu_prefix/share/vulkan/icd.d/MoltenVK_icd.json" ]] || fail 'Build the isolated GPU dependencies first.'
    export VK_DRIVER_FILES="$trial_gpu_prefix/share/vulkan/icd.d/MoltenVK_icd.json"
    export VK_ICD_FILENAMES="$VK_DRIVER_FILES"
    export SDL_VULKAN_LIBRARY="$trial_gpu_prefix/lib/libvulkan.1.dylib"
    python3 "$trial_root/tools/check_vulkan.py" --library "$SDL_VULKAN_LIBRARY"
}
test_emitter() {
    [[ -f "$cpu_repo/tests/test_a64.c" ]] || fail 'Initialize the recomp submodule first.'
    mkdir -p "$cpu_repo/out"
    clang -arch arm64 -O2 -std=gnu11 "$cpu_repo/tests/test_a64.c" -o "$cpu_repo/out/test_a64"
    [[ "$(lipo -archs "$cpu_repo/out/test_a64")" == arm64 ]] || fail 'Emitter test is not ARM64.'
    "$cpu_repo/out/test_a64"
}
build_diagnostic() {
    [[ -f "$cpu_repo/out/libbbcpu.a" ]] || fail 'Build bbcpu first with --test.'
    [[ -f "$trial_deps/lib/libZydis.a" && -f "$trial_deps/lib/libZycore.a" ]] || fail "Build Zydis 4.1.1 and Zycore in $trial_deps or set BB_DEPS. See docs/guide/building.md."
    mkdir -p "$trial_root/work" "$trial_root/logs"
    trial_flags=(-arch arm64 -O2 -std=gnu11 -Wall -Wextra -Werror -pthread
        -I"$trial_deps/include" -I"$cpu_repo/src" -I"$cpu_repo/src/cpu" -I"$runtime_repo/src"
        -I"$trial_root/runtime/src" -I"$trial_root/tests" -I"$trial_root/work")
    cp "$cpu_repo/out/libbbcpu.a" "$trial_root/work/libbbcpu-context.a"
    trial_archive_members=$(ar -t "$trial_root/work/libbbcpu-context.a") || fail 'Cannot read bbcpu archive.'
    for trial_member in cpu.c.o interp_vec.c.o interp.c.o jit_arm64.c.o; do
        case $'\n'"$trial_archive_members"$'\n' in
            *$'\n'"$trial_member"$'\n'*) ;;
            *) fail 'Unexpected bbcpu archive layout.' ;;
        esac
    done
    ar -d "$trial_root/work/libbbcpu-context.a" cpu.c.o interp_vec.c.o interp.c.o jit_arm64.c.o
    python3 "$trial_root/tools/prepare_cpu_scalar_support.py" "$cpu_repo/src/cpu/interp.c" "$trial_root/work/interp_scalar_diagnostic.c"
    clang "${trial_flags[@]}" -c "$trial_root/work/interp_scalar_diagnostic.c" -o "$trial_root/work/interp_scalar_diagnostic.o"
    ar -r "$trial_root/work/libbbcpu-context.a" "$trial_root/work/interp_scalar_diagnostic.o"
    python3 "$trial_root/tools/prepare_cpu_probe_support.py" "$cpu_repo/src/cpu/jit_arm64.c" "$trial_root/work/jit_arm64_probe_diagnostic.c"
    clang "${trial_flags[@]}" -c "$trial_root/work/jit_arm64_probe_diagnostic.c" -o "$trial_root/work/jit_arm64_probe_diagnostic.o"
    ar -r "$trial_root/work/libbbcpu-context.a" "$trial_root/work/jit_arm64_probe_diagnostic.o"
    python3 "$trial_root/tools/prepare_runtime_support.py" "$runtime_repo/src/runtime_mutex.c" "$trial_root/work/runtime_mutex_diagnostic.c"
    python3 "$trial_root/tools/prepare_memory_support.py" "$runtime_repo/src/runtime_memory.c" "$trial_root/work/runtime_memory_diagnostic.c"
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
        "$trial_root/work/runtime_mutex_diagnostic.c" "$runtime_repo/src/runtime_rwlock.c")
    trial_libraries=("$trial_root/work/libbbcpu-context.a" "$trial_deps/lib/libZydis.a" "$trial_deps/lib/libZycore.a")
    trial_sources=("${trial_support[@]}" "$trial_root/runtime/src/runtime_memory_arm64.c" "${trial_libraries[@]}")
    clang "${trial_flags[@]}" "$trial_root/runtime/src/probe_entry_arm64.c" \
        "${trial_sources[@]}" -Wl,-export_dynamic -o "$trial_root/work/probe_entry_arm64"
    [[ "$(lipo -archs "$trial_root/work/probe_entry_arm64")" == arm64 ]] || fail 'Diagnostic is not ARM64.'
}
build_cpu_strings() {
    clang "${trial_flags[@]}" "$trial_root/tests/test_cpu_strings.c" "${trial_sources[@]}" -o "$trial_root/work/test_cpu_strings"
    clang "${trial_flags[@]}" "$trial_root/tests/test_cpu_sse4a.c" "${trial_sources[@]}" -o "$trial_root/work/test_cpu_sse4a"
    clang "${trial_flags[@]}" "$trial_root/tests/test_cpu_widen.c" "${trial_sources[@]}" -o "$trial_root/work/test_cpu_widen"
    clang "${trial_flags[@]}" "$trial_root/tests/test_cpu_indexed.c" "${trial_sources[@]}" -o "$trial_root/work/test_cpu_indexed"
    clang "${trial_flags[@]}" "$trial_root/tests/test_cpu_zero_shift.c" "${trial_sources[@]}" -o "$trial_root/work/test_cpu_zero_shift"
    clang "${trial_flags[@]}" "$trial_root/tests/test_cpu_xadd.c" "${trial_sources[@]}" -o "$trial_root/work/test_cpu_xadd"
}
test_cpu_sse4a() {
    BB_JIT="$1" "$trial_root/work/test_cpu_sse4a"
    BB_JIT="$1" "$trial_root/work/test_cpu_widen"
    BB_JIT="$1" "$trial_root/work/test_cpu_indexed"
    BB_JIT="$1" "$trial_root/work/test_cpu_zero_shift"
    BB_JIT="$1" "$trial_root/work/test_cpu_xadd"
}
contracts() {
    python3 -m unittest discover -s "$trial_root/tests" -p test_shader_copy_probes.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_shader_watch_history.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_shader_watch_summary.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_shader_consume.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_shader_backing.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_sigbus_index_probes.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_shader_bind.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_startup_no_intro.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_content_audit.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_gpu_build.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_visual_trace.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_memory_trace.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_pool_replay.py
    python3 "$trial_root/tools/check_audio_quantum.py"
    python3 -m unittest discover -s "$trial_root/tests" -p test_shader_metadata.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_prepare_memory.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_gpu_link.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_gpu_load.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_gpu_supervisor.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_gpu_submission_trace.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_diagnostic_budget.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_cpu_fault_diagnostics.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_cpu_field_trace.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_cpu_probe_boundaries.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_cpu_shader_write_trace.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_cpu_interp_region.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_cpu_experiment.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_index_capture.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_cpu_field_analysis.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_host_fault_analysis.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_diagnostic_run.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_terminal_memory_sources.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_shader_submit_analysis.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_diagnostic_journal.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_stack_failure.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_pointer_trace.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_pointer_release.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_pointer_entries.py
    python3 "$trial_root/tools/check_task_guard.py"
    python3 -m unittest discover -s "$trial_root/tests" -p test_graphics_entry.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_prepare_linked.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_linked_modules.py
    python3 -m unittest discover -s "$trial_root/tests" -p test_vulkan.py
    clang "${trial_flags[@]}" "$trial_root/tests/test_memory_backing.c" \
        "$trial_root/runtime/src/platform_arm64.c" -o "$trial_root/work/test_memory_backing"
    "$trial_root/work/test_memory_backing"
    clang -target x86_64-linux-gnu -O1 -fno-stack-protector -fno-asynchronous-unwind-tables -fno-pic \
        -c "$trial_root/tests/fiber_guest.c" -o "$trial_root/work/fiber_guest.o"
    python3 "$trial_root/tools/prepare_fiber_fixture.py" "$trial_root/work/fiber_guest.o" "$trial_root/work/fiber_fixture_code.h"
    clang "${trial_flags[@]}" "$trial_root/tests/test_fibers.c" "${trial_sources[@]}" -o "$trial_root/work/test_fibers"
    python3 -m unittest discover -s "$trial_root/tests" -p test_fiber_failures.py
    clang -target x86_64-linux-gnu -O1 -mno-sse -mno-sse2 -fno-stack-protector -fno-asynchronous-unwind-tables -fno-pic \
        -c "$trial_root/tests/thread_guest.c" -o "$trial_root/work/thread_guest.o"
    python3 "$trial_root/tools/prepare_fiber_fixture.py" "$trial_root/work/thread_guest.o" \
        "$trial_root/work/thread_fixture_code.h" thread_fixture_code
    clang "${trial_flags[@]}" "$trial_root/tests/test_threads.c" "${trial_sources[@]}" -o "$trial_root/work/test_threads"
    clang "${trial_flags[@]}" "$trial_root/tests/test_keys.c" "${trial_sources[@]}" -o "$trial_root/work/test_keys"
    clang "${trial_flags[@]}" "$trial_root/tests/test_modules.c" "${trial_sources[@]}" -o "$trial_root/work/test_modules"
    clang "${trial_flags[@]}" "$trial_root/tests/test_users.c" "${trial_sources[@]}" -o "$trial_root/work/test_users"
    build_cpu_strings
    clang "${trial_flags[@]}" "$trial_root/tests/test_memory_query.c" \
        "${trial_support[@]}" "${trial_libraries[@]}" -o "$trial_root/work/test_memory_query"
    BB_DMEM_MB=5056 BB_DMEM_EXTRA_MB=512 "$trial_root/work/test_memory_query"
    clang "${trial_flags[@]}" "$trial_root/tests/test_memory_hooks.c" \
        "${trial_support[@]}" "${trial_libraries[@]}" -o "$trial_root/work/test_memory_hooks"
    for trial_jit in 1 0; do
        BB_JIT="$trial_jit" SOTC_MEMORY_BACKING=file "$trial_root/work/test_memory_hooks"
    done
    for trial_failure in allocation capacity; do
        trial_log="$trial_root/work/memory-hook-$trial_failure.log"
        if "$trial_root/work/test_memory_hooks" "--$trial_failure-failure" > "$trial_log" 2>&1; then
            fail "Expected explicit GPU notification $trial_failure failure"
        else
            trial_status=$?
            [[ "$trial_status" == 21 ]] || fail "Unexpected GPU notification failure: $trial_status"
        fi
        cat "$trial_log"
    done
    clang "${trial_flags[@]}" "$trial_root/tests/test_bootstrap.c" \
        "$trial_root/tests/test_clocks.c" "$trial_root/tests/test_files.c" "$trial_root/tests/test_rwlocks.c" \
        "$trial_root/tests/test_event_flags.c" "$trial_root/tests/test_equeues.c" \
        "$trial_root/tests/test_uuid.c" \
        "${trial_sources[@]}" -o "$trial_root/work/test_bootstrap"
    for trial_failure in restart callback-null callback-count; do
        trial_log="$trial_prefix-$trial_failure.log"
        if "$trial_root/work/test_bootstrap" "--$trial_failure-check" > "$trial_log" 2>&1; then
            fail "Expected explicit rejection for $trial_failure"
        else
            trial_status=$?
            [[ "$trial_status" == 27 ]] || fail "Unexpected $trial_failure exit: $trial_status"
        fi
        cat "$trial_log"
    done
    clang "${trial_flags[@]}" "$trial_root/tests/test_rtc.c" "${trial_sources[@]}" -o "$trial_root/work/test_rtc"
    clang "${trial_flags[@]}" "$trial_root/tests/test_graphics_draw.c" "${trial_sources[@]}" -o "$trial_root/work/test_graphics_draw"
    clang "${trial_flags[@]}" "$trial_root/tests/test_graphics_present.c" "${trial_sources[@]}" -o "$trial_root/work/test_graphics_present"
    clang "${trial_flags[@]}" "$trial_root/tests/test_graphics_submit.c" "${trial_sources[@]}" -o "$trial_root/work/test_graphics_submit"
    clang "${trial_flags[@]}" "$trial_root/tests/test_graphics_shaders.c" "${trial_sources[@]}" -o "$trial_root/work/test_graphics_shaders"
    clang "${trial_flags[@]}" "$trial_root/tests/test_graphics_routes.c" "${trial_sources[@]}" -o "$trial_root/work/test_graphics_routes"
    clang "${trial_flags[@]}" "$trial_root/tests/test_pad.c" "${trial_sources[@]}" -o "$trial_root/work/test_pad"
    clang "${trial_flags[@]}" "$trial_root/tests/test_gamepad.c" "${trial_sources[@]}" -o "$trial_root/work/test_gamepad"
    clang "${trial_flags[@]}" "$trial_root/tests/test_audio.c" "${trial_sources[@]}" -o "$trial_root/work/test_audio"
    clang "${trial_flags[@]}" "$trial_root/tests/test_audio_output.c" "${trial_sources[@]}" -o "$trial_root/work/test_audio_output"
    python3 -m unittest discover -s "$trial_root/tests" -p test_audio_trace.py
    clang "${trial_flags[@]}" "$trial_root/tests/test_graphics_color.c" "${trial_sources[@]}" -o "$trial_root/work/test_graphics_color"
    for trial_color_case in unequal reserved size; do
        trial_log="$trial_prefix-color-$trial_color_case.log"
        if "$trial_root/work/test_graphics_color" "--$trial_color_case" > "$trial_log" 2>&1; then
            fail "Expected unsupported color rejection for $trial_color_case"
        else
            trial_status=$?
            [[ "$trial_status" == 29 ]] || fail "Unexpected unsupported color exit: $trial_status"
        fi
        cat "$trial_log"
    done
    trial_contract_arguments=("$trial_root/work/test_bootstrap")
    [[ "$trial_mode" != --linked-test ]] || trial_contract_arguments+=(--direct-memory)
    for trial_jit in 1 0; do
        BB_JIT="$trial_jit" "$trial_root/work/test_graphics_routes"
        BB_JIT="$trial_jit" "$trial_root/work/test_graphics_shaders"
        BB_JIT="$trial_jit" "$trial_root/work/test_graphics_submit"
        BB_JIT="$trial_jit" "$trial_root/work/test_graphics_present"
        BB_JIT="$trial_jit" "$trial_root/work/test_graphics_draw"
        BB_JIT="$trial_jit" "$trial_root/work/test_rtc"
        BB_JIT="$trial_jit" "$trial_root/work/test_audio"
        BB_JIT="$trial_jit" "$trial_root/work/test_audio" --unavailable
        BB_JIT="$trial_jit" "$trial_root/work/test_audio" --not-ready
        BB_JIT="$trial_jit" "$trial_root/work/test_audio" --missing-callback
        BB_JIT="$trial_jit" "$trial_root/work/test_audio" --missing-query
        BB_JIT="$trial_jit" "$trial_root/work/test_audio_output"
        BB_JIT="$trial_jit" "$trial_root/work/test_audio_output" --missing-output
        trial_log="$trial_prefix-key-exit-jit$trial_jit.log"
        if BB_JIT="$trial_jit" "$trial_root/work/test_threads" --recursive-key-exit-check > "$trial_log" 2>&1; then
            fail 'Expected explicit rejection of recursive thread exit from a key destructor'
        else
            trial_status=$?
            [[ "$trial_status" -eq 23 ]] || { cat "$trial_log"; fail 'Unexpected recursive key-exit result'; }
            /usr/bin/grep -Fq 'recursive thread exit from a key destructor' "$trial_log" || { cat "$trial_log"; fail 'Missing key-exit rejection'; }
        fi
        BB_JIT="$trial_jit" "$trial_root/work/test_keys"
        BB_JIT="$trial_jit" "$trial_root/work/test_pad"
        BB_JIT="$trial_jit" "$trial_root/work/test_gamepad"
        test_cpu_sse4a "$trial_jit"
        BB_JIT="$trial_jit" "$trial_root/work/test_graphics_color"
        trial_log="$trial_prefix-cpu-strings-jit$trial_jit.log"
        if BB_JIT="$trial_jit" "$trial_root/work/test_cpu_strings" > "$trial_log" 2>&1; then
            cat "$trial_log"
        else
            trial_status=$?; cat "$trial_log"; exit "$trial_status"
        fi
        trial_log="$trial_prefix-fibers-jit$trial_jit.log"
        if BB_JIT="$trial_jit" "$trial_root/work/test_fibers" > "$trial_log" 2>&1; then
            cat "$trial_log"
        else
            trial_status=$?; cat "$trial_log"; exit "$trial_status"
        fi
        trial_log="$trial_prefix-modules-jit$trial_jit.log"
        if BB_JIT="$trial_jit" "$trial_root/work/test_modules" > "$trial_log" 2>&1; then
            cat "$trial_log"
        else
            trial_status=$?; cat "$trial_log"; exit "$trial_status"
        fi
        trial_log="$trial_prefix-users-jit$trial_jit.log"
        if BB_JIT="$trial_jit" "$trial_root/work/test_users" > "$trial_log" 2>&1; then
            cat "$trial_log"
        else
            trial_status=$?; cat "$trial_log"; exit "$trial_status"
        fi
        trial_log="$trial_prefix-threads-jit$trial_jit.log"
        if BB_JIT="$trial_jit" "$trial_root/work/test_threads" > "$trial_log" 2>&1; then
            cat "$trial_log"
        else
            trial_status=$?; cat "$trial_log"; exit "$trial_status"
        fi
        trial_log="$trial_prefix-contracts-jit$trial_jit.log"
        if BB_JIT="$trial_jit" BB_DMEM_MB=5056 BB_DMEM_EXTRA_MB=0 \
            "${trial_contract_arguments[@]}" > "$trial_log" 2>&1; then
            cat "$trial_log"
        else
            trial_status=$?; cat "$trial_log"; exit "$trial_status"
        fi
    done
}
require_arm64
# Reject the withdrawn setting before Vulkan discovery, the Metal precheck or
# any library loading. The native loader independently checks direct invocation.
case "$trial_mode" in
    --gpu-load-check|--renderer-check|--graphics-game-test)
        [[ "${BB_DIRECT_MEMORY_ACCESS-0}" == 0 ]] ||
            fail 'STOP_GPU_CONFIG: BB_DIRECT_MEMORY_ACCESS must be unset or 0; dynamic shader reads are suspended after a host watchdog panic'
        ;;
esac
case "$trial_mode" in
    --check)
        printf 'System: %s / %s\n' "$(uname -s)" "$(uname -m)"
        for trial_tool in git clang cmake ninja python3; do
            if command -v "$trial_tool" >/dev/null 2>&1; then
                printf 'Tool: %s\n' "$(command -v "$trial_tool")"
            else printf 'MISSING: %s\n' "$trial_tool"; fi
        done
        for trial_source in "$runtime_repo" "$cpu_repo" "$reference_repo"; do
            [[ -f "$trial_source/README.md" ]] && printf 'Source: %s\n' "$trial_source" || printf 'MISSING source: %s\n' "$trial_source"
        done
        [[ ! -f "$trial_game/eboot.bin" ]] || printf 'Local game executable: %s/eboot.bin\n' "$trial_game"
        printf 'See docs/guide/building.md. The diagnostic does not start gameplay.\n'
        ;;
    --fetch)
        need git
        # Historical submodules supply the before-fix regression fixtures.
        git -C "$trial_root" submodule update --init runtime/upstream reference
        git -C "$trial_root/runtime/upstream" submodule update --init recomp
        python3 "$trial_root/tools/build_gpu.py" --fetch
        printf 'Pinned CPU sources initialized; no game data downloaded.\n'
        ;;
    --test-emitter) need clang; test_emitter ;;
    --test)
        for trial_tool in clang cmake ninja python3; do need "$trial_tool"; done
        [[ -f "$cpu_repo/build.sh" ]] || fail 'Initialize upstream sources with --fetch.'
        mkdir -p "$trial_root/work" "$trial_root/logs"
        # Upstream build.sh installs its pinned Zydis dependency when absent.
        (cd "$cpu_repo"; BB_DEPS="$trial_deps" bash build.sh)
        [[ "$(lipo -archs "$cpu_repo/out/libbbcpu.a")" == arm64 ]] || fail 'bbcpu is not ARM64.'
        test_emitter
        build_diagnostic
        clang "${trial_flags[@]}" "$cpu_repo/tests/test_jit_fault.c" \
            "$trial_root/runtime/src/bootstrap_cpu_context.c" "$trial_root/runtime/src/bootstrap_cpu_vector.c" \
            "${trial_libraries[@]}" -o "$trial_root/work/test_jit_fault"
        "$trial_root/work/test_jit_fault"
        build_cpu_strings
        for trial_jit in 1 0; do
            BB_JIT="$trial_jit" "$trial_root/work/test_cpu_strings"
            test_cpu_sse4a "$trial_jit"
        done
        printf 'Native emitter and diagnostic JIT fault checks passed. The upstream Rosetta comparison suite was not run.\n'
        ;;
    --graphics-check)
        need clang; need python3
        mkdir -p "$trial_root/work" "$trial_root/logs"
        clang -arch arm64 -O2 -Wall -Wextra -Werror -fobjc-arc -framework Foundation -framework Metal \
            "$trial_root/tools/check_graphics.m" -o "$trial_root/work/check_graphics"
        python3 "$trial_root/tools/run_gpu_diagnostic.py" --seconds 10 --cooperative-stop \
            --log "$trial_root/logs/metal-smoke-$(date -u +%Y%m%dT%H%M%SZ).log" \
            -- "$trial_root/work/check_graphics"
        ;;
    --vulkan-check)
        need python3
        configure_gpu_driver
        ;;
    --gpu-load-check|--renderer-check)
        need clang; need python3
        if [[ "$trial_mode" == --renderer-check ]]; then
            configure_gpu_driver
            bash "$trial_root/packaging/test-arm64.sh" --graphics-check
        fi
        build_diagnostic
        python3 "$trial_root/tools/check_gpu_link.py"
        trial_gpu_state=$(mktemp -d "$trial_root/work/gpu-load-state-XXXXXX")
        if [[ "$trial_mode" == --renderer-check ]]; then
            BB_GPU_USER_DIR="$trial_gpu_state" BB_CONFIG="$trial_gpu_state/bbport.ini" \
                BB_GAME_PROFILE=none BB_SETTINGS_CHANNEL=0 BB_METAL_FRAME=1 \
                "$trial_root/work/probe_entry_arm64" --renderer-check "$trial_root/work/gpu-build-a27f0ca477bd/libbbgpu.dylib"
        else
            BB_GPU_USER_DIR="$trial_gpu_state" "$trial_root/work/probe_entry_arm64" --gpu-load-check "$trial_root/work/gpu-build-a27f0ca477bd/libbbgpu.dylib"
        fi
        ;;
    --graphics-game-test)
        # Both .command entry points and direct invocations share this path.
        # Private field selections are data, never sourced code. Explicit empty disables.
        if [[ -z ${SOTC_CPU_FAULT_FIELDS+x} && -f "$trial_root/work/cpu-fault-fields.txt" ]]; then
            SOTC_CPU_FAULT_FIELDS="$(< "$trial_root/work/cpu-fault-fields.txt")"
            export SOTC_CPU_FAULT_FIELDS
        fi
        if [[ -z ${SOTC_CPU_FIELD_TRACE+x} && -f "$trial_root/work/cpu-field-trace.txt" ]]; then
            SOTC_CPU_FIELD_TRACE="$(< "$trial_root/work/cpu-field-trace.txt")"
            export SOTC_CPU_FIELD_TRACE
        fi
        # Native scene resolution by default. Explicit comparison overrides remain supported.
        export BB_UPSCALE_PRESET="${BB_UPSCALE_PRESET-0}"
        export BB_UPSCALER="${BB_UPSCALER-none}"
        # Windowed by default; no Game Mode app bundle or forced fullscreen.
        export BB_FULLSCREEN="${BB_FULLSCREEN-0}"
        need clang; need python3
        configure_gpu_driver
        bash "$trial_root/packaging/test-arm64.sh" --graphics-check
        build_diagnostic
        python3 "$trial_root/tools/check_gpu_link.py"
        trial_system_modules="${SOTC_SYSTEM_MODULE_ROOT:-$trial_game/../sys_modules}"
        SOTC_GAME_ROOT="$trial_game" python3 "$trial_root/tools/prepare_linked_arm64.py" \
            --system-gesture-module "$trial_system_modules/libSceSystemGesture.sprx" \
            --ngs2-module "$trial_system_modules/libSceNgs2.sprx" \
            --out "$trial_root/work/sotc-graphics-arm64"
        trial_gpu_state=$(mktemp -d "$trial_root/work/gpu-game-state-XXXXXX")
        unset SOTC_SKIP_INTRO_SITE
        if [[ ${SOTC_SKIP_INTRO:-0} == 1 ]]; then
            trial_startup_site=$(python3 "$trial_root/tools/check_startup_profile.py" \
                --image "$trial_root/work/sotc-graphics-arm64/boot-linked.bin" \
                --profile "${SOTC_STARTUP_PROFILE:?Missing private startup profile}" \
                --output "$trial_gpu_state/startup-profile-check.json" --print-site)
            export SOTC_SKIP_INTRO_SITE="$trial_startup_site"
        fi
        if [[ -n ${SOTC_CPU_POINTER_PROFILE:-} ]]; then
            python3 "$trial_root/tools/check_pointer_profile.py" \
                --image "$trial_root/work/sotc-graphics-arm64/boot-linked.bin" \
                --profile "$SOTC_CPU_POINTER_PROFILE" --selection "${SOTC_CPU_POINTER_TRACE:-}" \
                --field-selection "${SOTC_CPU_FIELD_TRACE:-}" \
                --output "$trial_gpu_state/pointer-profile-check.json"
        fi
        if [[ -n ${SOTC_CPU_EXPERIMENT_PROFILE:-} ]]; then
            python3 "$trial_root/tools/check_cpu_experiment.py" \
                --profile "$SOTC_CPU_EXPERIMENT_PROFILE" --variant "${SOTC_CPU_EXPERIMENT:-base}" \
                --range "${SOTC_CPU_INTERPRET_RANGE:-}" \
                --image "$trial_root/work/sotc-graphics-arm64/boot-linked.bin" \
                --output "$trial_gpu_state/cpu-experiment-check.json"
        fi
        trial_log="$trial_root/logs/graphics-game-$(date -u +%Y%m%dT%H%M%SZ).log"
        # Reuse the pinned renderer's host-side flip statistics, only on request.
        # Keep each default CSV private and separate from previous comparisons.
        if [[ "${BB_FRAME_STATS:-}" == 1 ]]; then
            export BB_FRAME_LOG="${BB_FRAME_LOG-$trial_gpu_state/frames.csv}"
            if [[ -n "$BB_FRAME_LOG" ]]; then
                printf 'GPU flip statistics: %s (line-buffered when visual tracing is enabled).\n' "$BB_FRAME_LOG"
            fi
        fi
        printf 'Experimental graphics initialization; no game frame established. Log: %s\n' "$trial_log"
        if BB_GPU_USER_DIR="$trial_gpu_state" BB_CONFIG="$trial_gpu_state/bbport.ini" \
            BB_GAME_PROFILE=none BB_SETTINGS_CHANNEL=0 BB_METAL_FRAME=1 \
            SOTC_GPU_SUBMIT_TRACE="${SOTC_GPU_SUBMIT_TRACE:-1}" \
            SOTC_VISUAL_TRACE="${SOTC_VISUAL_TRACE:-1}" SOTC_VISUAL_CAPTURE="${SOTC_VISUAL_CAPTURE:-0}" \
            SOTC_AUDIO_TRACE="${SOTC_AUDIO_TRACE:-1}" \
            SOTC_MEMORY_TRACE="${SOTC_MEMORY_TRACE:-1}" \
            SOTC_DIAGNOSTIC_SECONDS="${SOTC_DIAGNOSTIC_SECONDS-0}" \
            BB_JIT="${BB_JIT:-1}" BB_DMEM_MB=5056 BB_DMEM_EXTRA_MB="${BB_DMEM_EXTRA_MB:-512}" \
            python3 "$trial_root/tools/run_gpu_diagnostic.py" \
            --seconds "${SOTC_DIAGNOSTIC_SECONDS-0}" --log "$trial_log" --cooperative-stop \
            -- "$trial_root/work/probe_entry_arm64" "$trial_root/work/sotc-graphics-arm64/boot-linked.bin" \
            --bootstrap --game-root "$trial_game" --graphics-library "$trial_root/work/gpu-build-a27f0ca477bd/libbbgpu.dylib"
        then trial_status=0; else trial_status=$?; fi
        python3 "$trial_root/tools/analyze_diagnostic_run.py" --log "$trial_log" \
            --state "$trial_gpu_state" --output "$trial_gpu_state/diagnostic.json" || \
            printf 'Combined diagnostic report unavailable; original status is %s.\n' "$trial_status" >&2
        if [[ -d "$trial_gpu_state/visual" ]]; then
            python3 "$trial_root/tools/analyze_visual_trace.py" "$trial_gpu_state/visual" || \
                printf 'Visual report unavailable; original diagnostic status is %s.\n' "$trial_status" >&2
        fi
        case "$trial_status" in
            20) printf 'Stopped at an unimplemented import with the graphics provider enabled; gameplay unverified.\n' ;;
            *) exit "$trial_status" ;;
        esac
        ;;
    --content-check)
        need python3
        python3 -m unittest discover -s "$trial_root/tests" -p test_content_audit.py
        python3 "$trial_root/tools/audit_content.py" --game-root "$trial_game" \
            --report "$trial_root/work/content-audit.json"
        ;;
    --prepare)
        need python3
        [[ -f "$trial_game/eboot.bin" ]] || fail 'Set SOTC_GAME_ROOT to your own readable CUSA08809 1.01 dump.'
        SOTC_GAME_ROOT="$trial_game" python3 "$trial_root/tools/prepare_game.py"
        ;;
    --entry-test|--bootstrap-test|--linked-test|--contracts-test)
        need clang; need python3
        build_diagnostic
        trial_prefix="$trial_root/logs/${trial_mode#--}-$(date -u +%Y%m%dT%H%M%SZ)"
        [[ "$trial_mode" == --entry-test ]] || contracts
        if [[ "$trial_mode" == --contracts-test ]]; then
            printf 'Synthetic contracts passed in JIT and interpreter; no game input was used.\n'; exit 0
        fi
        trial_input="$trial_root/work/sotc-original/boot.bin"
        [[ -f "$trial_input" ]] || fail 'Prepare your game input first with --prepare.'
        trial_arguments=("$trial_input")
        [[ "$trial_mode" == --entry-test ]] || trial_arguments+=(--bootstrap)
        if [[ "$trial_mode" == --linked-test ]]; then
            SOTC_GAME_ROOT="$trial_game" python3 "$trial_root/tools/prepare_linked_arm64.py"
            trial_arguments=("$trial_root/work/sotc-linked-arm64/boot-linked.bin" --bootstrap --game-root "$trial_game")
        fi
        trial_blocked=0
        for trial_jit in 1 0; do
            trial_log="$trial_prefix-jit$trial_jit.log"
            if BB_JIT="$trial_jit" BB_DMEM_MB=5056 BB_DMEM_EXTRA_MB=0 "$trial_root/work/probe_entry_arm64" \
                "${trial_arguments[@]}" > "$trial_log" 2>&1; then trial_status=0; else trial_status=$?; fi
            cat "$trial_log"
            case "$trial_status" in
                20) ;;
                24|25) trial_blocked="$trial_status" ;;
                *) fail "Unexpected diagnostic exit: $trial_status. Log: $trial_log" ;;
            esac
        done
        python3 "$trial_root/tools/compare_stops.py" "$trial_prefix-jit1.log" "$trial_prefix-jit0.log" "$trial_mode"
        printf 'Logs: %s-jit1.log and %s-jit0.log\n' "$trial_prefix" "$trial_prefix"
        [[ "$trial_blocked" -eq 0 ]] || exit "$trial_blocked"
        ;;
    *) fail 'Modes: --check --fetch --test-emitter --test --contracts-test --graphics-check --content-check --prepare --entry-test --bootstrap-test --linked-test' ;;
esac
