"""Native dyld and renderer-entry contracts using synthetic libraries only."""
from pathlib import Path
import os
import platform
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).parents[1]
PROBE = ROOT / 'work/probe_entry_arm64'


@unittest.skipUnless(platform.system() == 'Darwin' and shutil.which('clang') and PROBE.is_file(),
                     'built native macOS diagnostic required')
class GpuLoadTests(unittest.TestCase):
    def check(self, path):
        state = Path(path).parent / 'private GPU state'
        state.mkdir(exist_ok=True)
        env = dict(os.environ, BB_GPU_USER_DIR=str(state))
        return subprocess.run([str(PROBE), '--gpu-load-check', str(path)], env=env, capture_output=True, text=True)

    def test_missing_file(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.check(Path(directory) / 'missing.dylib')
            self.assertEqual(result.returncode, 28)
            self.assertIn('STOP_GPU_LOAD', result.stderr)

    def test_missing_private_state_rejected_before_loading(self):
        env = dict(os.environ)
        env.pop('BB_GPU_USER_DIR', None)
        result = subprocess.run([str(PROBE), '--gpu-load-check', '/missing.dylib'],
                                env=env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 28)
        self.assertIn('STOP_GPU_STATE', result.stderr)

    def fixture(self, directory, exports, unresolved=False):
        root = Path(directory)
        source, output = root / 'fixture.c', root / 'fixture.dylib'
        source.write_text('#include <stdlib.h>\n' + '\n'.join(
            'void %s(void) { abort(); }' % name for name in exports) +
            ('\nextern void runtime_fixture_missing(void);\nvoid force_binding(void) { runtime_fixture_missing(); }\n' if unresolved else ''))
        subprocess.run(['clang', '-arch', 'arm64', '-dynamiclib', '-Wl,-undefined,dynamic_lookup',
                        str(source), '-o', str(output)], check=True, capture_output=True)
        return output

    def test_exports_checked_without_calling_renderer(self):
        exports = ('bbgpu_init', 'bbgpu_register_kernel', 'bbgpu_resolve',
                   'bbgpu_window_loop', 'bbgpu_handle_fault', 'bbgpu_symbol_count')
        with tempfile.TemporaryDirectory(prefix='GPU loader fixture ') as directory:
            result = self.check(self.fixture(directory, exports))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('renderer not initialized', result.stdout)

    def test_missing_export_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.check(self.fixture(directory, ('bbgpu_init',)))
            self.assertEqual(result.returncode, 28)
            self.assertIn('STOP_GPU_EXPORT: bbgpu_resolve', result.stderr)

    def renderer_fixture(self, directory, result):
        root = Path(directory)
        source, output = root / 'renderer.c', root / 'renderer.dylib'
        # Other API functions must never be invoked during renderer-only bring-up.
        source.write_text('''#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
typedef struct { const char *title, *serial, *user_dir; unsigned sdk, attributes; int width, height; } Config;
void bbgpu_register_kernel(void) { abort(); }
void bbgpu_resolve(void) { abort(); }
void bbgpu_handle_fault(void) { abort(); }
void bbgpu_symbol_count(void) { abort(); }
void bbgpu_patch_image(void) { abort(); }
int bbgpu_init(const Config *c) {
    if (strcmp(c->serial, "CUSA08809") || c->width != 1280 || c->height != 720 ||
        !c->user_dir || c->sdk || c->attributes) abort();
    puts("FIXTURE_INIT"); return RESULT;
}
void bbgpu_window_loop(void) { puts("FIXTURE_WINDOW_LOOP"); fflush(NULL); _Exit(0); }
'''.replace('RESULT', str(result)))
        subprocess.run(['clang', '-arch', 'arm64', '-dynamiclib', str(source), '-o', str(output)],
                       check=True, capture_output=True)
        return output

    def renderer_check(self, path):
        state = Path(path).parent / 'private renderer state'
        state.mkdir()
        return subprocess.run([str(PROBE), '--renderer-check', str(path)],
                              env=dict(os.environ, BB_GPU_USER_DIR=str(state)),
                              capture_output=True, text=True, timeout=10)

    def test_renderer_configuration_and_main_thread_event_loop(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.renderer_check(self.renderer_fixture(directory, 0))
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('FIXTURE_INIT', result.stdout)
            self.assertIn('FIXTURE_WINDOW_LOOP', result.stdout)
            self.assertIn('no game commands submitted', result.stdout)

    def test_renderer_failure_does_not_enter_event_loop(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.renderer_check(self.renderer_fixture(directory, 17))
            self.assertEqual(result.returncode, 28, result.stderr)
            self.assertIn('STOP_GPU_INIT: renderer returned 17', result.stderr)
            self.assertNotIn('FIXTURE_WINDOW_LOOP', result.stdout)

    def test_unresolved_host_hook_rejected_at_load(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.check(self.fixture(directory, ('bbgpu_init',), unresolved=True))
            self.assertEqual(result.returncode, 28, result.stderr)
            self.assertIn('STOP_GPU_LOAD', result.stderr)


if __name__ == '__main__':
    unittest.main()
