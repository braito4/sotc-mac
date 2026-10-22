"""Guarded buffer-read proofs with actual IR and production pool-range selection.

Original synthetic input only. Neither a GPU nor a game module is initialized.
Unknown proofs must retain the original range and error/copy behavior.
"""
import os
from pathlib import Path
import subprocess
import tempfile

from check_buffer_copy_chunks import function
from check_gpu_link import host_symbol, inspect, parse_symbols

ROOT = Path(__file__).resolve().parents[1]


def main():
    source = ROOT / 'work/gpu-source'
    prefix = source / 'deps/macos-arm64'
    build = ROOT / 'work/gpu-build'
    library = build / 'libbbgpu.dylib'
    _, imports = parse_symbols(inspect(library))
    exports = ['-Wl,-exported_symbol,_' + name for name in sorted(imports) if host_symbol(name)]
    definitions = ['#include <stdlib.h>', '#include <stdint.h>', '#include <setjmp.h>', '#include <stdio.h>']
    for name in sorted(name for name in imports if host_symbol(name)):
        if name == 'runtime_fault_recover':
            definitions.append('_Thread_local sigjmp_buf *runtime_fault_recover;')
        elif name in ('bb_image_base', 'runtime_disabled_optimizations', 'runtime_experiment_bits'):
            definitions.append('uint64_t ' + name + ';')
        elif name == 'runtime_restarting':
            definitions.append('volatile int runtime_restarting;')
        else:
            definitions.append('void ' + name + '(void) { fputs("Unexpected host callback: '
                               + name + '\\n", stderr); abort(); }')
    with tempfile.TemporaryDirectory(prefix='buffer-read-bounds-', dir=ROOT/'work') as directory:
        work = Path(directory)
        host = work / 'host.c'
        host.write_text('\n'.join(definitions) + '\n')
        subprocess.run(['clang', '-arch', 'arm64', '-mmacosx-version-min=15.0',
                        '-c', str(host), '-o', str(work/'host.o')], check=True)
        actual = (source/'gpu/shadps4/video_core/buffer_cache/buffer_cache.cpp').read_text()
        method = function(actual, 'std::optional<std::pair<const Buffer*, u64>> BufferCache::UnifiedRange(')
        (work/'unified_range_fixture.inc').write_text(method + '\n')
        flags = ['-std=c++23', '-arch', 'arm64', '-mmacosx-version-min=15.0',
                 '-I'+str(ROOT/'runtime/src'), '-isystem', str(source/'gpu/shim'),
                 '-isystem', str(source/'gpu/shadps4'), '-isystem', str(prefix/'include'),
                 '-I'+str(work)]
        binary = work/'fixture'
        subprocess.run(['clang++', *flags, '-O1', '-Wall', '-Wextra', '-Werror',
                        str(ROOT/'tests/test_buffer_read_bounds.cpp'), str(work/'host.o'), str(library),
                        '-L'+str(prefix/'lib'), '-lfmt', '-Wl,-rpath,'+str(build), *exports,
                        '-o', str(binary)], check=True)
        env = dict(os.environ, BB_GPU_USER_DIR=str(work), BB_CONFIG=str(work/'bbport.ini'),
                   BB_DIRECT_MEMORY_ACCESS='0', BB_GAME_PROFILE='none')
        subprocess.run([str(binary)], check=True, timeout=20, env=env)


if __name__ == '__main__':
    main()
