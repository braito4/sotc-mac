"""Installed sample conversion and real assertion-hook controls, without GPU calls."""
from pathlib import Path
import os
import subprocess
import tempfile
from build_gpu import ROOT, source_path, build_path
from check_buffer_copy_chunks import function
from check_gpu_link import host_symbol, inspect, parse_symbols


def main():
    tree = source_path(ROOT); prefix = tree/'deps/macos-arm64'; build = build_path(ROOT)
    library = build/'libbbgpu.dylib'
    _, imports = parse_symbols(inspect(library))
    definitions = ['#include <stdlib.h>', '#include <stdint.h>', '#include <setjmp.h>']
    for name in sorted(n for n in imports if host_symbol(n)):
        if name == 'runtime_fault_recover':
            definitions.append('_Thread_local sigjmp_buf *runtime_fault_recover;')
        elif name in ('bb_image_base', 'runtime_disabled_optimizations', 'runtime_experiment_bits'):
            definitions.append('uint64_t '+name+';')
        elif name == 'runtime_restarting':
            definitions.append('volatile int runtime_restarting;')
        else:
            definitions.append('void '+name+'(void) { abort(); }')
    original = (ROOT/'runtime/upstream/gpu/shadps4/video_core/renderer_vulkan/liverpool_to_vk.cpp').read_text()
    text = (ROOT/'tests/sample_counts_fixture.cpp.in').read_text().replace(
        '@RAW@', function(original, 'vk::SampleCountFlagBits RawNumSamples(')).replace(
        '@SELECT@', function(original, 'vk::SampleCountFlagBits NumSamples('))
    with tempfile.TemporaryDirectory(prefix='samples-cpu-', dir=ROOT/'work') as temporary:
        folder = Path(temporary); host = folder/'host.c'; obj = folder/'host.o'
        host.write_text('\n'.join(definitions)+'\n')
        subprocess.run(['clang', '-arch', 'arm64', '-mmacosx-version-min=15.0',
                        '-c', str(host), '-o', str(obj)], check=True)
        cpp = folder/'fixture.cpp'; cpp.write_text(text)
        for hook in (False, True):
            binary = folder/('with-hook' if hook else 'without-hook')
            subprocess.run(['clang++', '-std=c++23', '-arch', 'arm64', '-mmacosx-version-min=15.0',
                '-O1', '-fsanitize=undefined', '-fno-sanitize-recover=undefined',
                *(['-DTEST_ASSERT_HOOK'] if hook else []),
                '-I'+str(ROOT/'runtime/src'), '-I'+str(tree/'gpu/shim'),
                '-I'+str(tree/'gpu/shadps4'), '-I'+str(prefix/'include'),
                '-I'+str(build/'include'), str(cpp), str(obj), str(library),
                '-L'+str(prefix/'lib'), '-lfmt', '-Wl,-rpath,'+str(build),
                '-Wl,-export_dynamic', '-o', str(binary)], check=True)
            env = dict(os.environ, BB_GPU_USER_DIR=str(folder))
            for mode in ([], ['pinned-empty']):
                result = subprocess.run([str(binary), *mode], capture_output=True, text=True,
                                        timeout=15, env=env)
                if result.returncode or 'PASS:' not in result.stdout:
                    raise RuntimeError(result.stdout+result.stderr)
                print(result.stdout.strip())
            for requested, mask, reason in [(0, 31, 'unsupported_guest_count'),
                    (3, 31, 'unsupported_guest_count'), (32, 31, 'unsupported_guest_count'),
                    (4294967295, 31, 'unsupported_guest_count'),
                    (8, 0, 'no_supported_fallback'), (2, 16, 'no_supported_fallback')]:
                result = subprocess.run([str(binary), str(requested), str(mask), 'test-origin'],
                    capture_output=True, text=True, timeout=15, env=env)
                required = (f'reason={reason}', f'requested={requested}',
                            f'supported=0x{mask:x}', 'origin=test-origin',
                            'STOP: GPU library assertion failed')
                if result.returncode != 23 or not all(x in result.stderr for x in required):
                    raise RuntimeError(result.stdout+result.stderr)
                calls = result.stderr.count('TEST: optional assertion history hook called')
                if calls != int(hook):
                    raise RuntimeError('Optional fatal hook presence/absence failed: '+result.stderr)
        print('PASS: real exit 23, count/mask/origin provenance, optional fatal hook present/absent; CPU only')


if __name__ == '__main__':
    main()
