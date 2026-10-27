"""Actual bbcpu probe API and protected field observations, no game input."""
import os
from pathlib import Path
import platform
import re
import shutil
import select
import time
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(platform.system() == 'Darwin' and platform.machine() == 'arm64'
                     and shutil.which('clang'), 'native ARM64 host required')
class CpuFieldTraceTests(unittest.TestCase):
    def test_fiber_failure_retains_context_and_frozen_history_without_blocking(self):
        with tempfile.TemporaryDirectory(prefix='fiber history ',dir=ROOT/'work') as folder:
            binary=self.compile_fixture(folder)
            for mode in ('1','0'):
                for shape in ('inside','outside','invalid_bounds'):
                    env=dict(os.environ,BB_JIT=mode,SOTC_TEST_FIBER_FAILURE='1')
                    if shape=='outside':env['SOTC_TEST_FIBER_BAD_RSP']='1'
                    if shape=='invalid_bounds':env['SOTC_TEST_FIBER_BAD_BOUNDS']='1'
                    run=subprocess.run([str(binary)],env=env,capture_output=True,text=True,timeout=3)
                    self.assertEqual(run.returncode,23,run.stderr)
                    self.assertIn('observed=0x4321 expected=0x4320',run.stderr)
                    self.assertIn('bounds_valid=0x0' if shape=='invalid_bounds' else 'bounds_valid=0x1',run.stderr)
                    self.assertIn('rsp_in_bounds=0x1' if shape=='inside' else 'rsp_in_bounds=0x0',run.stderr)
                    self.assertLess(run.stderr.index('Fiber stack failure:'),run.stderr.index('CPU field trace dump:'))
                    self.assertIn('frozen=0x1 newest_first=0x1',run.stderr)
                    self.assertIn('CPU field trace summary: attempted=0x12c retained=0x12c',run.stderr)
                for output in ('full','closed'):
                    read,write=os.pipe();os.set_blocking(write,False)
                    if output=='full':
                        try:
                            while True:os.write(write,b'x'*512)
                        except BlockingIOError:pass
                    else:os.close(read)
                    try:
                        run=subprocess.run([str(binary)],env=dict(os.environ,BB_JIT=mode,SOTC_TEST_FIBER_FAILURE='1'),
                            stderr=write,stdout=subprocess.DEVNULL,timeout=1)
                        self.assertEqual(run.returncode,23)
                    finally:
                        os.close(write)
                        if output=='full':os.close(read)

    def test_nested_fatal_minimum_and_output_backpressure(self):
        with tempfile.TemporaryDirectory(prefix='fatal minimum ',dir=ROOT/'work') as folder:
            binary=self.compile_fixture(folder)
            for mode in ('1','0'):
                for output in ('drained','full','closed'):
                    env=dict(os.environ,BB_JIT=mode,SOTC_TEST_FATAL_MINIMUM='1')
                    if output=='drained':
                        p=subprocess.run([str(binary)],env=env,capture_output=True,text=True,timeout=3)
                        self.assertEqual(p.returncode,139,p.stderr)
                        self.assertEqual(p.stderr.count('CPU fatal minimum:'),2)
                        reporters=re.findall(r'reporter=(0x[0-9a-f]+)',p.stderr)
                        self.assertEqual(len(reporters),2)
                        self.assertEqual(reporters[0],reporters[1])
                        self.assertIn('sample=0x1 seq=0x12c',p.stderr)
                        self.assertIn('value=0x8765432101234567',p.stderr)
                    else:
                        read,write=os.pipe();os.set_blocking(write,False)
                        if output=='full':
                            try:
                                while True:os.write(write,b'x'*512)
                            except BlockingIOError:pass
                        else:os.close(read)
                        began=time.monotonic()
                        try:
                            p=subprocess.run([str(binary)],env=env,stderr=write,stdout=subprocess.DEVNULL,timeout=1)
                            self.assertEqual(p.returncode,139)
                            self.assertLess(time.monotonic()-began,1)
                        finally:
                            os.close(write)
                            if output=='full':os.close(read)

    def compile_fixture(self, folder):
        binary = Path(folder)/'test'
        subprocess.run(['clang', '-arch', 'arm64', '-O1', '-g', '-std=gnu11',
            '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined,bounds',
            '-I'+str(ROOT/'runtime/src'), '-I'+str(ROOT/'runtime/upstream/src'),
            '-I'+str(ROOT/'runtime/upstream/recomp/src/cpu'),
            '-I'+str(ROOT/'work/deps-arm64/include'),
            str(ROOT/'tests/test_cpu_field_trace.c'),
            str(ROOT/'runtime/src/platform_arm64.c'),
            str(ROOT/'runtime/src/bootstrap_cpu_context.c'),
            str(ROOT/'runtime/src/bootstrap_cpu_vector.c'),
            str(ROOT/'work/libbbcpu-context.a'),
            str(ROOT/'work/deps-arm64/lib/libZydis.a'),
            str(ROOT/'work/deps-arm64/lib/libZycore.a'), '-o', str(binary)],
            check=True, capture_output=True, text=True)
        return binary

    def test_installed_probes_and_bounded_ring_in_both_modes(self):
        with tempfile.TemporaryDirectory(prefix='field trace ', dir=ROOT/'work') as folder:
            binary = self.compile_fixture(folder)
            for mode in ('1', '0'):
                env = dict(os.environ, BB_JIT=mode)
                result = subprocess.run([str(binary)], env=env, capture_output=True,
                                        text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
                self.assertIn('PASS:', result.stdout)

    def fixture(self, path):
        image = bytearray(16384)
        # Original synthetic object, setter call and deliberate terminating read.
        image[:20] = bytes.fromhex('4c8d25') + struct.pack('<i', 512-7) + bytes.fromhex(
            '4c89e7 e831000000 31c0 488b00')
        image[64:73] = bytes.fromhex('48c7471822000000c3')
        path.write_bytes(struct.pack('<8s6Q', b'BBPROBE2', len(image), 0, 2, 0, 0, 0)
                         + struct.pack('<6Q', 0, 256, 5, 256, len(image)-256, 6) + image)

    def test_cooperative_history_budget_and_output_backpressure(self):
        with tempfile.TemporaryDirectory(prefix='field cooperative ', dir=ROOT/'work') as folder:
            binary = self.compile_fixture(folder)
            for mode in ('1', '0'):
                for output in ('drained', 'full', 'closed'):
                    control_read, control_write = os.pipe()
                    output_read = output_write = None
                    if output == 'full':
                        output_read, output_write = os.pipe(); os.set_blocking(output_write, False)
                        for size in (4096, 1):
                            try:
                                while True: os.write(output_write, b'x'*size)
                            except BlockingIOError:
                                pass
                        os.set_blocking(output_write, True)
                    env = dict(os.environ, BB_JIT=mode, SOTC_TEST_COOPERATIVE='1',
                               SOTC_DIAGNOSTIC_CONTROL_FD=str(control_read))
                    child = subprocess.Popen([str(binary)], env=env, pass_fds=(control_read,),
                        stdout=subprocess.PIPE, stderr=output_write if output == 'full' else subprocess.PIPE)
                    os.close(control_read)
                    if output_write is not None: os.close(output_write)
                    try:
                        if output != 'full':
                            self.assertTrue(select.select([child.stdout], [], [], 5)[0])
                            self.assertEqual(child.stdout.readline(), b'FIELD_TRACE_READY\n')
                        if output == 'closed': child.stderr.close()
                        began = time.monotonic(); os.write(control_write, b'S')
                        if output == 'drained':
                            _, errors = child.communicate(timeout=3)
                            self.assertEqual(errors.count(b'CPU field trace event:'), 128)
                            self.assertIn(b'event_limit=0x80 budget_omitted=0xac', errors)
                            self.assertIn(b'frozen=0x1', errors)
                            self.assertIn(b'newest_first=0x1',errors)
                            sequences=[int(v,16) for v in re.findall(rb'CPU field trace event: seq=(0x[0-9a-f]+)',errors)]
                            self.assertEqual(sequences,list(range(300,172,-1)))
                            first=next(line for line in errors.splitlines() if line.startswith(b'CPU field trace event:'))
                            self.assertIn(b'value=0x12c',first)
                            self.assertIn(b'attempted=0x12c retained=0x80', errors)
                            self.assertIn(b'budget_omitted=0xac', errors)
                        else:
                            # A full pipe stays undrained until after exit. A closed
                            # reader must preserve exit 21 rather than SIGPIPE.
                            child.wait(timeout=1)
                        self.assertEqual(child.returncode, 21, f'{mode}/{output}')
                        self.assertLess(time.monotonic()-began, 1)
                    finally:
                        os.close(control_write)
                        if output_read is not None:
                            os.set_blocking(output_read, False)
                            try:
                                while os.read(output_read, 65536): pass
                            except BlockingIOError:
                                pass
                            os.close(output_read)
                        if child.stderr and not child.stderr.closed: child.stderr.close()
                        child.wait(timeout=3)
                        if child.stdout: child.stdout.close()

    @unittest.skipUnless((ROOT/'work/probe_entry_arm64').is_file(), 'built diagnostic required')
    def test_actual_fatal_dump_keeps_exit_and_history_in_both_modes(self):
        sys_path=str(ROOT/'tools')
        import sys
        if sys_path not in sys.path:sys.path.insert(0,sys_path)
        from analyze_event_journal import analyze
        with tempfile.TemporaryDirectory(prefix='field fatal ', dir=ROOT/'work') as folder:
            image = Path(folder)/'fixture.bin'; self.fixture(image)
            for mode in ('1', '0'):
                state=Path(folder)/('jit'+mode);state.mkdir()
                env = dict(os.environ, BB_JIT=mode, BB_DMEM_MB='16', BB_DMEM_EXTRA_MB='0',
                           SOTC_CPU_FIELD_TRACE='0x40:7:24,0xf:12:24',
                           SOTC_CPU_FAULT_FIELDS='', SOTC_DIAGNOSTIC_SECONDS='5',
                           SOTC_DIAGNOSTIC_JOURNAL='1',BB_GPU_USER_DIR=str(state))
                result = subprocess.run([str(ROOT/'work/probe_entry_arm64'), str(image)],
                                        env=env, capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 139, result.stdout+result.stderr)
                self.assertIn('2 configured block-entry probes', result.stdout)
                self.assertEqual(result.stderr.count('CPU field trace event:'), 2)
                self.assertIn(' value=0x0 status=0x7', result.stderr)
                self.assertIn(' value=0x22 status=0x7', result.stderr)
                self.assertIn('attempted=0x2 retained=0x2', result.stderr)
                self.assertIn('frozen=0x1', result.stderr)
                self.assertIn('STOP_EXECUTION_FAULT: SIGSEGV', result.stderr)
                self.assertLess(result.stderr.index('CPU fatal minimum:'),result.stderr.index('Host ARM64 context:'))
                minimum=next(line for line in result.stderr.splitlines() if line.startswith('CPU fatal minimum:'))
                self.assertIn('sample=0x1 seq=0x2',minimum)
                self.assertIn('value=0x22',minimum)
                evidence=analyze(state/'cpu-events.bin','cpu')
                self.assertEqual(evidence['state'],'readable')
                self.assertEqual(evidence['retained'],2)
                self.assertEqual([c['retained'] for c in evidence['channels']],[1,1])
                self.assertEqual({e['value'] for e in evidence['events']},{0,0x22})
                if mode=='1':
                    self.assertEqual(evidence['terminal_context']['kind'],2)
                    self.assertEqual(evidence['terminal_context']['source'],'translated_fault')
                    self.assertEqual(evidence['terminal_context']['available'],1)
                    self.assertEqual(evidence['terminal_context']['registers']['rax'],0)
                else:
                    self.assertEqual(evidence['terminal_context']['kind'],4)
                    self.assertEqual(evidence['terminal_context']['source'],'interpreted_fault')
                    self.assertEqual(evidence['terminal_context']['available'],1)
                    self.assertEqual(evidence['terminal_context']['registers']['rax'],0)
                # No stderr dump is needed for the persistent records.
                hidden=Path(folder)/('hidden'+mode);hidden.mkdir();env['BB_GPU_USER_DIR']=str(hidden)
                result=subprocess.run([str(ROOT/'work/probe_entry_arm64'),str(image)],
                    env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=10)
                self.assertEqual(result.returncode,139)
                self.assertEqual(analyze(hidden/'cpu-events.bin','cpu')['retained'],2)
                hidden_context=analyze(hidden/'cpu-events.bin','cpu')['terminal_context']
                self.assertEqual(hidden_context['kind'],2 if mode=='1' else 4)

    @unittest.skipUnless((ROOT/'work/probe_entry_arm64').is_file(), 'built diagnostic required')
    def test_filtered_history_in_actual_loader_both_modes(self):
        with tempfile.TemporaryDirectory(prefix='field filtered ', dir=ROOT/'work') as folder:
            image = Path(folder)/'fixture.bin'; self.fixture(image)
            for mode in ('1', '0'):
                for matched in (True, False):
                    offset = 512 if matched else 513
                    env = dict(os.environ, BB_JIT=mode, BB_DMEM_MB='16', BB_DMEM_EXTRA_MB='0',
                               SOTC_CPU_FIELD_TRACE=f'0x40:7:24:0:7:{offset},0xf:12:24:0:12:{offset}',
                               SOTC_CPU_FAULT_FIELDS='', SOTC_DIAGNOSTIC_SECONDS='5')
                    result = subprocess.run([str(ROOT/'work/probe_entry_arm64'), str(image)],
                                            env=env, capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode, 139, result.stdout+result.stderr)
                    self.assertEqual(result.stderr.count('CPU field trace event:'), 2 if matched else 0)
                    self.assertIn('filtered=0x0' if matched else 'filtered=0x2', result.stderr)
                    if matched:
                        self.assertIn(' value=0x0 status=0x7', result.stderr)
                        self.assertIn(' value=0x22 status=0x7', result.stderr)
                    self.assertIn('STOP_EXECUTION_FAULT: SIGSEGV', result.stderr)

    @unittest.skipUnless((ROOT/'work/probe_entry_arm64').is_file(), 'built diagnostic required')
    def test_invalid_config_and_nonexecutable_sites_stop_before_execution(self):
        with tempfile.TemporaryDirectory(prefix='field invalid ', dir=ROOT/'work') as folder:
            image = Path(folder)/'fixture.bin'; self.fixture(image)
            for config, expected in [('0x40:16:0', 'Invalid SOTC_CPU_FIELD_TRACE'),
                                     ('0x200:7:0', 'site is not executable'),
                                     ('0x4000:7:0', 'site outside image'),
                                     ('0x40:7:0:0:16:0', 'Invalid SOTC_CPU_FIELD_TRACE'),
                                     ('0x40:7:0:0:7:0x4000', 'match outside image')]:
                env = dict(os.environ, SOTC_CPU_FIELD_TRACE=config, SOTC_CPU_FAULT_FIELDS='')
                result = subprocess.run([str(ROOT/'work/probe_entry_arm64'), str(image)],
                                        env=env, capture_output=True, text=True, timeout=10)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(expected, result.stderr)
                self.assertNotIn('STOP_EXECUTION_FAULT', result.stderr)


if __name__ == '__main__':
    unittest.main()
