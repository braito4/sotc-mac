"""Portable CI coverage for the production transfer planner, without Metal/game input."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
from check_small_copy_trial import CORE


class SmallCopyPolicy(unittest.TestCase):
    def test_production_planner_against_independent_overlap_oracle(self):
        compiler = shutil.which('clang++') or shutil.which('c++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        with tempfile.TemporaryDirectory(prefix='small-copy-policy-') as directory:
            root = Path(directory)
            source = root/'fixture.cpp'
            executable = root/'fixture'
            source.write_text(CORE)
            subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-I'+str(TOOLS.parent/'runtime/src'), str(source), '-o', str(executable)],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(executable)], check=True, capture_output=True, text=True)
