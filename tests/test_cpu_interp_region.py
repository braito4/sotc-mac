"""Real pinned CPU comparisons, synthetic input only; no game or GPU."""
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
class InterpreterRegionTests(unittest.TestCase):
    def test_execution_boundaries_and_invalid_operations(self):
        cpu=ROOT/'runtime/upstream/recomp';deps=ROOT/'work/deps-arm64'
        with tempfile.TemporaryDirectory(prefix='cpu-regions-',dir=ROOT/'work') as folder:
            folder=Path(folder);source=folder/'jit.c';source.write_text(prepare((cpu/'src/cpu/jit_arm64.c').read_bytes()))
            flags=['clang','-arch','arm64','-O2','-std=gnu11','-Wall','-Wextra','-Werror','-pthread',
                   '-I'+str(cpu/'src/cpu'),'-I'+str(deps/'include'),'-I'+str(ROOT/'runtime/upstream/src'),'-I'+str(ROOT/'runtime/src')]
            obj=folder/'jit_region.o';subprocess.run([*flags,'-c',str(source),'-o',str(obj)],check=True,capture_output=True,text=True)
            archive=folder/'libcpu.a';shutil.copy2(cpu/'out/libbbcpu.a',archive)
            subprocess.run(['ar','-d',str(archive),'jit_arm64.c.o'],check=True,capture_output=True,text=True)
            subprocess.run(['ar','-r',str(archive),str(obj)],check=True,capture_output=True,text=True)
            binary=folder/'control'
            subprocess.run([*flags,str(ROOT/'tests/test_cpu_interp_region.c'),str(ROOT/'runtime/src/platform_arm64.c'),
                str(archive),str(deps/'lib/libZydis.a'),str(deps/'lib/libZycore.a'),'-o',str(binary)],check=True,capture_output=True,text=True)
            env=dict(os.environ,BB_JIT='1');env.pop('BB_JIT_DENY',None);env.pop('SOTC_CPU_INTERPRET_RANGE',None)
            for mode in ('none','caller','callee','helper','partial','branch'):
                for threaded in (False,True):
                    with self.subTest(mode=mode,threaded=threaded):
                        run=subprocess.run([str(binary),mode]+(['threads'] if threaded else []),env=env,capture_output=True,text=True,timeout=15)
                        self.assertEqual(run.returncode,0,run.stdout+run.stderr)
                        self.assertEqual('CPU region interpreter entered:' in run.stderr,mode!='none')
                        boundaries=[line for line in run.stderr.splitlines() if line.startswith('CPU region tail:')]
                        if mode=='none':self.assertEqual(boundaries,[])
                        else:
                            self.assertTrue(any('kind=enter ' in line and 'thread=' in line and 'rdi=' in line for line in boundaries),run.stderr)
                            self.assertTrue(any('kind=exit ' in line and 'thread=' in line and 'rsi=' in line for line in boundaries),run.stderr)
                            self.assertLessEqual(len(boundaries),32 if threaded else 8)
                            seqs=[int(line.split(' seq=')[1].split()[0],0) for line in boundaries]
                            self.assertGreater(min(seqs),64,run.stderr)
            snapshot=subprocess.run([str(binary),'callee','snapshot'],env=env,capture_output=True,text=True,timeout=15)
            self.assertEqual(snapshot.returncode,0,snapshot.stdout+snapshot.stderr)
            tails=[line for line in snapshot.stderr.splitlines() if line.startswith('CPU region tail:')]
            self.assertTrue(tails,snapshot.stderr)
            self.assertLessEqual(len(tails),160)
            for line in tails:
                values=dict(token.split('=',1) for token in line.split()[3:])
                self.assertIn(values['kind'],('enter','exit'))
                self.assertGreater(int(values['seq'],0),0)
                self.assertGreater(int(values['ns'],0),0)
                self.assertTrue(all(k in values for k in ('rax','rcx','rdx','rsi','rdi')))
            field_modes=('field-base','field-load','field-consumer','field-both')
            field_results=[]
            for mode in field_modes:
                with self.subTest(field_mode=mode):
                    run=subprocess.run([str(binary),mode],env=env,capture_output=True,text=True,timeout=15)
                    self.assertEqual(run.returncode,0,run.stdout+run.stderr)
                    self.assertIn('result=0x8309008f80',run.stdout)
                    field_results.append(run.stdout)
                    boundaries=[line for line in run.stderr.splitlines() if line.startswith('CPU region tail:')]
                    if mode=='field-base':self.assertEqual(boundaries,[])
                    else:
                        self.assertTrue(any('kind=enter ' in line for line in boundaries),run.stderr)
                        self.assertTrue(any('kind=exit ' in line for line in boundaries),run.stderr)
            self.assertTrue(all('result=0x8309008f80' in line for line in field_results),field_results)
            selection=[]
            for mode in ('selection-base','selection-interpreted'):
                run=subprocess.run([str(binary),mode],env=env,capture_output=True,text=True,timeout=15)
                self.assertEqual(run.returncode,0,run.stdout+run.stderr)
                self.assertIn('result=0x9999aaaabbbbcccc',run.stdout)
                tails=[line for line in run.stderr.splitlines() if line.startswith('CPU region tail:')]
                if mode=='selection-base':self.assertEqual(tails,[])
                else:
                    self.assertEqual(len(tails),2,run.stderr)
                    self.assertTrue(any('kind=enter ' in line and 'offset=0x306 ' in line for line in tails),run.stderr)
                    self.assertTrue(any('kind=exit ' in line and 'offset=0x311 ' in line for line in tails),run.stderr)
                selection.append(run.stdout.splitlines()[-1].split(' result=')[-1])
            self.assertEqual(selection,['0x9999aaaabbbbcccc']*2)
            selection_interpreter=subprocess.run([str(binary),'selection-base'],env=dict(env,BB_JIT='0'),
                capture_output=True,text=True,timeout=15)
            self.assertEqual(selection_interpreter.returncode,0,selection_interpreter.stdout+selection_interpreter.stderr)
            self.assertIn('result=0x9999aaaabbbbcccc',selection_interpreter.stdout)
            full_field=subprocess.run([str(binary),'field-base'],env=dict(env,BB_JIT='0'),capture_output=True,text=True,timeout=15)
            self.assertEqual(full_field.returncode,0,full_field.stdout+full_field.stderr)
            self.assertEqual(full_field.stdout.splitlines()[-1].split(' result=')[-1],
                             field_results[0].splitlines()[-1].split(' result=')[-1])
            invalid=subprocess.run([str(binary),'field-invalid'],env=env,capture_output=True,text=True,timeout=15)
            self.assertEqual(invalid.returncode,139,invalid.stdout+invalid.stderr)
            self.assertTrue(any('kind=enter ' in line for line in invalid.stderr.splitlines()),invalid.stderr)
            full=subprocess.run([str(binary),'none'],env=dict(env,BB_JIT='0'),capture_output=True,text=True,timeout=15)
            self.assertEqual(full.returncode,0,full.stdout+full.stderr)
            for mode,status in [('fault',139),('unsupported',30)]:
                run=subprocess.run([str(binary),mode],env=env,capture_output=True,text=True,timeout=10)
                self.assertEqual(run.returncode,status,run.stdout+run.stderr)
                self.assertIn('CPU region interpreter entered:',run.stderr)
            for bad in ('','0x0:0x10001','0x1:0x1','0x0:0x2,0x3:0x4'):
                run=subprocess.run([str(binary),'invalid'],env=dict(env,SOTC_CPU_INTERPRET_RANGE=bad),capture_output=True,text=True,timeout=10)
                self.assertEqual(run.returncode,24,run.stdout+run.stderr)
                self.assertNotIn('CPU region interpreter entered:',run.stderr)


if __name__=='__main__':unittest.main()
