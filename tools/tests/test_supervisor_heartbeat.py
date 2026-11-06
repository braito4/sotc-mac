"""Synthetic subprocess controls; never initializes graphics or reads game data."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]

class SupervisorHeartbeatTests(unittest.TestCase):
    def test_silence_notice_does_not_extend_deadline(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder)/'quiet.log'
            r = subprocess.run([sys.executable,str(ROOT/'tools/run_gpu_diagnostic.py'),
                '--seconds','16','--log',str(log),'--',sys.executable,'-c',
                'import time; time.sleep(60)'],capture_output=True,text=True,timeout=24,
                env=dict(os.environ,SOTC_SUPERVISOR_HEARTBEAT='1',SOTC_GPU_UTILIZATION='0'))
            self.assertEqual(r.returncode,21,r.stderr)
            text=log.read_text()
            self.assertIn('GPU supervisor status: elapsed=15s child_output_silent=15s',text)
            self.assertIn('GPU progress unknown',text)
            self.assertIn('STOP_GPU_WALL_TIMEOUT',text)

    def test_gpu_stall_still_stops_immediately(self):
        with tempfile.TemporaryDirectory() as folder:
            log=Path(folder)/'stall.log'
            r=subprocess.run([sys.executable,str(ROOT/'tools/run_gpu_diagnostic.py'),
                '--seconds','30','--log',str(log),'--',sys.executable,'-c',
                'import time; print("GPU watchdog: synthetic",flush=True); time.sleep(60)'],
                capture_output=True,text=True,timeout=6,
                env=dict(os.environ,SOTC_SUPERVISOR_HEARTBEAT='1',SOTC_GPU_UTILIZATION='0'))
            self.assertEqual(r.returncode,31,r.stderr)
            self.assertIn('STOP_GPU_STALL',log.read_text())
            self.assertNotIn('GPU supervisor status:',log.read_text())
