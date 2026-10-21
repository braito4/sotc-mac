"""Native CPU fixtures for the GPU submission journal; no graphics libraries/devices."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class SubmissionTraceTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++') and shutil.which('cc'), 'native C/C++ compilers required')
    def test_native_journal_and_strict_opt_in(self):
        with tempfile.TemporaryDirectory(prefix='submission fixture ') as temporary:
            binary = Path(temporary) / 'fixture'
            control = Path(temporary) / 'control.o'
            build = subprocess.run(['cc', '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror',
                                    '-pthread', '-I', str(ROOT / 'runtime/src'), '-c',
                                    str(ROOT / 'tests/gpu_submission_trace_control.c'), '-o', str(control)],
                                   capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            build = subprocess.run(['c++', '-std=c++20', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                                    '-pthread', '-I', str(ROOT / 'runtime/src'),
                                    str(ROOT / 'tests/gpu_submission_trace.cpp'), str(control), '-o', str(binary)],
                                   capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            for value in (None, '0', 'yes', '01', '1'):
                with self.subTest(value=value):
                    env = dict(os.environ)
                    env.pop('SOTC_GPU_SUBMIT_TRACE', None)
                    if value is not None: env['SOTC_GPU_SUBMIT_TRACE'] = value
                    run = subprocess.run([str(binary), '1' if value == '1' else '0'],
                                         env=env, capture_output=True, text=True, timeout=30)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                    if value == '1': self.assertIn('PASS:', run.stdout)
            log = Path(temporary) / 'synthetic-supervisor.log'
            run = subprocess.run([sys.executable, str(ROOT / 'tools/run_gpu_diagnostic.py'),
                                  '--seconds', '2', '--cooperative-stop', '--log', str(log),
                                  '--', str(binary), 'supervised'],
                                 env=dict(os.environ, SOTC_GPU_SUBMIT_TRACE='1'),
                                 capture_output=True, text=True, timeout=8)
            self.assertEqual(run.returncode, 31, run.stdout + run.stderr)
            text = log.read_text()
            self.assertLess(text.index('AWAITED tick=1'), text.index('GPU watchdog:'))
            self.assertIn('count=2,3,4', text)
            self.assertIn('child_returncode=21 exit=31', text)
