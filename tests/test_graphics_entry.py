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
    # Open a restricted 8-channel stream, query its real bound device, then close it.
    code += bytes.fromhex('bf ff000000 31f6 31d2 b9 00010000 41b8 80bb0000 41b9 05000100')
    code += b'\xff\x15' + struct.pack('<i',256-(len(code)+6))
    code += bytes.fromhex('41 89c4 89c7')  # Preserve the stream handle in r12d.
    code += b'\x48\x8d\x35' + struct.pack('<i',1024-(len(code)+7))
    code += b'\xff\x15' + struct.pack('<i',264-(len(code)+6))
    code += bytes.fromhex('85c0 7402 0f0b 4489e7')
    code += b'\x48\x8d\x35' + struct.pack('<i',3072-(len(code)+7))
    code += b'\xff\x15' + struct.pack('<i',288-(len(code)+6))
    code += bytes.fromhex('3d 00010000 7402 0f0b')  # Return frames, not interleaved samples.
    code += b'\x44\x89\x25' + struct.pack('<i',2048-(len(code)+7))
    code += b'\x48\x8d\x05' + struct.pack('<i',3072-(len(code)+7))
    code += b'\x48\x89\x05' + struct.pack('<i',2056-(len(code)+7))
    code += b'\x48\x8d\x3d' + struct.pack('<i',2048-(len(code)+7))
    code += bytes.fromhex('be 01000000')
    code += b'\xff\x15' + struct.pack('<i',280-(len(code)+6))
    code += bytes.fromhex('3d 00010000 7402 0f0b 4489e7')
    code += b'\xff\x15' + struct.pack('<i',272-(len(code)+6))
    code += b'\xff\x15' + struct.pack('<i',304-(len(code)+6))
    code += bytes.fromhex('3d 0200e380 7402 0f0b')  # No PS4 launch-control context.
    code += bytes.fromhex('31ff 31f6')
    code += b'\xff\x15' + struct.pack('<i',312-(len(code)+6))
    code += bytes.fromhex('3d 02005580 7402 0f0b')  # NP init error precedes null-pointer checks.
    code += bytearray.fromhex('bf ff000000 31f6 31d2 31c9')
    code += b'\xff\x15' + struct.pack('<i', 248-(len(code)+6))
    if fault:
        code += bytes.fromhex('c700 03000000')  # Guest writes to the provider's protected/EOF page.
    code += b'\x48\x89\xc7'
    code += b'\xff\x15' + struct.pack('<i', 296-(len(code)+6))
    code += b'\x0f\x0b'
    assert len(code)<232
    image[:len(code)] = code
    image[3072:11264]=b'\x3f'*8192  # Original synthetic samples, deliberately nonzero.
    # Synthetic initializer checks service availability, language and the full status profile.
    initializer=bytes.fromhex('31ff')+b'\xff\x15'+struct.pack('<i',320-(512+8))
    initializer+=bytes.fromhex('3d 02005580 7402 0f0b')
    initializer+=bytes.fromhex('bf 01000000')
    initializer+=b'\x48\x8d\x35'+struct.pack('<i',1200-(512+len(initializer)+7))
    initializer+=b'\xff\x15'+struct.pack('<i',328-(512+len(initializer)+6))
    initializer+=bytes.fromhex('85c0 7402 0f0b')
    initializer+=b'\x83\x3d'+struct.pack('<i',1200-(512+len(initializer)+7))+b'\x01'
    initializer+=bytes.fromhex('7402 0f0b')
    initializer+=b'\xff\x15'+struct.pack('<i',336-(512+len(initializer)+6))
    initializer+=bytes.fromhex('3d 09214180 7402 0f0b')
    initializer+=b'\x48\x8d\x3d'+struct.pack('<i',1280-(512+len(initializer)+7))
    initializer+=b'\xff\x15'+struct.pack('<i',344-(512+len(initializer)+6))
    initializer+=bytes.fromhex('85c0 7402 0f0b')
    initializer+=b'\x48\x8d\x3d'+struct.pack('<i',1280-(512+len(initializer)+7))
    initializer+=bytes.fromhex('b9 88000000 31c0 f3ae 7402 0f0b 31ff')
    initializer+=b'\xff\x15'+struct.pack('<i',352-(512+len(initializer)+6))
    initializer+=bytes.fromhex('3d 0b009f80 7402 0f0b 31ff 31f6')
    initializer+=b'\xff\x15'+struct.pack('<i',360-(512+len(initializer)+6))
    initializer+=bytes.fromhex('3d 01009f80 7402 0f0b 31ff 31f6 31d2 31c9')
    initializer+=b'\xff\x15'+struct.pack('<i',368-(512+len(initializer)+6))
    initializer+=bytes.fromhex('3d 01165580 7402 0f0b 31ff 31f6')
    initializer+=b'\xff\x15'+struct.pack('<i',376-(512+len(initializer)+6))
    initializer+=bytes.fromhex('3d 0100d980 7402 0f0b')
    for slot in (384,392):
        initializer+=b'\xff\x15'+struct.pack('<i',slot-(512+len(initializer)+6))
        initializer+=bytes.fromhex('85c0 7402 0f0b')
    for slot in range(408,472,8):
        initializer+=bytes.fromhex('31ff 31f6 31d2 31c9')
        initializer+=b'\xff\x15'+struct.pack('<i',slot-(512+len(initializer)+6))
        initializer+=bytes.fromhex('3d 01009f80 7402 0f0b')
    initializer+=b'\xff\x15'+struct.pack('<i',472-(512+len(initializer)+6))
    initializer+=bytes.fromhex('3d 0100b880 7402 0f0b 31ff')
    initializer+=b'\xff\x15'+struct.pack('<i',480-(512+len(initializer)+6))
    initializer+=bytes.fromhex('3d 0d00b880 7402 0f0b')
    initializer+=bytes.fromhex('bf 25000000 31f6 31d2')
    initializer+=b'\xff\x15'+struct.pack('<i',400-(512+len(initializer)+6))
    initializer+=bytes.fromhex('3d 0b002980 7402 0f0b 31c0c3')
    assert 512+len(initializer)<1024
    image[1280:1416]=b'\xa5'*136
    image[512:512+len(initializer)]=initializer
    def words(*values):
        return struct.pack('<'+'Q'*len(values), *values)
    path.write_bytes(b'BBPROBE5' + words(16384, 0, 1, 32, 32, 1) +
        words(1536, 0, 0, 0, 0, 1) + words(0, 16384, 512, 0, 0, 0, 0) + words(0) +
        words(0, 16384, 5) + b'hv1luiJrqQM#H#I'.ljust(128, b'\0') + b'JfEPXVxhFqA#q#M'.ljust(128, b'\0') +
        b'Up36PTk687E#G#H'.ljust(128, b'\0') + b'ekNvsT22rsY#q#M'.ljust(128,b'\0') +
        b'GrQ9s4IrNaQ#q#M'.ljust(128,b'\0') + b's1--uE9mBFw#q#M'.ljust(128,b'\0') +
        b'w3PdaSTSwGE#q#M'.ljust(128,b'\0') + b'QOQtbeDqsT4#q#M'.ljust(128,b'\0') +
        b'fixture-stop#none'.ljust(128, b'\0') + b'Vo5V8KAwCmk#a#a'.ljust(128,b'\0') +
        b'Ec63y59l9tw#S#T'.ljust(128,b'\0') +
        b'A2CQ3kgSopQ#S#T'.ljust(128,b'\0') + b'fZo48un7LK4#a#a'.ljust(128,b'\0') + b'gky0+oaNM4k#D#E'.ljust(128,b'\0') + b'rPo6tV8D9bM#a#a'.ljust(128,b'\0') + b'TywrFKCoLGY#u#d'.ljust(128,b'\0') + b'dyIhnXq-0SM#u#d'.ljust(128,b'\0') + b'XbkjbobZlCY#O#P'.ljust(128,b'\0') + b'R9lA82OraNs#t#Z'.ljust(128,b'\0') + b'ERKzksauAJA#v#f'.ljust(128,b'\0') + b'KK3Bdg1RWK0#v#f'.ljust(128,b'\0') + b'MTxxrOCeSig#G#H'.ljust(128,b'\0') +
        b'32HQAQdwM2o#u#d'.ljust(128,b'\0') + b'0z45PIH+SNI#u#d'.ljust(128,b'\0') +
        b'BMR4F-Uek3E#u#d'.ljust(128,b'\0') + b'VwadwBBBJ80#u#d'.ljust(128,b'\0') +
        b'65VH0Qaaz6s#u#d'.ljust(128,b'\0') + b'85zul--eGXs#u#d'.ljust(128,b'\0') +
        b'c88Yy54Mx0w#u#d'.ljust(128,b'\0') + b'S1GkePI17zQ#u#d'.ljust(128,b'\0') +
        b's9e3+YpRnzw#v#f'.ljust(128,b'\0') + b'4tPhsP6FpDI#v#f'.ljust(128,b'\0') +
        words(232,1,0,0,240,1,1,0,248,1,2,0,256,1,3,0,264,1,4,0,272,1,5,0,280,1,6,0,288,1,7,0,296,1,8,0,304,1,9,0,312,1,10,0,320,1,11,0,328,1,12,0,336,1,13,0,344,1,14,0,352,1,15,0,360,1,16,0,368,1,17,0,376,1,18,0,384,1,19,0,392,1,20,0,400,1,21,0,408,1,22,0,416,1,23,0,424,1,24,0,432,1,25,0,440,1,26,0,448,1,27,0,456,1,28,0,464,1,29,0,472,1,30,0,480,1,31,0) + image)


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
typedef struct { int format, channels, frequency; } AudioSpec;
static int audio_stream, audio_opened;
void *SDL_OpenAudioDeviceStream(uint32_t device,const AudioSpec *spec,void *callback,void *userdata) {
    if(pthread_main_np() || device!=UINT32_MAX || spec->format!=0x8120 || spec->channels!=8 ||
       spec->frequency!=48000 || callback || userdata || audio_opened) abort();
    audio_opened=1;return &audio_stream;
}
bool SDL_ResumeAudioStreamDevice(void *stream) { if(stream!=&audio_stream || !audio_opened) abort();return true; }
void SDL_DestroyAudioStream(void *stream) { if(stream!=&audio_stream || !audio_opened) abort();audio_opened=0; }
uint32_t SDL_GetAudioStreamDevice(void *stream) { if(stream!=&audio_stream || !audio_opened) abort();return 100; }
bool SDL_GetAudioDeviceFormat(uint32_t device,AudioSpec *spec,int *frames) {
    if(device!=100 || !audio_opened) abort();
    if(frames) *frames=1024;
    *spec=(AudioSpec){0x8120,2,48000};puts("FIXTURE_AUDIO_DEVICE_QUERY");return true;
}
bool SDL_PutAudioStreamData(void *stream,const void *data,int bytes) {
    if(stream!=&audio_stream || !audio_opened || !data || bytes!=8192) abort();
    for(int i=0;i<bytes;++i) if(((const unsigned char *)data)[i]!=0x3f) abort();
    puts("FIXTURE_PCM_ACCEPTED");return true;
}
int SDL_GetAudioStreamQueued(void *stream) { if(stream!=&audio_stream || !audio_opened) abort();return 0; }
bool SDL_FlushAudioStream(void *stream) { if(stream!=&audio_stream || !audio_opened) abort();return true; }
uint32_t SDL_WasInit(uint32_t flags) { if (!pthread_main_np()) abort(); return getenv("SOTC_FIXTURE_NO_INPUT") ? 0 : flags; }
const bool *SDL_GetKeyboardState(int *count) { static bool keys[512]; *count=512; return keys; }
int SDL_GetScancodeFromName(const char *name) { (void)name; return 1; }
uint32_t *SDL_GetGamepads(int *count) { *count=0; return NULL; }
void SDL_free(void *p) { if(p) abort(); }
void *SDL_OpenGamepad(uint32_t id) { (void)id; return NULL; }
void SDL_CloseGamepad(void *p) { (void)p; abort(); }
bool SDL_GamepadConnected(void *p) { (void)p; return false; }
const char *SDL_GetGamepadName(void *p) { (void)p; return "fixture"; }
int16_t SDL_GetGamepadAxis(void *p,int axis) { (void)p; (void)axis; return 0; }
bool SDL_GetGamepadButton(void *p,int button) { (void)p; (void)button; return false; }
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
    const char *dma=getenv("BB_DIRECT_MEMORY_ACCESS");if(!dma) abort();
    printf("FIXTURE_DMA=%s\\n",dma);
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
                baseline=dict(os.environ, BB_JIT=mode, BB_GPU_USER_DIR=str(state),
                              BB_DMEM_MB='16', BB_DMEM_EXTRA_MB='0', SOTC_DIAGNOSTIC_SECONDS='10')
                baseline.pop('BB_DIRECT_MEMORY_ACCESS',None)
                result = subprocess.run([str(PROBE), str(boot), '--bootstrap', '--game-root', str(root),
                    '--graphics-library', str(library)], capture_output=True, text=True, timeout=15,env=baseline)
                self.assertEqual(result.returncode, 20, result.stdout+result.stderr)
                self.assertIn('FIXTURE_MAIN_THREAD_INIT', result.stdout)
                self.assertIn('FIXTURE_DMA=0',result.stdout)
                self.assertIn('Diagnostic watchdog: 10 seconds',result.stdout)
                self.assertIn('FIXTURE_FAULT_RECOVERED', result.stdout)
                self.assertIn('FIXTURE_GUEST_WORKER_OPEN', result.stdout)
                self.assertIn('Guest first argument: 0x1234', result.stdout)
                self.assertIn('name=fixture-stop#none', result.stdout)
                self.assertIn('Linked module 0 initializer returned 0', result.stdout)
                self.assertIn('keyboard backend ready; initialized=1', result.stdout)
                self.assertIn('AudioOut: SDL ready=1, initialized=1',result.stdout)
                self.assertIn('FIXTURE_AUDIO_DEVICE_QUERY',result.stdout)
                self.assertIn('streams opened=1, closed=1',result.stdout)
                self.assertIn('state queries=1',result.stdout)
                self.assertEqual(result.stdout.count('FIXTURE_PCM_ACCEPTED'),2)
                self.assertIn('PCM buffers=2, PCM bytes=16384',result.stdout)
                self.assertIn('unavailable splash-hide requests=1',result.stdout)
                self.assertIn('unavailable NP title registrations=1',result.stdout)
                self.assertIn('unavailable NP content-restriction requests=1',result.stdout)
                self.assertIn('system language queries=1',result.stdout)
                self.assertIn('system status queries=1',result.stdout)
                self.assertIn('unavailable SaveData Initialize3 requests=1',result.stdout)
                self.assertIn('unavailable SaveData directory searches=1',result.stdout)
                self.assertIn('mount=1, mount2=1, unmount=1, backup unmount=1, info=1, parameter=1, icon=1, delete=1',result.stdout)
                self.assertIn('unavailable trophy requests: create context=1',result.stdout)
                self.assertIn('unavailable AppContent requests: initialize=1, add-on lists=0',result.stdout)
                self.assertIn('save-dialog status queries=1, updates=1 (NONE; no dialog instance)',result.stdout)
                self.assertIn('save-dialog startup requests: initialize=1, open=1',result.stdout)
                self.assertIn('unsupported VideoOut window-margin requests=1',result.stdout)
                self.assertIn('unavailable network-control initialization requests=1',result.stdout)
                for seconds in (None,'0','1800'):
                    override=root/('config-'+mode+'-'+str(seconds));override.mkdir()
                    env=dict(baseline,BB_DIRECT_MEMORY_ACCESS='0',BB_GPU_USER_DIR=str(override))
                    env.pop('SOTC_DIAGNOSTIC_SECONDS',None)
                    if seconds is not None:env['SOTC_DIAGNOSTIC_SECONDS']=seconds
                    check=subprocess.run([str(PROBE),str(boot),'--bootstrap','--game-root',str(root),
                        '--graphics-library',str(library)],capture_output=True,text=True,timeout=15,env=env)
                    self.assertEqual(check.returncode,20,check.stdout+check.stderr)
                    self.assertIn('FIXTURE_DMA=0',check.stdout)
                    expected = ('Diagnostic duration limit: none' if seconds in (None, '0') else
                                'Diagnostic watchdog: '+seconds+' seconds')
                    self.assertIn(expected,check.stdout)
                rejected_dma=subprocess.run([str(PROBE),str(boot),'--bootstrap','--game-root',str(root),
                    '--graphics-library',str(library)],capture_output=True,text=True,timeout=15,
                    env=dict(baseline,BB_DIRECT_MEMORY_ACCESS='1'))
                self.assertEqual(rejected_dma.returncode,1,rejected_dma.stdout+rejected_dma.stderr)
                self.assertIn('STOP_GPU_CONFIG',rejected_dma.stderr)
                self.assertNotIn('FIXTURE_MAIN_THREAD_INIT',rejected_dma.stdout)
                self.assertNotIn('Starting linked module',rejected_dma.stdout)
                for fault, permissions in (('protected','r--'), ('backing','rw-')):
                    fault_boot=root/('fault-'+fault+'.bin')
                    synthetic_image(fault_boot, fault=True)
                    failed=subprocess.run([str(PROBE),str(fault_boot),'--bootstrap','--game-root',str(root),
                        '--graphics-library',str(library)],capture_output=True,text=True,timeout=15,
                        env=dict(baseline,BB_JIT=mode,BB_GPU_USER_DIR=str(state),
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
                env=dict(baseline, BB_GPU_USER_DIR=str(state), SOTC_FIXTURE_NO_INPUT='1'))
            self.assertEqual(rejected.returncode, 28, rejected.stdout+rejected.stderr)
            self.assertIn('STOP_INPUT_BACKEND', rejected.stderr)
            self.assertNotIn('Starting linked module', rejected.stdout)



if __name__ == '__main__':
    unittest.main()
