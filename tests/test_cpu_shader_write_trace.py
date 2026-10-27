"""Execute synthetic translated stores with the bounded post-store hook."""
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from prepare_cpu_probe_support import prepare


@unittest.skipUnless(platform.system()=='Darwin' and platform.machine()=='arm64','native ARM64 required')
class ShaderWriteTraceTests(unittest.TestCase):
    def test_scalar_vector_disabled_and_interpreter_controls(self):
        cpu=ROOT/'runtime/upstream/recomp';deps=ROOT/'work/deps-arm64'
        with tempfile.TemporaryDirectory(prefix='shader-write-trace-',dir=ROOT/'work') as folder:
            folder=Path(folder);source=folder/'jit.c'
            source.write_text(prepare((cpu/'src/cpu/jit_arm64.c').read_bytes()))
            flags=['clang','-arch','arm64','-O2','-std=gnu11','-pthread',
                '-I'+str(cpu/'src/cpu'),'-I'+str(deps/'include'),
                '-I'+str(ROOT/'runtime/upstream/src'),'-I'+str(ROOT/'runtime/src')]
            obj=folder/'jit.o'
            subprocess.run([*flags,'-c',str(source),'-o',str(obj)],check=True,capture_output=True,text=True)
            archive=folder/'libbbcpu.a';shutil.copy2(cpu/'out/libbbcpu.a',archive)
            subprocess.run(['ar','-d',str(archive),'jit_arm64.c.o'],check=True,capture_output=True,text=True)
            subprocess.run(['ar','-r',str(archive),str(obj)],check=True,capture_output=True,text=True)
            binary=folder/'test'
            subprocess.run([*flags,str(ROOT/'tests/test_cpu_shader_write_trace.c'),
                str(ROOT/'runtime/src/platform_arm64.c'),str(archive),
                str(deps/'lib/libZydis.a'),str(deps/'lib/libZycore.a'),'-o',str(binary)],
                check=True,capture_output=True,text=True)
            for jit,trace in [('1','trace'),('1','off'),('0','trace')]:
                env=dict(os.environ,BB_JIT=jit)
                run=subprocess.run([str(binary),trace],env=env,capture_output=True,text=True,timeout=15)
                self.assertEqual(run.returncode,0,f'jit={jit}/{trace}: '+run.stdout+run.stderr)
                if jit=='1' and trace=='trace':
                    self.assertIn('events=2',run.stdout)
                else:self.assertIn('events=0',run.stdout)


if __name__=='__main__':unittest.main()
