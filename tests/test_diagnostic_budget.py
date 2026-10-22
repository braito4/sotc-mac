"""Optional duration limits and cancellation, using synthetic guests without graphics."""
import os
import json
from pathlib import Path
import platform
import signal
import shutil
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
        for budget in ('0', '0000', '1', '30', '60', '61', '300', '600', '999', '1800', '0300'):
            with self.subTest(budget=budget):
                result = self.check(budget)
                self.assertIn('cannot open input', result.stderr)
                self.assertNotIn('must be', result.stderr)

    def test_out_of_range_and_malformed_budgets_are_rejected(self):
        for budget in ('', '-1', '+300', ' 300', '300 ', '1.5', '1801', '9999',
                       '10000', '99999999999999999999999999999999999999999', '３００'):
            with self.subTest(budget=budget):
                result = self.check(budget)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('SOTC_DIAGNOSTIC_SECONDS must be 0..1800', result.stderr)
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

    def test_parent_cooperatively_stops_real_guest_before_its_own_alarm(self):
        with tempfile.TemporaryDirectory(prefix='cooperative guest ') as directory:
            root = Path(directory)
            boot = root/'loop.bin'
            image = bytes.fromhex('ebfe')+bytes(16382)
            boot.write_bytes(struct.pack('<8s9Q',b'BBPROBE2',16384,0,1,0,0,0,0,16384,5)+image)
            for mode in ('0','1'):
                for budget in (None, '0', '10'):
                    with self.subTest(mode=mode, budget=budget):
                        self.check_parent_stop(root, boot, mode, budget)

    def check_parent_stop(self, root, boot, mode, budget):
        log = root/('jit'+mode+'-'+str(budget)+'.log')
        env = dict(os.environ, BB_JIT=mode, BB_DMEM_MB='16', BB_DMEM_EXTRA_MB='0')
        env.pop('SOTC_DIAGNOSTIC_SECONDS', None)
        if budget is not None:
            env['SOTC_DIAGNOSTIC_SECONDS'] = budget
        result = subprocess.run([os.sys.executable,str(ROOT/'tools/run_gpu_diagnostic.py'),
            '--seconds','1','--log',str(log),'--cooperative-stop','--',str(PROBE),str(boot)],
            capture_output=True,text=True,timeout=6,env=env)
        self.assertEqual(result.returncode,21,result.stdout+result.stderr)
        data = log.read_text()
        self.assertIn('STOP_DIAGNOSTIC_CONTROL: parent requested termination',data)
        self.assertIn('child_returncode=21 exit=21',data)
        self.assertNotIn('STOP_TIMEOUT: diagnostic budget exhausted',data)
        expected = ('Diagnostic duration limit: none' if budget == '0' else
                    'Diagnostic watchdog: '+(budget or '30')+' seconds')
        self.assertIn(expected, data)

    def test_unlimited_native_guest_stays_alive_until_manual_parent_cancellation(self):
        with tempfile.TemporaryDirectory(prefix='unlimited guest ') as directory:
            root = Path(directory)
            boot = root/'loop.bin'
            image = bytes.fromhex('ebfe')+bytes(16382)
            boot.write_bytes(struct.pack('<8s9Q',b'BBPROBE2',16384,0,1,0,0,0,0,16384,5)+image)
            for mode in ('0','1'):
                with self.subTest(mode=mode):
                    log = root/('manual-jit'+mode+'.log')
                    parent = subprocess.Popen([os.sys.executable,str(ROOT/'tools/run_gpu_diagnostic.py'),
                        '--seconds','0','--log',str(log),'--cooperative-stop','--',str(PROBE),str(boot)],
                        stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                        env=dict(os.environ,SOTC_DIAGNOSTIC_SECONDS='0',BB_JIT=mode,
                                 BB_DMEM_MB='16',BB_DMEM_EXTRA_MB='0'))
                    try:
                        with self.assertRaises(subprocess.TimeoutExpired):
                            parent.communicate(timeout=1.2)
                        parent.send_signal(signal.SIGINT)
                        out, errors = parent.communicate(timeout=5)
                        self.assertEqual(parent.returncode,130,out+errors)
                        data = log.read_text()
                        self.assertIn('Diagnostic duration limit: none',data)
                        self.assertIn('STOP_GPU_CANCELLED',data)
                        self.assertIn('child_returncode=21 exit=130',data)
                        self.assertNotIn('STOP_TIMEOUT:',data)
                        self.assertNotIn('STOP_GPU_WALL_TIMEOUT',data)
                    finally:
                        if parent.poll() is None:
                            parent.terminate()
                        parent.communicate(timeout=5)


@unittest.skipUnless(platform.system() == 'Darwin' and platform.machine() == 'arm64',
                     'native ARM64 shell required')
