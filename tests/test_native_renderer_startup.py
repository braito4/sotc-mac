"""Shell startup selection; no GPU or game execution."""
import json
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]

class NativeStartup(unittest.TestCase):
    def run_mode(self, mode, native, backend=None):
        script = (ROOT / 'packaging/test-arm64.sh').read_text()
        function = script[script.index('configure_gpu_driver() {'):script.index('test_emitter() {')]
        code = 'set -eu\ntrial_root="$1"\ntrial_mode="$2"\nfail() { exit 99; }\n'
        code += 'python3() { printf "FALLBACK_CHECK\\n"; }\n' + function
        code += 'configure_gpu_driver\n/usr/bin/env\n'
        env = dict(os.environ, BB_METAL_NATIVE_BOOT=native, VK_DRIVER_FILES='stale',
                   VK_ICD_FILENAMES='stale', SDL_VULKAN_LIBRARY='stale')
        env.pop('BB_GFX_BACKEND', None)
        if backend is not None:
            env['BB_GFX_BACKEND'] = backend
        return subprocess.check_output(['bash', '-c', code, 'test', str(ROOT), mode],
                                       env=env, text=True)

    def test_native_startup_drops_loader_environment(self):
        for mode in ['--graphics-game-test', '--renderer-check']:
            output = self.run_mode(mode, '1')
            self.assertIn('BB_METAL_NATIVE_BOOT=1', output)
            self.assertIn('BB_METAL_FRAME=1', output)
            self.assertIn('BB_GFX_BACKEND=metal', output)
            for key in ['VK_DRIVER_FILES=', 'VK_ICD_FILENAMES=', 'SDL_VULKAN_LIBRARY=', 'FALLBACK_CHECK']:
                self.assertNotIn(key, output)

    def test_native_startup_overrides_stale_vulkan_resource_selection(self):
        output = self.run_mode('--graphics-game-test', '1', 'vulkan')
        self.assertIn('BB_GFX_BACKEND=metal', output)
        self.assertNotIn('BB_GFX_BACKEND=vulkan', output)

    def test_bridge_and_vulkan_check_keep_explicit_loader(self):
        for mode, native in [('--graphics-game-test', '0'), ('--vulkan-check', '1')]:
            output = self.run_mode(mode, native)
            self.assertIn('BB_METAL_NATIVE_BOOT=0', output)
            self.assertIn('FALLBACK_CHECK', output)
            self.assertIn('VK_DRIVER_FILES=', output)

if __name__ == '__main__':
    unittest.main()
