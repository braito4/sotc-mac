"""Real child-process fixtures only: no renderer, GPU, controller or game input."""
from pathlib import Path
import os
import signal
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).parents[1]
RUNNER = ROOT/'tools/run_gpu_diagnostic.py'


@unittest.skipUnless(os.name == 'posix', 'POSIX process groups required')
class GpuSupervisorTests(unittest.TestCase):
    def run_fixture(self, source, seconds='5', arguments=(), environment=None):
        with tempfile.TemporaryDirectory(prefix='GPU supervisor fixture ') as directory:
            log = Path(directory)/'private log.txt'
            started = time.monotonic()
            result = subprocess.run([sys.executable, str(RUNNER), '--seconds', seconds,
                '--log', str(log), '--', sys.executable, '-u', '-c', source, *arguments],
                capture_output=True, timeout=8,
                env=dict(os.environ, **(environment or {})))
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
        self.assertRegex(log, rb'GPU supervisor: child_pid=[0-9]+\n')

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
            self.assertIn(b'STOP_GPU_SUPERVISOR_CLEANUP:',data)
            self.assertIn(b'process-group cancellation: synthetic cleanup error',data)
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

    def test_flip_statistics_settings_are_logged_and_reach_the_child(self):
        import json
        with tempfile.TemporaryDirectory(prefix='flip statistics fixture ') as directory:
            frames = Path(directory)/'private frames.csv'
            result, log, _ = self.run_fixture(
                "import os,pathlib; assert os.environ['BB_FRAME_STATS']=='1'; "
                "pathlib.Path(os.environ['BB_FRAME_LOG']).write_text('synthetic flip fixture\\n')",
                environment={'BB_FRAME_STATS': '1', 'BB_FRAME_LOG': str(frames),
                             'BB_UPSCALE_PRESET': '4', 'BB_RENDER_RES': '',
                             'BB_OUTPUT_RES': '1920x1080',
                             'SYNTHETIC_PRIVATE_SECRET': 'must-not-be-in-the-log'})
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(frames.read_text(), 'synthetic flip fixture\n')
            settings = json.loads(log.splitlines()[0].split(b'settings=', 1)[1])
            self.assertEqual(settings['BB_FRAME_STATS'], '1')
            self.assertEqual(settings['BB_FRAME_LOG'], str(frames))
            self.assertEqual(settings['BB_UPSCALE_PRESET'], '4')
            self.assertEqual(settings['BB_RENDER_RES'], '')
            self.assertEqual(settings['BB_OUTPUT_RES'], '1920x1080')
            self.assertNotIn(b'must-not-be-in-the-log', log)

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
                self.assertRegex(child.stdout.readline(),rb'GPU supervisor: child_pid=[0-9]+\n')
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


