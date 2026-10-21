"""Isolate the PerVertexKHR MSL limitation without a game or GPU.

Exit 0: both synthetic interfaces convert; 2: the known unsupported raw-per-vertex
interface is reproduced; other failures are errors. Requires isolated GPU deps.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    source = ROOT / 'work/gpu-source'
    prefix = source / 'deps/macos-arm64'
    sirit = ROOT / 'work/gpu-build/third_party/sirit/src/libsirit.a'
    output = ROOT / 'work/test_fragment_interface'
    emitter = ROOT / 'work/fragment_interface_emitter.o'
    if not sirit.is_file():
        raise SystemExit('Build the isolated GPU dependencies first.')
    flags = ['clang++', '-std=c++23', '-arch', 'arm64', '-mmacosx-version-min=15.0']
    fixture = ROOT / 'tests/test_fragment_interface.cpp'
    subprocess.run([*flags, '-DSOTC_SPIRV_EMITTER',
                    '-I'+str(source / 'gpu/third_party/sirit/include'),
                    '-I'+str(source / 'gpu/third_party/sirit/externals/SPIRV-Headers/include'),
                    '-c', str(fixture), '-o', str(emitter)], check=True)
    subprocess.run([*flags,
                    '-I'+str(prefix / 'include/spirv_cross'), '-I'+str(prefix / 'include'),
                    str(fixture), str(emitter), str(sirit),
                    '-L'+str(prefix / 'lib'), '-lspirv-cross-msl', '-lspirv-cross-glsl',
                    '-lspirv-cross-core', '-lfmt', '-o', str(output)], check=True)
    return subprocess.run([str(output)], check=False).returncode


if __name__ == '__main__':
    raise SystemExit(main())
