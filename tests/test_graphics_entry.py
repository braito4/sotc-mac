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


def synthetic_image(path, fault=False):
    image = bytearray(16384)
    code = bytearray(b'\xff\x15' + struct.pack('<i', 232-6))  # Initialize the keyboard and audio adapters.
    code += b'\xff\x15' + struct.pack('<i', 240-(len(code)+6))
    code += bytearray.fromhex('bf ff000000 31f6 31d2 31c9')
    code += b'\xff\x15' + struct.pack('<i', 248-(len(code)+6))
    if fault:
        code += bytes.fromhex('c700 03000000')  # Guest writes to the provider's protected/EOF page.
    code += b'\x48\x89\xc7'
    code += b'\xff\x15' + struct.pack('<i', 256-(len(code)+6))
    code += b'\x0f\x0b'
    image[:len(code)] = code
    image[512:515] = bytes.fromhex('31c0c3')  # Original synthetic module initializer: return 0.
    def words(*values):
        return struct.pack('<'+'Q'*len(values), *values)
    path.write_bytes(b'BBPROBE5' + words(16384, 0, 1, 4, 4, 1) +
        words(128, 0, 0, 0, 0, 1) + words(0, 16384, 512, 0, 0, 0, 0) + words(0) +
        words(0, 16384, 5) + b'hv1luiJrqQM#H#I'.ljust(128, b'\0') + b'JfEPXVxhFqA#q#M'.ljust(128, b'\0') +
        b'Up36PTk687E#G#H'.ljust(128, b'\0') + b'fixture-stop#none'.ljust(128, b'\0') +
        words(232, 1, 0, 0, 240, 1, 1, 0, 248, 1, 2, 0, 256, 1, 3, 0) + image)


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
#include <fcntl.h>
extern __thread sigjmp_buf *runtime_fault_recover;
typedef struct { const char *title, *serial, *user_dir; unsigned sdk, attributes; int width, height; } Config;
#include <stdbool.h>
bool SDL_InitSubSystem(uint32_t flags) { if (!pthread_main_np() || flags!=0x10) abort(); return true; }
const char *SDL_GetCurrentAudioDriver(void) { if (!pthread_main_np()) abort(); return "coreaudio"; }
void *SDL_OpenAudioDeviceStream(uint32_t device,const void *spec,void *callback,void *userdata) {
    (void)device;(void)spec;(void)callback;(void)userdata;abort();
}
bool SDL_ResumeAudioStreamDevice(void *stream) { (void)stream;abort(); }
void SDL_DestroyAudioStream(void *stream) { (void)stream;abort(); }
uint32_t SDL_WasInit(uint32_t flags) { if (!pthread_main_np()) abort(); return getenv("SOTC_FIXTURE_NO_INPUT") ? 0 : flags; }
const bool *SDL_GetKeyboardState(int *count) { static bool keys[512]; *count=512; return keys; }
int SDL_GetScancodeFromName(const char *name) { (void)name; return 1; }
int bbgpu_overlay_captures_input(void) { return 0; }
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
static intptr_t video_open(int user, int bus, int index, void *params) {
    if (!initialized || pthread_main_np() || user!=255 || bus || index || params) abort();
    const char *fault=getenv("SOTC_FIXTURE_FAULT");
    if (fault) {
        if (!strcmp(fault,"protected")) {
            void *page=mmap(NULL,16384,PROT_READ,MAP_PRIVATE|MAP_ANON,-1,0);
            if (page==MAP_FAILED) abort();
            return (intptr_t)page;
        }
        /* A shared mapping can be writable yet SIGBUS on an absent backing
           page. Exercise that distinction without a GPU or game content. */
        char path[]="/tmp/sotc-fault-fixture-XXXXXX";
        int fd=mkstemp(path);
        if (fd<0 || unlink(path) || ftruncate(fd,16384)) abort();
        char *page=mmap(NULL,32768,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
        if (page==MAP_FAILED || close(fd)) abort();
        return (intptr_t)(page+16384);
    }
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
                self.assertIn('keyboard backend ready; initialized=1', result.stdout)
                self.assertIn('AudioOut: SDL ready=1, initialized=1',result.stdout)
                for fault, permissions in (('protected','r--'), ('backing','rw-')):
                    fault_boot=root/('fault-'+fault+'.bin')
                    synthetic_image(fault_boot, fault=True)
                    failed=subprocess.run([str(PROBE),str(fault_boot),'--bootstrap','--game-root',str(root),
                        '--graphics-library',str(library)],capture_output=True,text=True,timeout=15,
                        env=dict(os.environ,BB_JIT=mode,BB_GPU_USER_DIR=str(state),
                                 BB_DMEM_MB='16',BB_DMEM_EXTRA_MB='0',SOTC_DIAGNOSTIC_SECONDS='10',
                                 SOTC_FIXTURE_FAULT=fault))
                    self.assertIn(failed.returncode,(138,139),failed.stdout+failed.stderr)
                    self.assertIn('STOP_EXECUTION_FAULT:',failed.stderr)
                    self.assertRegex(failed.stderr,r'signal code=0x[0-9a-f]+ esr=0x[1-9a-f][0-9a-f]* far=0x')
                    self.assertIn('gpu traps=0x0',failed.stderr)
                    self.assertRegex(failed.stderr,r'host mapping: [0-9a-f]+-[0-9a-f]+ '+permissions)
                    if fault=='backing':
                        self.assertIn('STOP_EXECUTION_FAULT: SIGBUS',failed.stderr)
            state = root/'unavailable-input-state'
            state.mkdir()
            rejected = subprocess.run([str(PROBE), str(boot), '--bootstrap', '--game-root', str(root),
                '--graphics-library', str(library)], capture_output=True, text=True, timeout=15,
                env=dict(os.environ, BB_GPU_USER_DIR=str(state), SOTC_FIXTURE_NO_INPUT='1'))
            self.assertEqual(rejected.returncode, 28, rejected.stdout+rejected.stderr)
            self.assertIn('STOP_INPUT_BACKEND', rejected.stderr)
            self.assertNotIn('Starting linked module', rejected.stdout)



if __name__ == '__main__':
    unittest.main()
