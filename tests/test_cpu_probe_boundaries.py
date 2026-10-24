"""Actual pinned CPU old/new probe controls, without a game or GPU."""
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


class ProbePreparationTests(unittest.TestCase):
    def test_foreign_or_modified_source_is_rejected(self):
        with self.assertRaises(ValueError):prepare(b'int main(void) {return 0;}')
        path=ROOT/'runtime/upstream/recomp/src/cpu/jit_arm64.c'
        if path.is_file():
            with self.assertRaises(ValueError):prepare(path.read_bytes()+b'\n')


@unittest.skipUnless(platform.system()=='Darwin' and platform.machine()=='arm64','native ARM64 required')
class ProbeBoundariesTests(unittest.TestCase):
    def test_old_misses_interior_probes_new_retains_first_and_loop_iterations(self):
        cpu=ROOT/'runtime/upstream/recomp';deps=ROOT/'work/deps-arm64'
        with tempfile.TemporaryDirectory(prefix='probe-boundaries-',dir=ROOT/'work') as folder:
            folder=Path(folder);source=folder/'jit.c';source.write_text(prepare((cpu/'src/cpu/jit_arm64.c').read_bytes()))
            flags=['clang','-arch','arm64','-O2','-std=gnu11','-pthread','-I'+str(cpu/'src/cpu'),'-I'+str(deps/'include'),'-I'+str(ROOT/'runtime/upstream/src'),'-I'+str(ROOT/'runtime/src')]
            obj=folder/'jit_probe.o';subprocess.run([*flags,'-c',str(source),'-o',str(obj)],check=True,capture_output=True,text=True)
            archive=folder/'libadapted.a';shutil.copy2(cpu/'out/libbbcpu.a',archive)
            subprocess.run(['ar','-d',str(archive),'jit_arm64.c.o'],check=True,capture_output=True,text=True)
            subprocess.run(['ar','-r',str(archive),str(obj)],check=True,capture_output=True,text=True)
            for label,lib in [('old',cpu/'out/libbbcpu.a'),('adapted',archive)]:
                binary=folder/label
                subprocess.run([*flags,str(ROOT/'tests/test_cpu_probe_boundaries.c'),str(ROOT/'runtime/src/platform_arm64.c'),str(lib),str(deps/'lib/libZydis.a'),str(deps/'lib/libZycore.a'),'-o',str(binary)],check=True,capture_output=True,text=True)
                for jit in ('0','1'):
                    for mode in ('none','missed' if label=='old' and jit=='1' else 'observed'):
                        run=subprocess.run([str(binary),mode],env=dict(os.environ,BB_JIT=jit),capture_output=True,text=True,timeout=10)
                        self.assertEqual(run.returncode,0,f'{label}/{jit}/{mode}: '+run.stdout+run.stderr)


if __name__=='__main__':unittest.main()