class LauncherBudgetTests(unittest.TestCase):
    def test_game_budget_reaches_both_layers_without_disabling_metal_budget(self):
        with tempfile.TemporaryDirectory(prefix='launcher without GPU ') as directory:
            root = Path(directory)
            script = root/'packaging/test-arm64.sh'
            script.parent.mkdir()
            shutil.copy2(ROOT/'packaging/test-arm64.sh', script)
            # Only orchestration runs: all compiler/loader/renderer commands are
            # synthetic, and the recording stub never executes a GPU command.
            for name in ('runtime/upstream/recomp/out/libbbcpu.a',
                         'work/deps-arm64/lib/libZydis.a', 'work/deps-arm64/lib/libZycore.a',
                         'work/gpu-source-22923f080229/deps/macos-arm64/lib/libvulkan.1.dylib',
                         'work/gpu-source-22923f080229/deps/macos-arm64/lib/libMoltenVK.dylib',
                         'work/gpu-source-22923f080229/deps/macos-arm64/share/vulkan/icd.d/MoltenVK_icd.json'):
                path = root/name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('synthetic fixture only')
            stubs = root/'stubs'
            stubs.mkdir()
            sources = {
                'clang': '#!/bin/sh\nexit 0\n',
                'lipo': '#!/bin/sh\necho arm64\n',
                'ar': '#!/bin/sh\nif [ "$1" = -t ]; then printf "cpu.c.o\\ninterp_vec.c.o\\n"; fi\n',
                'python3': '#!'+os.sys.executable+'\n'
                           'import json,os,sys\n'
                           'with open(os.environ["SOTC_FIXTURE_CALLS"],"a") as out:\n'
                           '    out.write(json.dumps({"args":sys.argv[1:],'
                           '"budget":os.getenv("SOTC_DIAGNOSTIC_SECONDS"),'
                           '"visual":os.getenv("SOTC_VISUAL_TRACE"),'
                           '"audio":os.getenv("SOTC_AUDIO_TRACE"),'
                           '"capture":os.getenv("SOTC_VISUAL_CAPTURE")})+"\\n")\n'
                           'if "run_gpu_diagnostic.py" in sys.argv[1] and "--bootstrap" in sys.argv:\n'
                           '    os.mkdir(os.path.join(os.environ["BB_GPU_USER_DIR"],"visual"))\n'
                           '    sys.exit(int(os.getenv("SOTC_FIXTURE_EXIT","0")))\n'
                           'if "analyze_visual_trace.py" in sys.argv[1]:\n'
                           '    sys.exit(int(os.getenv("SOTC_FIXTURE_REPORT_EXIT","0")))\n',
            }
            for name, source in sources.items():
                stub = stubs/name
                stub.write_text(source)
                stub.chmod(0o755)
            for configured in (None, '0', '120', '1800'):
                with self.subTest(configured=configured):
                    calls = root/('calls-'+str(configured)+'.jsonl')
                    env = dict(os.environ, PATH=str(stubs)+':/usr/bin:/bin',
                               SOTC_FIXTURE_CALLS=str(calls), BB_DIRECT_MEMORY_ACCESS='0')
                    env.pop('SOTC_DIAGNOSTIC_SECONDS', None)
                    env.pop('BB_DEPS', None)
                    env.pop('SOTC_VISUAL_TRACE', None)
                    env.pop('SOTC_VISUAL_CAPTURE', None)
                    env.pop('SOTC_AUDIO_TRACE', None)
                    if configured == '120':
                        env['SOTC_VISUAL_CAPTURE'] = '1'
                    if configured == '1800':
                        env['SOTC_VISUAL_TRACE'] = '0'
                        env['SOTC_AUDIO_TRACE'] = '0'
                        env['SOTC_FIXTURE_EXIT'] = '23'
                        env['SOTC_FIXTURE_REPORT_EXIT'] = '9'
                    if configured is not None:
                        env['SOTC_DIAGNOSTIC_SECONDS'] = configured
                    result = subprocess.run(['/bin/bash',str(script),'--graphics-game-test'],
                        env=env,capture_output=True,text=True,timeout=10)
                    self.assertEqual(result.returncode,23 if configured == '1800' else 0,result.stdout+result.stderr)
                    supervised = [json.loads(line) for line in calls.read_text().splitlines()
                                  if 'run_gpu_diagnostic.py' in line]
                    self.assertEqual(len(supervised),2)
                    metal, game = supervised
                    self.assertEqual(metal['args'][metal['args'].index('--seconds')+1],'10')
                    self.assertEqual(game['args'][game['args'].index('--seconds')+1],configured or '0')
                    self.assertEqual(game['budget'],configured or '0')
                    self.assertEqual(game['visual'], '0' if configured == '1800' else '1')
                    self.assertEqual(game['capture'], '1' if configured == '120' else '0')
                    self.assertEqual(game['audio'], '0' if configured == '1800' else '1')
                    self.assertIn('analyze_visual_trace.py', calls.read_text())
                    self.assertIn('--cooperative-stop', game['args'])
