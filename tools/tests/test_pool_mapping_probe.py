"""Compile the production observation wrapper against a synthetic VMA table."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PoolMappingProbeTests(unittest.TestCase):
    def test_probe_preserves_results_and_mapping_and_bounds_output(self):
        compiler = shutil.which('clang') or shutil.which('cc')
        self.assertIsNotNone(compiler, 'A C compiler is required for the public probe regression')
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'fixture'
            subprocess.run([compiler, '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-pthread',
                            '-I' + str(ROOT / 'runtime/src'),
                            str(ROOT / 'tools/tests/pool_mapping_fixture.c'), '-o', str(binary)],
                           check=True, capture_output=True)
            for mode in ('0', '1'):
                result = subprocess.run([str(binary), mode], check=True, capture_output=True,
                                        text=True, timeout=5)
                self.assertIn('PASS: result preserved', result.stdout)
                self.assertEqual(result.stdout.count('Pool mapping probe: sample='),
                                 256 if mode == '1' else 0)
                self.assertEqual(result.stdout.count('limit=256 reached'), int(mode))
