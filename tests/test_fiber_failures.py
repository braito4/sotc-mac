"""Actual synthetic guest fiber guard rejection, no game or GPU."""
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from analyze_diagnostic_run import summarize


class FiberFailureTests(unittest.TestCase):
    @unittest.skipUnless(platform.system()=='Darwin' and platform.machine()=='arm64',
                         'native ARM64 fixture required')
    def test_real_guard_mismatch_is_not_reported_as_rsp_exhaustion(self):
        binary=ROOT/'work/test_fibers'
        self.assertTrue(binary.is_file(),'build the synthetic fiber fixture first')
        for jit in ('1','0'):
            run=subprocess.run([str(binary)],env=dict(os.environ,BB_JIT=jit,
                SOTC_TEST_CORRUPT_FIBER_GUARD='1'),capture_output=True,text=True,timeout=5)
            self.assertEqual(run.returncode,23,run.stdout+run.stderr)
            self.assertIn('STOP: fiber guest stack overflow',run.stderr)
            self.assertIn('bounds_valid=0x1 rsp_in_bounds=0x1',run.stderr)
            match=re.search(r'observed=(0x[0-9a-f]+) expected=(0x[0-9a-f]+)',run.stderr)
            self.assertIsNotNone(match,run.stderr)
            self.assertEqual(int(match[1],16)^int(match[2],16),1)
            self.assertIn('name_hex='+b'worker A'.hex(),run.stderr)
            self.assertIn('  rax=',run.stderr)
            self.assertIn('  r8=',run.stderr)
            report=summarize(run.stderr)
            self.assertEqual(report['stops'][0]['message'],'STOP: fiber guest stack overflow')
            evidence=report['fiber_stack_failures'][0]
            identity=report['fiber_stack_identities'][0]
            self.assertEqual(evidence['fiber'],identity['fiber'])
            self.assertEqual(evidence['rsp_in_bounds'],1)
            self.assertEqual(identity['name_hex'],b'worker A'.hex().ljust(64,'0'))
            self.assertEqual(report['cpu']['fault_register_history']['state'],'single_snapshot')


if __name__=='__main__':unittest.main()
