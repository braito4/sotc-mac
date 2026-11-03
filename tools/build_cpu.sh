#!/bin/bash
# Build only the CPU archive: recompiler tooling is an independent build.
set -euo pipefail
cpu_root="$(cd -- "${1:?CPU source directory required}" && pwd -P)"
cpu_deps="${BB_DEPS:?Set BB_DEPS to the installed ARM64 dependencies}"
[[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]] || exit 1
[[ -f "$cpu_deps/include/Zydis/Zydis.h" ]] || { echo 'Missing Zydis headers' >&2; exit 1; }
mkdir -p "$cpu_root/out/cpu"
cpu_objects=()
for cpu_source in "$cpu_root"/src/cpu/*.c "$cpu_root/src/cpu/hostcall.S"; do
    cpu_object="$cpu_root/out/cpu/$(basename "$cpu_source").o"
    clang -arch arm64 -std=gnu11 -O2 -g -Wall -Wextra -Werror -Wno-unused-parameter \
        -I"$cpu_deps/include" -c "$cpu_source" -o "$cpu_object"
    cpu_objects+=("$cpu_object")
done
# Explicit member list prevents old objects from a different revision being linked.
rm -f "$cpu_root/out/libbbcpu.a"
ar rcs "$cpu_root/out/libbbcpu.a" "${cpu_objects[@]}"
