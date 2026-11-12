"""Portable checks for the actual merged-region manager, without Metal or game."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
from check_pool_regions_trial import CORE


class PoolRegions(unittest.TestCase):
    def test_ranges_aliases_and_completion_lifetime(self):
        compiler = shutil.which('clang++') or shutil.which('c++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        with tempfile.TemporaryDirectory(prefix='pool-regions-') as directory:
            source = Path(directory) / 'fixture.cpp'
            binary = Path(directory) / 'fixture'
            source.write_text(CORE)
            subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-I' + str(TOOLS.parent / 'runtime/src'), str(source), '-o', str(binary)],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)