@unittest.skipUnless(os.name == 'posix' and shutil.which('cc'), 'native C compiler required')
class CooperativeDiagnosticTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix='cooperative native fixture ')
        cls.root = Path(cls.temporary.name)
        source = cls.root/'fixture.c'
        cls.binary = cls.root/'fixture'
        source.write_text('''#include "diagnostic_control.h"
#include <signal.h>
int main(void) {
    if (sotc_diagnostic_control_start()) return 28;
    signal(SIGTERM,SIG_IGN);
    for (;;) pause();
}
''')
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-pthread',
            '-I'+str(ROOT/'runtime/src'),str(source),'-o',str(cls.binary)],
            check=True,capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_native_child_accepts_cooperative_deadline_despite_ignored_term(self):
        log = self.root/'deadline.log'
        result = subprocess.run([sys.executable,str(RUNNER),'--seconds','1','--log',str(log),
            '--cooperative-stop','--',str(self.binary)],capture_output=True,timeout=5)
        self.assertEqual(result.returncode,21,result.stderr)
        data = log.read_bytes()
        self.assertIn(b'stop_mode=cooperative',data)
        self.assertIn(b'STOP_GPU_WALL_TIMEOUT',data)
        self.assertIn(b'STOP_DIAGNOSTIC_CONTROL: parent requested termination',data)
        self.assertIn(b'child_returncode=21 exit=21',data)
        self.assertRegex(data,rb'control_notice_seen=1 control_notice_after_request_ms=[0-9.]+')
        self.assertNotIn(b'STOP_GPU_SUPERVISOR_CLEANUP',data)

    def test_notice_is_distinct_from_observed_child_exit(self):
        log = self.root/'notice-with-delayed-exit.log'
        finished = self.root/'delayed-fixture-finished'
        source = '''import os,pathlib,sys,time
fd=int(os.environ['SOTC_DIAGNOSTIC_CONTROL_FD'])
os.read(fd,1)
os.write(2,b'STOP_DIAGNOSTIC_CONTROL: parent requested ')
time.sleep(.02)
os.write(2,b'termination\\n')
time.sleep(2.5)
pathlib.Path(sys.argv[1]).write_text('synthetic child reached self-exit')
os._exit(21)
'''
        result = subprocess.run([sys.executable,str(RUNNER),'--seconds','1','--log',str(log),
            '--cooperative-stop','--',sys.executable,'-c',source,str(finished)],
            capture_output=True,timeout=6)
        self.assertEqual(result.returncode,1,result.stderr)
        data = log.read_bytes()
        self.assertIn(b'child_returncode=None exit=1',data)
        self.assertIn(b'cooperative stop: child exit not observed',data)
        self.assertRegex(data,rb'control_notice_seen=1 control_notice_after_request_ms=[0-9.]+')
        # The fixture finishes itself shortly after the supervisor's allowance;
        # do not signal it or leave a delayed writer behind for another test.
        deadline = time.monotonic()+3
        while not finished.exists() and time.monotonic()<deadline:
            time.sleep(.01)
        self.assertTrue(finished.exists())

    def test_parent_drains_queued_output_during_cooperative_stop(self):
        log = self.root/'queued-output.log'
        source = '''import os
fd=int(os.environ['SOTC_DIAGNOSTIC_CONTROL_FD'])
os.read(fd,1)
os.write(1,b'queued synthetic output\\n'*50000)
os.write(2,b'SYNTHETIC_STOP_FINISHED\\n')
os._exit(21)
'''
        result = subprocess.run([sys.executable,str(RUNNER),'--seconds','1','--log',str(log),
            '--cooperative-stop','--',sys.executable,'-c',source],capture_output=True,timeout=6)
        self.assertEqual(result.returncode,21,result.stderr)
        data = log.read_bytes()
        self.assertEqual(data.count(b'queued synthetic output\n'),50000)
        self.assertIn(b'SYNTHETIC_STOP_FINISHED',data)
        self.assertIn(b'child_returncode=21 exit=21',data)
        self.assertNotIn(b'STOP_GPU_SUPERVISOR_CLEANUP',data)

    def test_native_cancellation_exits_with_a_full_output_pipe(self):
        control_read, control_write = os.pipe()
        output_read, output_write = os.pipe()
        # Fill the inherited diagnostic output before launch. No reader drains
        # it until after the exit assertion, so a blocking stop message deadlocks.
        os.set_blocking(output_write,False)
        for size in (4096, 1):
            try:
                while True:
                    os.write(output_write,b'x'*size)
            except BlockingIOError:
                pass
        os.set_blocking(output_write,True)
        child = subprocess.Popen([str(self.binary)],pass_fds=(control_read,),
            env=dict(os.environ,SOTC_DIAGNOSTIC_CONTROL_FD=str(control_read)),
            stdout=subprocess.DEVNULL,stderr=output_write)
        os.close(control_read)
        os.close(output_write)
        try:
            os.write(control_write,b'S')
            self.assertEqual(child.wait(timeout=1),21)
        finally:
            os.close(control_write)
            # Release a broken fixture without external signals or leaving a
            # process behind, including when this regression fails before a fix.
            os.set_blocking(output_read,False)
            try:
                while os.read(output_read,65536):
                    pass
            except BlockingIOError:
                pass
            child.wait(timeout=3)
            os.close(output_read)

    def test_parent_pipe_closure_stops_native_child(self):
        read_fd, write_fd = os.pipe()
        child = subprocess.Popen([str(self.binary)],pass_fds=(read_fd,),
            env=dict(os.environ,SOTC_DIAGNOSTIC_CONTROL_FD=str(read_fd)),
            stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        os.close(read_fd)
        os.close(write_fd)
        _, errors = child.communicate(timeout=5)
        self.assertEqual(child.returncode,21,errors)
        self.assertIn(b'parent channel closed',errors)

    def test_native_child_handles_parent_cancellation(self):
        for sig in (signal.SIGINT, signal.SIGTERM):
            with self.subTest(signal=sig):
                log = self.root/('cancel-'+str(sig)+'.log')
                parent = subprocess.Popen([sys.executable,str(RUNNER),'--seconds','5',
                    '--log',str(log),'--cooperative-stop','--',str(self.binary)],
                    stdout=subprocess.PIPE,stderr=subprocess.PIPE)
                try:
                    self.assertIn(b'stop_mode=cooperative',parent.stdout.readline())
                    for _ in range(4):
                        ready = parent.stdout.readline()
                        if b'parent cancellation channel ready' in ready:
                            break
                    self.assertIn(b'parent cancellation channel ready',ready)
                    parent.send_signal(sig)
                    _, errors = parent.communicate(timeout=5)
                    self.assertEqual(parent.returncode,128+sig,errors)
                    data = log.read_bytes()
                    self.assertIn(b'parent requested termination',data)
                    self.assertIn(f'child_returncode=21 exit={128+sig}'.encode(),data)
                    self.assertNotIn(b'STOP_GPU_SUPERVISOR_CLEANUP',data)
                finally:
                    if parent.poll() is None:
                        parent.kill()
                    parent.communicate(timeout=5)

    def test_invalid_control_fd_fails_before_startup(self):
        for value in ('','0','1','2','-1','bad','2147483648','99999999999999999999'):
            with self.subTest(value=value):
                result = subprocess.run([str(self.binary)],capture_output=True,timeout=5,
                    env=dict(os.environ,SOTC_DIAGNOSTIC_CONTROL_FD=value))
                self.assertEqual(result.returncode,28,result.stderr)
                self.assertIn(b'STOP_DIAGNOSTIC_CONTROL: invalid cancellation pipe',result.stderr)


if __name__ == '__main__':
    unittest.main()
