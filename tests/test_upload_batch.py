"""CPU-only budget controls; no game or graphics device needed."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class UploadBatchTests(unittest.TestCase):
    def test_budget_and_backend_controls(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'budget'
            subprocess.run(['clang++', '-std=c++20', '-fsanitize=undefined',
                '-I' + str(ROOT / 'runtime/src'), str(ROOT / 'tests/test_upload_batch.cpp'),
                '-o', str(binary)], check=True, capture_output=True)
            for setting in [None, '0', '1']:
                env = dict(os.environ)
                env.pop('SOTC_UPLOAD_BATCH', None)
                if setting is not None:
                    env['SOTC_UPLOAD_BATCH'] = setting
                subprocess.run([str(binary)], env=env, check=True, capture_output=True)
