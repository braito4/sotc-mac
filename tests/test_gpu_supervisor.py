"""Real child-process fixtures only: no renderer, GPU, controller or game input."""
from pathlib import Path
import os
import signal
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).parents[1]
RUNNER = ROOT/'tools/run_gpu_diagnostic.py'


@unittest.skipUnless(os.name == 'posix', 'POSIX process groups required')
class GpuSupervisorTests(unittest.TestCase):
    def run_fixture(self, source, seconds='5', arguments=()):
        with tempfile.TemporaryDirectory(prefix='GPU supervisor fixture ') as directory:
            log = Path(directory)/'private log.txt'
            started = time.monotonic()
            result = subprocess.run([sys.executable, str(RUNNER), '--seconds', seconds,
                '--log', str(log), '--', sys.executable, '-u', '-c', source, *arguments],
                capture_output=True, timeout=8)
            elapsed = time.monotonic()-started
            return result, log.read_bytes() if log.exists() else b'', elapsed

    def test_exact_output_arguments_and_nonzero_child_exit(self):
        result, log, _ = self.run_fixture(
            "import os,sys; os.write(1,b'out\\x00'); os.write(2,b'err\\n'); "
            "print(repr(sys.argv[1:])); sys.exit(20)", arguments=('two words', 'literal$`text`'))
        self.assertEqual(result.returncode, 20, result.stderr)
        self.assertEqual(log, result.stdout)
        self.assertIn(b'out\0err\n', log)
        self.assertIn(b"['two words', 'literal$`text`']", log)
        self.assertIn(b'GPU supervisor: finished', log)
        self.assertIn(b'child_returncode=20 exit=20', log)

    def test_natural_exit_with_large_output_has_a_conclusive_footer(self):
        result, log, _ = self.run_fixture("import os; os.write(1,b'payload\\n'*150000)")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(log.count(b'payload\n'), 150000)
        self.assertIn(b'child_returncode=0 exit=0', log)
        self.assertNotIn(b'STOP_GPU_WALL_TIMEOUT', log)

    def test_wall_stop_is_recorded_before_child_cleanup_output(self):
        source = '''import os,signal,sys,time
def stopped(signum,frame):
    os.write(1,b'CLEANUP_LAST_OUTPUT\\n')
    sys.exit(7)
signal.signal(signal.SIGTERM,stopped)
while True:
    os.write(1,b'busy output\\n'*300)
    time.sleep(.005)
'''
        result, log, _ = self.run_fixture(source, seconds='1')
        self.assertEqual(result.returncode, 21, result.stderr)
        self.assertLess(log.index(b'STOP_GPU_WALL_TIMEOUT'), log.index(b'CLEANUP_LAST_OUTPUT'))
        self.assertIn(b'child_returncode=7 exit=21', log)

    def test_cleanup_error_is_recorded_with_both_exit_statuses(self):
        source = '''import sys
sys.path.insert(0,sys.argv[1])
import run_gpu_diagnostic as runner
original=runner.stop_group
def failed_cleanup(child):
    original(child)
    raise OSError('synthetic cleanup error')
runner.stop_group=failed_cleanup
raise SystemExit(runner.supervise([sys.executable,'-c','pass'],5,sys.argv[2]))
'''
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory)/'cleanup.log'
            result = subprocess.run([sys.executable,'-c',source,str(ROOT/'tools'),str(log)],
                                    capture_output=True,timeout=5)
            self.assertEqual(result.returncode,1,result.stderr)
            data = log.read_bytes()
            self.assertIn(b'STOP_GPU_SUPERVISOR_CLEANUP: synthetic cleanup error',data)
            self.assertIn(b'child_returncode=0 exit=1',data)

    def test_silent_child_stops_at_parent_wall_deadline(self):
        result, log, elapsed = self.run_fixture('import time; time.sleep(30)', seconds='1')
        self.assertEqual(result.returncode, 21, result.stderr)
        self.assertIn(b'STOP_GPU_WALL_TIMEOUT', log)
        self.assertLess(elapsed, 4)

    def test_split_watchdog_message_stops_child_ignoring_termination(self):
        result, log, elapsed = self.run_fixture(
            "import os,signal,time; signal.signal(signal.SIGTERM,signal.SIG_IGN); "
            "os.write(1,b'GPU '); time.sleep(.05); os.write(1,b'watch'); "
            "time.sleep(.05); os.write(1,b'dog: stuck\\n'); time.sleep(30)")
        self.assertEqual(result.returncode, 31, result.stderr)
        self.assertIn(b'GPU watchdog: stuck', log)
        self.assertIn(b'STOP_GPU_STALL', log)
        self.assertLess(elapsed, 4)

    def test_shader_error_does_not_trigger_a_gpu_stall_stop(self):
        result, log, _ = self.run_fixture("print('PerVertexKHR decoration is not supported in MSL')")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn(b'STOP_GPU_STALL', log)

    def test_stalled_process_group_does_not_leave_a_worker_running(self):
        with tempfile.TemporaryDirectory() as directory:
            heartbeat = Path(directory)/'heartbeat'
            source = '''import os,signal,sys,time
pid=os.fork()
if pid==0:
    signal.signal(signal.SIGTERM,signal.SIG_IGN)
    while True:
        with open(sys.argv[1],'ab') as out: out.write(b'.')
        time.sleep(.03)
print('WORKER_PID='+str(pid),flush=True)
while not os.path.exists(sys.argv[1]): time.sleep(.01)
print('GPU watchdog: stalled fixture',flush=True)
time.sleep(30)
'''
            result, log, _ = self.run_fixture(source, arguments=(str(heartbeat),))
            pid = int(log.split(b'WORKER_PID=')[1].splitlines()[0])
            try:
                self.assertEqual(result.returncode, 31, result.stderr)
                size = heartbeat.stat().st_size
                time.sleep(.2)
                self.assertEqual(heartbeat.stat().st_size, size, 'worker survived group termination')
            finally:
                try:
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass

    def test_parent_cancellation_is_recorded(self):
        for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            with self.subTest(signal=sig):
                self.check_parent_cancellation(sig)

    def check_parent_cancellation(self, sig):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory)/'cancel.log'
            child = subprocess.Popen([sys.executable,str(RUNNER),'--seconds','5','--log',str(log),
                '--',sys.executable,'-u','-c',"import time; print('READY'); time.sleep(30)"],
                stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            try:
                self.assertIn(b'GPU supervisor:', child.stdout.readline())
                self.assertEqual(child.stdout.readline(), b'READY\n')
                child.send_signal(sig)
                _, errors = child.communicate(timeout=5)
                self.assertEqual(child.returncode, 128+sig, errors)
                self.assertIn(b'STOP_GPU_CANCELLED', log.read_bytes())
            finally:
                if child.poll() is None:
                    child.kill()
                child.communicate(timeout=5)

    def test_child_timeout_and_signal_status_are_preserved(self):
        for source, expected in (('raise SystemExit(21)', 21),
                ('import os,signal; os.kill(os.getpid(),signal.SIGTERM)', 128+signal.SIGTERM)):
            with self.subTest(expected=expected):
                result, log, _ = self.run_fixture(source)
                self.assertEqual(result.returncode, expected, result.stderr)
                self.assertNotIn(b'STOP_GPU_WALL_TIMEOUT', log)
                self.assertIn(b'GPU supervisor: finished', log)

    def test_invalid_budget_never_launches_child(self):
        for seconds in ('0', '1801', '-1', '', '1.5', ' 1', '\u0661'):
            with self.subTest(seconds=seconds):
                result, log, _ = self.run_fixture("print('MUST_NOT_RUN')", seconds=seconds)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(log, b'')
                self.assertNotIn(b'MUST_NOT_RUN', result.stdout)

    def test_existing_log_is_preserved_without_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory)/'existing.log'
            log.write_bytes(b'keep previous evidence\n')
            result = subprocess.run([sys.executable,str(RUNNER),'--seconds','1','--log',str(log),
                '--',sys.executable,'-c',"print('MUST_NOT_RUN')"],capture_output=True,timeout=5)
            self.assertEqual(result.returncode,1,result.stderr)
            self.assertEqual(log.read_bytes(),b'keep previous evidence\n')
            self.assertNotIn(b'MUST_NOT_RUN',result.stdout)


if __name__ == '__main__':
    unittest.main()
