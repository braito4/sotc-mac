"""Check actual built SRT/shader-info passes with synthetic IR and no renderer.

The installed library's existing walker still reports unsupported CPU-side Phi
and lane offsets. This diagnostic verifies that those reads remain dynamic and
select GPU page-table/fault-buffer resources when direct memory access is enabled.
It does not validate GPU execution or game pixels.
"""
from pathlib import Path
import os
import subprocess
import tempfile

from check_gpu_link import host_symbol, inspect, parse_symbols

ROOT = Path(__file__).resolve().parents[1]


def main():
    source = ROOT / 'work/gpu-source'
    prefix = source / 'deps/macos-arm64'
    build = ROOT / 'work/gpu-build'
    library = build / 'libbbgpu.dylib'
    if not library.is_file():
        raise SystemExit('Build the isolated GPU library first.')
    _, imports = parse_symbols(inspect(library))
    required = sorted(name for name in imports if host_symbol(name))
    # No host callback is needed by the valid-memory, portable shader pass fixture.
    # Unexpected calls abort. These are never successful runtime/GPU substitutes.
    definitions = ['#include <stdlib.h>', '#include <stdint.h>', '#include <setjmp.h>']
    for name in required:
        if name == 'runtime_fault_recover':
            definitions.append('_Thread_local sigjmp_buf *runtime_fault_recover;')
        elif name in ('bb_image_base', 'runtime_disabled_optimizations', 'runtime_experiment_bits'):
            definitions.append('uint64_t ' + name + ';')
        elif name == 'runtime_restarting':
            definitions.append('volatile int runtime_restarting;')
        else:
            definitions.append('void ' + name + '(void) { abort(); }')
    flags = ['-arch', 'arm64', '-mmacosx-version-min=15.0']
    output = ROOT / 'work/test_shader_memory'
    with tempfile.TemporaryDirectory(prefix='shader-memory-', dir=ROOT / 'work') as directory:
        state = Path(directory)
        host, obj = state/'host.c', state/'host.o'
        host.write_text('\n'.join(definitions) + '\n')
        subprocess.run(['clang', *flags, '-c', str(host), '-o', str(obj)], check=True)
        subprocess.run(['clang++', '-std=c++23', *flags,
                        '-I'+str(source/'gpu/shim'), '-I'+str(source/'gpu/shadps4'),
                        '-I'+str(prefix/'include'), '-I'+str(build/'include'),
                        str(ROOT/'tests/test_shader_memory.cpp'), str(obj), str(library),
                        '-L'+str(prefix/'lib'), '-lfmt', '-Wl,-rpath,'+str(build),
                        '-Wl,-export_dynamic', '-o', str(output)], check=True)
        for dma in ('0', '1'):
            subprocess.run([str(output)], check=True, env=dict(os.environ,
                           BB_GPU_USER_DIR=str(state), BB_DIRECT_MEMORY_ACCESS=dma, BB_SRT_INTERP='1'))


if __name__ == '__main__':
    main()
