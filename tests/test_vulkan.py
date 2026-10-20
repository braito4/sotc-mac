"""Surface-extension regression: a Vulkan loader alone is insufficient on macOS."""
import importlib.util
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).parents[1]
spec = importlib.util.spec_from_file_location('check_vulkan', ROOT / 'tools/check_vulkan.py')
vulkan = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vulkan)
PREFIX = ROOT / 'work/gpu-source/deps/macos-arm64'
LOADER = PREFIX / 'lib/libvulkan.1.dylib'
ICD = PREFIX / 'share/vulkan/icd.d/MoltenVK_icd.json'


class VulkanSurfaceTests(unittest.TestCase):
    def test_metal_surface_with_base_surface(self):
        vulkan.require_surface_extensions({'VK_KHR_surface', 'VK_EXT_metal_surface'})

    def test_legacy_macos_surface_with_base_surface(self):
        vulkan.require_surface_extensions({'VK_KHR_surface', 'VK_MVK_macos_surface'})

    def test_loader_only_extensions_do_not_supply_surface(self):
        with self.assertRaisesRegex(ValueError, 'VK_KHR_surface'):
            vulkan.require_surface_extensions({'VK_EXT_debug_utils', 'VK_KHR_portability_enumeration'})

    def test_surface_without_macos_backend_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Metal/macOS'):
            vulkan.require_surface_extensions({'VK_KHR_surface', 'VK_KHR_xcb_surface'})

    @unittest.skipUnless(platform.system() == 'Darwin' and LOADER.is_file() and ICD.is_file(),
                         'built native Vulkan/MoltenVK dependencies required')
    def test_real_loader_fails_without_icd_and_passes_with_bundled_icd(self):
        command = [sys.executable, str(ROOT / 'tools/check_vulkan.py'), '--library', str(LOADER)]
        # Each fresh process gets its own Vulkan loader discovery cache.
        with tempfile.TemporaryDirectory(prefix='Vulkan missing driver ') as directory:
            missing = str(Path(directory) / 'absent.json')
            env = dict(os.environ, VK_DRIVER_FILES=missing, VK_ICD_FILENAMES=missing)
            rejected = subprocess.run(command, env=env, capture_output=True, text=True, timeout=20)
            self.assertEqual(rejected.returncode, 28, rejected.stderr)
            self.assertIn('VK_KHR_surface', rejected.stderr)
            env.update(VK_DRIVER_FILES=str(ICD), VK_ICD_FILENAMES=str(ICD), SDL_VULKAN_LIBRARY=str(LOADER))
            accepted = subprocess.run(command, env=env, capture_output=True, text=True, timeout=20)
            self.assertEqual(accepted.returncode, 0, accepted.stderr)
            self.assertIn('PASS: Vulkan loader exposes VK_KHR_surface', accepted.stdout)
            shell = subprocess.run(['bash', str(ROOT / 'packaging/test-arm64.sh'), '--vulkan-check'],
                                   env=dict(os.environ, VK_DRIVER_FILES=missing, VK_ICD_FILENAMES=missing,
                                            SDL_VULKAN_LIBRARY=missing), capture_output=True, text=True, timeout=20)
            self.assertEqual(shell.returncode, 0, shell.stderr)
            self.assertIn('PASS: Vulkan loader exposes VK_KHR_surface', shell.stdout)


if __name__ == '__main__':
    unittest.main()
