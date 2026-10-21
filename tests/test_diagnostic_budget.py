"""Bounded CLI budgets are validated before reading an image or loading graphics."""
import os
from pathlib import Path
import platform
import subprocess
import struct
import tempfile
import unittest

ROOT = Path(__file__).parents[1]
PROBE = ROOT / 'work/probe_entry_arm64'


@unittest.skipUnless(platform.system() == 'Darwin' and PROBE.is_file(), 'native diagnostic required')
class DiagnosticBudgetTests(unittest.TestCase):
    def check(self, budget):
        return subprocess.run([str(PROBE), '/missing-synthetic-budget-input', '--bootstrap'],
                              env=dict(os.environ, SOTC_DIAGNOSTIC_SECONDS=budget),
                              capture_output=True, text=True, timeout=5)

    def test_extended_budgets_reach_image_validation(self):
        for budget in ('1', '30', '60', '61', '300', '600', '999', '1800', '0300'):
            with self.subTest(budget=budget):
                result = self.check(budget)
                self.assertIn('cannot open input', result.stderr)
                self.assertNotIn('must be', result.stderr)

    def test_disabled_unbounded_and_malformed_budgets_are_rejected(self):
        for budget in ('', '0', '0000', '-1', '+300', ' 300', '300 ', '1.5', '1801', '9999',
                       '10000', '99999999999999999999999999999999999999999', '３００'):
            with self.subTest(budget=budget):
                result = self.check(budget)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('SOTC_DIAGNOSTIC_SECONDS must be 1..1800', result.stderr)
                self.assertNotIn('cannot open input', result.stderr)

    def test_short_budget_still_stops_a_running_guest_in_both_modes(self):
        with tempfile.TemporaryDirectory(prefix='budget guest ') as directory:
            boot=Path(directory)/'loop.bin'
            image=bytes.fromhex('ebfe')+bytes(16382)  # Original synthetic infinite x86 jump.
            boot.write_bytes(struct.pack('<8s9Q',b'BBPROBE2',16384,0,1,0,0,0,0,16384,5)+image)
            for mode in ('0','1'):
                with self.subTest(mode=mode):
                    result=subprocess.run([str(PROBE),str(boot)],capture_output=True,text=True,timeout=8,
                        env=dict(os.environ,SOTC_DIAGNOSTIC_SECONDS='1',BB_JIT=mode,BB_DMEM_MB='16',BB_DMEM_EXTRA_MB='0'))
                    self.assertEqual(result.returncode,21,result.stdout+result.stderr)
                    self.assertIn('Diagnostic watchdog: 1 seconds',result.stdout)
                    self.assertIn('STOP_TIMEOUT: diagnostic budget exhausted',result.stderr)
