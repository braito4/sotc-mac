"""Real terminating CPU diagnostics with entirely original synthetic input."""
import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT/'work/probe_entry_arm64'


@unittest.skipUnless(platform.system() == 'Darwin' and platform.machine() == 'arm64'
                     and shutil.which('clang'), 'native ARM64 host required')
class CpuFaultDiagnosticsTests(unittest.TestCase):
    def test_host_snapshot_and_actual_native_instruction_fault(self):
        with tempfile.TemporaryDirectory(prefix='host fault reader ') as directory:
            binary=Path(directory)/'test'
            subprocess.run(['clang','-arch','arm64','-O2','-std=gnu11','-Wall','-Wextra','-Werror',
                            '-I'+str(ROOT/'runtime/src'),str(ROOT/'tests/test_host_fault_diagnostics.c'),
                            '-o',str(binary)],check=True,capture_output=True,text=True)
            snapshot=subprocess.run([str(binary)],check=True,capture_output=True,text=True,timeout=10)
            self.assertIn('x0=0x100',snapshot.stdout)
            self.assertIn('x28=0x11c',snapshot.stdout)
            self.assertIn('pc=0xffffffffffffffff lr=0x123456',snapshot.stdout)
            self.assertIn('no pointed-to memory read',snapshot.stdout)
            self.assertLess(len(snapshot.stdout),4096)
            fault=subprocess.run([str(binary),'--fault'],capture_output=True,text=True,timeout=10)
            self.assertEqual(fault.returncode,139,fault.stdout+fault.stderr)
            self.assertEqual(fault.stderr.count('Host ARM64 context: private'),1)
            self.assertIn('host control: pc=0x1000 lr=0x',fault.stderr)
            self.assertIn('x28=',fault.stderr)
            self.assertIn('Host ARM64 context: end',fault.stderr)
            self.assertRegex(fault.stdout,r'Host image: load=0x[0-9a-f]+ text=0x[0-9a-f]+ size=0x[0-9a-f]+ uuid=[0-9a-f]{32} name=test')
            self.assertLessEqual(fault.stdout.count('Host image:'),512)

    def test_native_reader_guards_and_bounds(self):
        with tempfile.TemporaryDirectory(prefix='cpu fault reader ') as directory:
            binary = Path(directory)/'test'
            subprocess.run(['clang', '-arch', 'arm64', '-O2', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                            '-I'+str(ROOT/'runtime/src'), '-I'+str(ROOT/'runtime/upstream/src'),
                            '-I'+str(ROOT/'runtime/upstream/recomp/src/cpu'),
                            str(ROOT/'tests/test_cpu_fault_diagnostics.c'),
                            str(ROOT/'runtime/src/platform_arm64.c'), '-o', str(binary)],
                           check=True, capture_output=True, text=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True, timeout=10)
            self.assertIn('PASS:', result.stdout)

    @unittest.skipUnless(PROBE.is_file(), 'built native diagnostic required')
    def test_actual_fatal_path_keeps_exit_and_reports_available_evidence(self):
        with tempfile.TemporaryDirectory(prefix='cpu fatal fixture ') as directory:
            boot = Path(directory)/'fixture.bin'
            image = bytearray(16384)
            image[:7] = bytes.fromhex('e8 3b000000 0f0b')  # call synthetic function; ud2
            code = bytearray.fromhex('55 4889e5')  # frame pointer for one candidate
            code += bytes.fromhex('4c8d25') + struct.pack('<i', 512-(64+len(code)+7))
            # Original r12/r10 fixture with scale two, unrelated object layout.
            code += bytes.fromhex('4d8b542410 4983e2f8 4b8b445420 c9c3')
            image[64:64+len(code)] = code
            bad_size = 0x1234567800000055
            image[512:544] = struct.pack('<4Q', 0x1111, 0x2222, bad_size, 0x4444)
            field_marker = 0x9876543212345678
            image[512+305:512+313] = struct.pack('<Q', field_marker)
            boot.write_bytes(struct.pack('<8s9Q', b'BBPROBE2', len(image), 0, 1, 0, 0, 0,
                                         0, len(image), 5)+image)
            for mode in ('1', '0'):
                with self.subTest(mode=mode):
                    result = subprocess.run([str(PROBE), str(boot)], capture_output=True, text=True,
                        timeout=10, env=dict(os.environ, BB_JIT=mode, BB_DMEM_MB='16',
                                            BB_DMEM_EXTRA_MB='0', SOTC_DIAGNOSTIC_SECONDS='5',
                                            SOTC_CPU_FAULT_FIELDS='12:305'))
                    self.assertEqual(result.returncode, 139, result.stdout+result.stderr)
                    self.assertEqual(result.stderr.count('STOP_EXECUTION_FAULT: SIGSEGV'), 1)
                    self.assertEqual(result.stderr.count('Host ARM64 context: private'),1)
                    self.assertIn('host control: pc=0x',result.stderr)
                    self.assertIn(' lr=0x',result.stderr)
                    if mode == '1':
                        self.assertIn('CPU fault memory r12', result.stderr)
                        self.assertIn(hex(bad_size), result.stderr)
                        self.assertIn('image_offset=0x5', result.stderr)
                        self.assertIn('CPU fault context: end', result.stderr)
                        self.assertIn('CPU fault field candidate reg=r12 offset=0x131', result.stderr)
                        self.assertIn(hex(field_marker), result.stderr)
                    else:
                        self.assertIn('interpreted guest instruction', result.stderr)
                        self.assertNotIn('CPU fault context: unavailable outside translated code', result.stderr)
                        self.assertIn('CPU fault memory r12', result.stderr)
                        self.assertIn(hex(bad_size), result.stderr)
                        self.assertIn('CPU fault context: end', result.stderr)
                        self.assertIn('CPU fault field candidate reg=r12 offset=0x131', result.stderr)
                        self.assertIn(hex(field_marker), result.stderr)

    @unittest.skipUnless(PROBE.is_file(), 'built native diagnostic required')
    def test_invalid_fields_stop_before_loading_input(self):
        result = subprocess.run([str(PROBE), '/missing/synthetic-input'], capture_output=True,
            text=True, timeout=10, env=dict(os.environ, SOTC_CPU_FAULT_FIELDS='12:0:9'))
        self.assertEqual(result.returncode, 24, result.stderr)
        self.assertIn('Invalid SOTC_CPU_FAULT_FIELDS', result.stderr)
        self.assertNotIn('STOP_EXECUTION_FAULT', result.stderr)


if __name__ == '__main__':
    unittest.main()
