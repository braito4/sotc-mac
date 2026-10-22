"""Run synthetic valid/reserved/truncated instructions through the installed decoder.

Guard pages distinguish explicit rejection from an out-of-bounds memory access.
No renderer initialization, GPU, game input, or game-derived instruction fixture.
"""
from pathlib import Path
from build_gpu import source_path, build_path
import os
import subprocess
import tempfile

from check_gpu_link import host_symbol, inspect, parse_symbols

ROOT = Path(__file__).resolve().parents[1]


def main():
    source = source_path(ROOT)
    prefix = source / 'deps/macos-arm64'
    build = build_path(ROOT)
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
    output = ROOT / 'work/test_shader_decode'
    with tempfile.TemporaryDirectory(prefix='shader-decode-', dir=ROOT / 'work') as directory:
        state = Path(directory)
        host, obj = state/'host.c', state/'host.o'
        host.write_text('\n'.join(definitions) + '\n')
        subprocess.run(['clang', *flags, '-c', str(host), '-o', str(obj)], check=True)
        subprocess.run(['clang++', '-std=c++23', *flags,
                        '-I'+str(ROOT/'runtime/src'), '-I'+str(source/'gpu/shim'), '-I'+str(source/'gpu/shadps4'),
                        '-I'+str(prefix/'include'), '-I'+str(build/'include'),
                        str(ROOT/'tests/test_shader_decode.cpp'), str(obj), str(library),
                        '-L'+str(prefix/'lib'), '-lfmt', '-Wl,-rpath,'+str(build),
                        '-Wl,-export_dynamic', '-o', str(output)], check=True)
        cases = {
            'valid': None,
            'read32': ('truncated_instruction', 'word_offset=1', 'required_words=1'),
            'read64': ('truncated_instruction', 'word_offset=0', 'required_words=2'),
            'peek': ('truncated_instruction', 'remaining_words=1', 'required_words=2'),
            'literal': ('truncated_instruction', 'word_offset=1', 'remaining_words=0'),
            'wide': ('truncated_instruction', 'remaining_words=1', 'required_words=2'),
            'sdwa': ('truncated_instruction', 'word_offset=1', 'remaining_words=0'),
            'dpp': ('truncated_instruction', 'word_offset=1', 'remaining_words=0'),
            'illegal': ('illegal_encoding', 'raw_word=ffffffff'),
            'reserved': ('unknown_instruction_format', 'word_offset=1', 'opcode=1601',
                         'raw_word=c3c00000', 'phase=fixture'),
            'fetch': ('unknown_instruction_format', 'phase=fetch', 'shader=0000000000005678'),
            'main-empty': ('truncated_instruction', 'phase=main', 'shader=0000000000005678'),
            'copy-empty': ('truncated_instruction', 'phase=copy', 'shader=0000000000001234'),
        }
        for mode, fragments in cases.items():
            result = subprocess.run([str(output), mode], capture_output=True, text=True,
                                    timeout=15, env=dict(os.environ, BB_GPU_USER_DIR=str(state)))
            expected = 0 if fragments is None else 23
            if result.returncode != expected:
                raise RuntimeError(f'{mode}: expected {expected}, got {result.returncode}: {result.stderr}')
            for fragment in fragments or ():
                if fragment not in result.stderr:
                    raise RuntimeError(f'{mode}: missing {fragment}: {result.stderr}')
        # Original direct slice read crosses the same guard page. This tiny control
        # uses the pinned header and never calls the new decoder with an old layout.
        pinned = state/'pinned.cpp'
        pinned.write_text('''#include <sys/mman.h>
#include <unistd.h>
#include "shader_recompiler/frontend/decode.h"
int main() {
    const auto p = sysconf(_SC_PAGESIZE);
    auto* a = static_cast<char*>(mmap(nullptr, p*2, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0));
    if (a == MAP_FAILED || mprotect(a+p,p,PROT_NONE)) return 90;
    auto* word = reinterpret_cast<u32*>(a+p-4);
    *word = 7;
    Shader::Gcn::GcnCodeSlice code(word,word+1);
    volatile auto value = code.readu64();
    (void)value;
}''')
        original = state/'pinned'
        subprocess.run(['clang++', '-std=c++23', *flags,
                        '-I'+str(ROOT/'runtime/upstream/gpu/shim'),
                        '-I'+str(ROOT/'runtime/upstream/gpu/shadps4'),
                        '-I'+str(prefix/'include'),
                        str(pinned), '-o', str(original)], check=True)
        result = subprocess.run([str(original)], capture_output=True, text=True, timeout=10)
        if result.returncode not in (-10, -11):
            raise RuntimeError(f'pinned guard control did not fault: {result.returncode}')
        print('PASS: thirteen installed-decoder cases; explicit exit 23 before guarded reads, '
              'SMRD opcode 15 provenance, empty main/copy, fetch, valid literals, '
              'unaligned 64-bit reads and nested/thread-local context; pinned read faults; no GPU.')



if __name__ == '__main__':
    main()
