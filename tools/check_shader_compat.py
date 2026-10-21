"""Reproduce and check shader compatibility with synthetic IR, without a GPU/game.

The original wave64 pass must produce the missing declaration; the adapted pass
must preserve the two mask halves, scratch offsets/barriers and a single U32 view.
Also check the exact-only first-vertex policy against installed SPIRV-Cross.
"""
from pathlib import Path
import os
import subprocess
import tempfile
import struct

from check_gpu_link import host_symbol, inspect, parse_symbols
from replay_shader_cache import build_converter

ROOT = Path(__file__).resolve().parents[1]


def main():
    source = ROOT / 'work/gpu-source'
    prefix = source / 'deps/macos-arm64'
    build = ROOT / 'work/gpu-build'
    library = build / 'libbbgpu.dylib'
    if not library.is_file():
        raise SystemExit('Build the isolated GPU library first.')
    flags = ['-std=c++23', '-arch', 'arm64', '-mmacosx-version-min=15.0',
             '-I'+str(source/'gpu/shim'), '-I'+str(source/'gpu/shadps4'),
             '-I'+str(prefix/'include'), '-I'+str(build/'include'),
             '-I'+str(ROOT/'runtime/src')]
    cross = ['-L'+str(prefix/'lib'), '-lspirv-cross-msl', '-lspirv-cross-glsl',
             '-lspirv-cross-core', '-lfmt']
    _, imports = parse_symbols(inspect(library))
    exports = ['-Wl,-exported_symbol,_'+name for name in sorted(imports) if host_symbol(name)]
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
    with tempfile.TemporaryDirectory(prefix='shader-compat-', dir=ROOT/'work') as directory:
        work = Path(directory)
        env = dict(os.environ, BB_GPU_USER_DIR=str(work), BB_CONFIG=str(work/'bbport.ini'),
                   BB_DIRECT_MEMORY_ACCESS='0', BB_GAME_PROFILE='none')
        host = work/'host.c'; host.write_text('\n'.join(definitions)+'\n')
        subprocess.run(['clang', '-arch', 'arm64', '-mmacosx-version-min=15.0',
                        '-c', str(host), '-o', str(work/'host.o')], check=True)
        fixture = ROOT/'tests/test_wave64_shared.cpp'
        subprocess.run(['clang++', *flags, str(fixture), *cross,
                        '-o', str(work/'convert')], check=True)
        for label, tree, expected in (('pinned', ROOT/'runtime/upstream', 2), ('adapted', source, 0)):
            lower = tree/'gpu/shadps4/shader_recompiler/ir/passes/lower_wave64_pass.cpp'
            binary = work/label
            subprocess.run(['clang++', *flags, '-DSOTC_SPIRV_EMITTER',
                            '-DLowerWave64BallotPass=SotcTestLowerWave64BallotPass', str(fixture), str(lower),
                            str(work/'host.o'), str(library), '-L'+str(prefix/'lib'), '-lfmt',
                            '-Wl,-rpath,'+str(build), *exports, '-o', str(binary)], check=True)
            modules = work/(label+'-modules'); modules.mkdir()
            subprocess.run([str(binary), str(modules)], check=True, timeout=15, env=env)
            result = subprocess.run([str(work/'convert'), str(modules)], capture_output=True, text=True, timeout=15)
            print(label+': '+result.stdout.strip(), flush=True)
            if result.returncode != expected:
                raise RuntimeError(f'{label}: expected exit {expected}, got {result.returncode}: '+result.stderr)
        fixture = ROOT/'tests/test_msl_compat.cpp'
        subprocess.run(['clang++', *flags, '-DSOTC_SPIRV_EMITTER',
                        '-I'+str(source/'gpu/third_party/sirit/include'),
                        '-I'+str(source/'gpu/third_party/sirit/externals/SPIRV-Headers/include'),
                        '-c', str(fixture), '-o', str(work/'fragment.o')], check=True)
        subprocess.run(['clang++', *flags, str(fixture), str(work/'fragment.o'),
                        str(source/'gpu/shadps4/video_core/renderer_metal/mtl_spirv_pervertex.cpp'),
                        str(build/'third_party/sirit/src/libsirit.a'), *cross,
                        '-o', str(work/'fragment')], check=True)
        modules=work/'replay-modules';modules.mkdir()
        subprocess.run([str(work/'fragment'),str(modules)], check=True, timeout=15)
        replay=work/'replay';build_converter(replay)
        dependencies=subprocess.check_output(['otool','-L',str(replay)],text=True)
        assert not any(name in dependencies for name in ('/Metal.', 'MoltenVK', 'libbbgpu', '/Vulkan.'))
        for first in (False,True):
            for vertices in (0,1,2,4,3):
                result=subprocess.run([str(replay),str(modules/(str(vertices)+'.spv')),str(int(first))],
                                      capture_output=True,text=True,timeout=10)
                expected=0 if vertices==0 or (first and vertices==1) else 2
                assert result.returncode==expected,(first,vertices,result.stderr)
                assert result.stdout.splitlines()[0]==str(int(first and vertices==1))
        for words in ([0,0x10300,0,1,0], [0x07230203,0x10300,0,1,0,0],
                      [0x07230203,0x10300,0,1000001,0], [0x07230203,0x10300,0,1,0,0x00020000]):
            malformed=modules/'malformed.spv';malformed.write_bytes(struct.pack('<'+'I'*len(words),*words))
            result=subprocess.run([str(replay),str(malformed),'1'],capture_output=True,text=True,timeout=10)
            assert result.returncode==2 and ('invalid SPIR-V' in result.stderr or 'truncated SPIR-V' in result.stderr)
        print('PASS: CPU replay reads synthetic captures, preserves exact-only policy, rejects malformed modules, and has no Metal/Vulkan/renderer dependency')


if __name__ == '__main__':
    main()
