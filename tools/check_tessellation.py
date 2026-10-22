"""Compile a synthetic interface regression against the actual built SPIR-V emitter.

Requires the isolated GPU build, but no GPU device or game. Does not load bbgpu.
"""
from pathlib import Path
from build_gpu import source_path, build_path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    build = build_path(ROOT)
    source = source_path(ROOT)
    output = ROOT / 'work/test_tessellation_locations'
    emitter = build / 'CMakeFiles/bbgpu.dir/shadps4/shader_recompiler/backend/spirv/emit_spirv_quad_rect.cpp.o'
    sirit = build / 'third_party/sirit/src/libsirit.a'
    if not emitter.is_file() or not sirit.is_file():
        raise SystemExit('Build the isolated GPU library first.')
    subprocess.run(['clang++', '-std=c++23', '-arch', 'arm64', '-mmacosx-version-min=15.0',
                    '-I'+str(source / 'gpu/shim'), '-I'+str(source / 'gpu/shadps4'),
                    '-I'+str(build / 'include'),
                    '-I'+str(source / 'deps/macos-arm64/include'),
                    str(ROOT / 'tests/test_tessellation_locations.cpp'), str(emitter), str(sirit),
                    '-L'+str(source / 'deps/macos-arm64/lib'), '-lfmt', '-o', str(output)], check=True)
    subprocess.run([str(output)], check=True)


if __name__ == '__main__':
    main()
