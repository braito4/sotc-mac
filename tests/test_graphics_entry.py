"""Synthetic linked x86 image + native GPU double: exercise the actual loader.

No game code, renderer execution, window, assets or physical GPU are required.
"""
import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).parents[1]
PROBE = ROOT / 'work/probe_entry_arm64'


def synthetic_image(path):
    image = bytearray(16384)
    code = bytearray.fromhex('bf ff000000 31f6 31d2 31c9')
    code += b'\xff\x15' + struct.pack('<i', 256-(len(code)+6))
    code += b'\x48\x89\xc7'
    code += b'\xff\x15' + struct.pack('<i', 264-(len(code)+6))
    code += b'\x0f\x0b'
    image[:len(code)] = code
    image[512:515] = bytes.fromhex('31c0c3')  # Original synthetic module initializer: return 0.
    def words(*values):
        return struct.pack('<'+'Q'*len(values), *values)
    path.write_bytes(b'BBPROBE5' + words(16384, 0, 1, 2, 2, 1) +
        words(128, 0, 0, 0, 0, 1) + words(0, 16384, 512, 0, 0, 0, 0) + words(0) +
        words(0, 16384, 5) + b'Up36PTk687E#G#H'.ljust(128, b'\0') +
        b'fixture-stop#none'.ljust(128, b'\0') + words(256, 1, 0, 0, 264, 1, 1, 0) + image)


@unittest.skipUnless(platform.system() == 'Darwin' and shutil.which('clang') and PROBE.is_file(),
                     'built native ARM64 diagnostic required')
class GraphicsEntryTests(unittest.TestCase):
    def test_real_loader_routes_guest_calls_on_worker_in_both_cpu_modes(self):
        with tempfile.TemporaryDirectory(prefix='graphics entry fixture ') as directory:
            root = Path(directory)
            source, library = root/'gpu.c', root/'gpu.dylib'
            source.write_text(r'''
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <setjmp.h>
#include <sys/mman.h>
extern __thread sigjmp_buf *runtime_fault_recover;
typedef struct { const char *title, *serial, *user_dir; unsigned sdk, attributes; int width, height; } Config;
static int registered, initialized;
void bbgpu_register_kernel(void) { if (!pthread_main_np()) abort(); registered=1; }
int bbgpu_init(const Config *c) {
    if (!registered || !pthread_main_np() || strcmp(c->serial,"CUSA08809") || c->sdk) abort();
    /* Exercise the actual probe's speculative-read fault recovery before any
       guest runs, as renderer resource tracking can do during initialization. */
    void *page=mmap(NULL,16384,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);
    if (page==MAP_FAILED) abort();
    sigjmp_buf recovery;
    if (!sigsetjmp(recovery,1)) {
        runtime_fault_recover=&recovery;
        (void)*(volatile char *)page;
        abort();
    }
    if (runtime_fault_recover || munmap(page,16384)) abort();
    puts("FIXTURE_FAULT_RECOVERED");
    initialized=1; puts("FIXTURE_MAIN_THREAD_INIT"); return 0;
}
static int video_open(int user, int bus, int index, void *params) {
    if (!initialized || pthread_main_np() || user!=255 || bus || index || params) abort();
    puts("FIXTURE_GUEST_WORKER_OPEN"); return 0x1234;
}
static void unexpected_call(void) { abort(); }
uintptr_t bbgpu_resolve(const char *name) {
    if (!strcmp(name,"Up36PTk687E#G#H")) return (uintptr_t)video_open;
    return (uintptr_t)unexpected_call;
}
int bbgpu_handle_fault(void *c, void *a) { (void)c; (void)a; return 0; }
unsigned bbgpu_symbol_count(void) { return 1; }
void bbgpu_patch_image(void) { abort(); }
void bbgpu_window_loop(void) {
    if (!pthread_main_np()) abort();
    for (;;) usleep(1000);
}
''')
            subprocess.run(['clang', '-arch', 'arm64', '-dynamiclib', '-Wl,-undefined,dynamic_lookup', str(source), '-o', str(library)],
                           check=True, capture_output=True)
            boot = root/'synthetic-boot.bin'
            synthetic_image(boot)
            for mode in ('1', '0'):
                state = root/('state-'+mode)
                state.mkdir()
                result = subprocess.run([str(PROBE), str(boot), '--bootstrap', '--game-root', str(root),
                    '--graphics-library', str(library)], capture_output=True, text=True, timeout=15,
                    env=dict(os.environ, BB_JIT=mode, BB_GPU_USER_DIR=str(state),
                             BB_DMEM_MB='16', BB_DMEM_EXTRA_MB='0', SOTC_DIAGNOSTIC_SECONDS='10'))
                self.assertEqual(result.returncode, 20, result.stdout+result.stderr)
                self.assertIn('FIXTURE_MAIN_THREAD_INIT', result.stdout)
                self.assertIn('FIXTURE_FAULT_RECOVERED', result.stdout)
                self.assertIn('FIXTURE_GUEST_WORKER_OPEN', result.stdout)
                self.assertIn('Guest first argument: 0x1234', result.stdout)
                self.assertIn('name=fixture-stop#none', result.stdout)
                self.assertIn('Linked module 0 initializer returned 0', result.stdout)


if __name__ == '__main__':
    unittest.main()
