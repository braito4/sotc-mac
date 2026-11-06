import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]

class PoolContiguousTest(unittest.TestCase):
    def test_real_query_header(self):
        with tempfile.TemporaryDirectory() as folder:
            exe = pathlib.Path(folder) / 'fixture'
            subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L',
                            '-Wall', '-Wextra', '-Werror', '-pthread',
                            '-I'+str(ROOT/'runtime/src'),
                            str(ROOT/'tools/tests/pool_contiguous_fixture.c'),
                            '-o', str(exe)], check=True)
            for enabled in ('0', '1'):
                subprocess.run([str(exe), enabled], check=True)
