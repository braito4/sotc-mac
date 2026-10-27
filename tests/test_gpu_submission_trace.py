"""Native CPU fixtures for the GPU submission journal; no graphics libraries/devices."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from analyze_gpu_submission import analyze as analyze_submissions

class SubmissionTraceTests(unittest.TestCase):
    def test_standalone_metal_failure_does_not_certify_progress(self):
        log = """GPU submission trace: FAILED_METAL chunk=7 retained=1 buffer=0x10 recording=9 status=5 error=1
GPU submission trace: Metal chunk=7 scheduled_callback=1 completed_callback=1
GPU submission trace: Metal progress chunk=7 available=1 reached=26 encoded=26 event=0x20
GPU render trace: buffer=0x10 recording=9 checkpoint=2 progress_event=0x20 draws=1
GPU render trace: draw=1 kind=3 indirect=0x30 offset=0
"""
        report = analyze_submissions(log)['reports'][0]
        self.assertEqual(report['kind'], 'native_failure')
        batch = report['submissions'][0]
        self.assertEqual(batch['buffers'][0]['chunks'][0]['progress']['reached'], 26)
        self.assertEqual(batch['render_scopes'][0]['completion'], 'failed_command_progress_unreliable')
        self.assertEqual(len(batch['render_scopes'][0]['draw_details']), 1)

    @unittest.skipUnless(shutil.which('c++'), 'native C++ compiler required')
    def test_failed_command_preserves_optional_cpu_history(self):
        with tempfile.TemporaryDirectory(prefix='command history ') as temporary:
            binary=Path(temporary)/'fixture'
            subprocess.run(['c++','-std=c++20','-O1','-Wall','-Wextra','-Werror','-pthread',
                '-DSOTC_TEST_HISTORY_HOOK','-I'+str(ROOT/'runtime/src'),
                str(ROOT/'tests/gpu_command_failure.cpp'),'-o',str(binary)]+
                (['-Wl,-export_dynamic'] if sys.platform=='darwin' else []),check=True)
            for case in ('success','error'):
                run=subprocess.run([str(binary),case],capture_output=True,text=True,timeout=5,
                    env=dict(os.environ,SOTC_GPU_SUBMIT_TRACE='0',SOTC_MEMORY_TRACE='0',SOTC_VISUAL_TRACE='0'))
                self.assertEqual(run.returncode,0 if case=='success' else 23,run.stderr)
                self.assertEqual(run.stderr.count('SYNTHETIC_CPU_HISTORY'),0 if case=='success' else 1)

    @unittest.skipUnless(shutil.which('c++'), 'native C++ compiler required')
    def test_failed_command_stops_with_or_without_journal(self):
        with tempfile.TemporaryDirectory(prefix='command failure ') as temporary:
            binary=Path(temporary)/'fixture'
            subprocess.run(['c++','-std=c++20','-O1','-Wall','-Wextra','-Werror','-pthread',
                            '-I'+str(ROOT/'runtime/src'),str(ROOT/'tests/gpu_command_failure.cpp'),
                            '-o',str(binary)],check=True)
            for trace in ('0','1'):
                for case in ('success','error','unknown'):
                    with self.subTest(trace=trace,case=case):
                        state=Path(temporary)/(trace+'-'+case);state.mkdir()
                        run=subprocess.run([str(binary),case],capture_output=True,text=True,timeout=5,
                                           env=dict(os.environ,SOTC_GPU_SUBMIT_TRACE=trace,
                                                    BB_GPU_USER_DIR=str(state),SOTC_MEMORY_TRACE='1'))
                        self.assertEqual(run.returncode,0 if case=='success' else 23,run.stderr)
                        if case=='success':
                            self.assertNotIn('STOP_',run.stderr)
                            self.assertFalse((state/'memory').exists())
                        else:
                            self.assertIn('STOP_GPU_METAL_COMMAND: status=5',run.stderr)
                            self.assertIn('allocated_bytes=123456 recommended_working_set_bytes=654321',run.stderr)
                            self.assertIn('code=123' if case=='unknown' else 'code=8',run.stderr)
                            self.assertIn('domain=unavailable' if case=='unknown' else 'MTLCommandBufferErrorDomain',run.stderr)
                            self.assertEqual('FAILED_METAL' in run.stderr,trace=='1')
                            if trace=='1': self.assertIn('count=2,3,4',run.stderr)
                            failure=json.loads((state/'memory/summary.jsonl').read_text().splitlines()[-1])
                            self.assertEqual(failure['event'],'memory_failure')
                            self.assertEqual(failure['code'],123 if case=='unknown' else 8)
                            self.assertEqual(failure['native_allocated_bytes'],123456)

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
                    if value == '1':
                        self.assertIn('PASS:', run.stdout)
                        progress = analyze_submissions(run.stdout)['reports'][0]['submissions'][0]['buffers'][0]['chunks'][0]['progress']
                        self.assertEqual(progress['encoded'], 26)
                        self.assertEqual(progress['reached'], 7)
                        self.assertEqual(progress['event'], 999)
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
            # Consume actual production-formatted output, never a game capture.
            report = analyze_submissions(text)['reports'][0]
            batch = report['submissions'][0]
            self.assertEqual(batch['tick'], 1)
            encoding = batch['native_encoding']
            self.assertEqual(encoding['retained'], 1)
            event = encoding['events'][0]
            self.assertEqual(event['entry'], 'cs_indirect_fixture')
            self.assertEqual(event['pipeline'], 6)
            self.assertEqual(event['backend_pipeline'], 9)
            self.assertEqual([event[k] for k in ('source','destination','source_offset','destination_offset','bytes','source_length','destination_length')], [101,202,16,32,64,128,256])
            self.assertEqual(event['indirect'], 7)
            self.assertEqual(event['offset'], 48)
            self.assertEqual(event['groups'], [0, 0, 0])  # indirect dimensions were not read
            self.assertEqual(event['threads'], [32, 1, 1])
            self.assertEqual(batch['waits'], [dict(semaphore=66, value=7)])
            self.assertEqual(batch['signals'], [dict(semaphore=55, value=1)])
            self.assertEqual(batch['observed_chunks'], 0)
            self.assertIsNone(batch['native_allocated_bytes_range'])
            self.assertTrue(any('callbacks absent' in gap for gap in report['evidence_gaps']))
