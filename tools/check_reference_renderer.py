"""Exercise the linked ARM64 SRT interpreter, the SPIR-V emitter and the texture copy planner."""
import os
import re
from pathlib import Path
import subprocess
import tempfile
from build_gpu import ROOT, build_path, source_path
from check_gpu_link import host_symbol, inspect, parse_symbols


def main():
    base, build = source_path(), build_path()
    directory = build
    library = directory / 'libbbgpu.dylib'
    prefix = base / 'deps/macos-arm64'
    _, imports = parse_symbols(inspect(library))
    stubs = ['#include <stdlib.h>', '#include <stdint.h>', '#include <setjmp.h>']
    for symbol in sorted(filter(host_symbol, imports)):
        if symbol == 'runtime_memory_read_backing':
            continue  # The fixture supplies a checked synthetic table.
        if symbol == 'runtime_fault_recover':
            stubs.append('_Thread_local sigjmp_buf *runtime_fault_recover;')
        elif symbol in ('bb_image_base', 'runtime_disabled_optimizations', 'runtime_experiment_bits'):
            stubs.append('uint64_t ' + symbol + ';')
        elif symbol == 'runtime_restarting':
            stubs.append('volatile int runtime_restarting;')
        else:
            stubs.append('void ' + symbol + '(void) { abort(); }')
    with tempfile.TemporaryDirectory(dir=directory) as tmp:
        tmp = Path(tmp)
        (tmp / 'host.c').write_text('\n'.join(stubs))
        subprocess.run(['clang', '-arch', 'arm64', '-c', tmp / 'host.c', '-o', tmp / 'host.o'], check=True)
        includes = []
        includes += ['-I' + str(d) for d in (base / 'gpu/shim', base / 'gpu/shadps4',
                     prefix / 'include', build / 'include', ROOT / 'runtime/src')]
        subprocess.run(['clang++', '-std=c++23', '-arch', 'arm64', '-fsanitize=address,undefined',
                        *includes, '-include', base / 'gpu/shim/bbport_arm64_compat.h',
                        ROOT / 'tools/tests/reference_renderer.cpp', tmp / 'host.o', library,
                        '-L' + str(prefix / 'lib'), '-lfmt', '-lspirv-cross-msl', '-lspirv-cross-glsl', '-lspirv-cross-core',
                        '-Wl,-export_dynamic',
                        '-Wl,-rpath,' + str(directory), '-o', tmp / 'check'], check=True)
        module = tmp / 'window.spv'
        subprocess.run([tmp / 'check', module], check=True, timeout=30, env=dict(os.environ,
                       BB_GPU_USER_DIR=str(tmp), BB_CONFIG=str(tmp / 'bbport.ini'),
                       BB_DIRECT_MEMORY_ACCESS='0', BB_SRT_INTERP='1', BB_GAME_PROFILE='none'))
        subprocess.run(['spirv-val', '--target-env', 'spv1.6', module], check=True)
        assembly = subprocess.check_output(['spirv-dis', module], text=True)
        bound = re.search(r'^(?:\s*)(%\w+) = OpConstant %\w+ 2047$', assembly, re.MULTILINE)
        if not bound or not re.search(r' UMin %\w+ ' + re.escape(bound[1]) + r'$', assembly, re.MULTILINE):
            raise ValueError('Missing shader-side bound on the dynamic 8 KiB table: ' + '\n'.join(
                line for line in assembly.splitlines() if 'UMin' in line or '2047' in line))
    print('PASS', 'ARM64 window, guest data, bounded SPIR-V and texture gather')


if __name__ == '__main__':
    main()
