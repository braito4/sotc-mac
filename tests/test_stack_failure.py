"""Real terminating guest import, discarded/blocked output; no game or GPU."""
import os
from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from analyze_event_journal import analyze
from analyze_diagnostic_run import summarize


@unittest.skipUnless(platform.system()=='Darwin' and platform.machine()=='arm64','native ARM64 required')
class StackFailureTests(unittest.TestCase):
    def image(self,path):
        code=bytearray(16384)
        # An original synthetic function calls the actual stack-failure import.
        # This tests that terminating path, not a game/PS4 stack-canary layout.
        code[:15]=bytes.fromhex('55 4889e5 488b05')+struct.pack('<i',64-11)+bytes.fromhex('ffd0 c9 c3')
        name=b'Ou3iL1abvng#0#F'
        path.write_bytes(struct.pack('<8s6Q',b'BBPROBE2',len(code),0,2,1,1,1)+
            struct.pack('<6Q',0,256,5,256,len(code)-256,6)+name.ljust(128,b'\0')+
            struct.pack('<4Q',64,1,0,0)+code)

    def test_stack_failure_retains_exit_and_context_in_both_cpu_modes(self):
        with tempfile.TemporaryDirectory(prefix='stack-failure-',dir=ROOT/'work') as folder:
            root=Path(folder);image=root/'input.bin';self.image(image)
            for jit in ('1','0'):
                for output in ('drained','discarded','full','closed'):
                    with self.subTest(jit=jit,output=output):
                        state=root/(jit+'-'+output);state.mkdir()
                        env=dict(os.environ,BB_JIT=jit,BB_DMEM_MB='16',BB_DMEM_EXTRA_MB='0',
                            BB_GPU_USER_DIR=str(state),SOTC_DIAGNOSTIC_JOURNAL='1',
                            SOTC_CPU_FIELD_TRACE='0:4:0',SOTC_CPU_FAULT_FIELDS='',SOTC_DIAGNOSTIC_SECONDS='5')
                        control_read,control_write=os.pipe();read=write=None
                        env['SOTC_DIAGNOSTIC_CONTROL_FD']=str(control_read)
                        stderr=subprocess.PIPE if output=='drained' else subprocess.DEVNULL
                        if output in ('full','closed'):
                            read,write=os.pipe();os.set_blocking(write,False);stderr=write
                            if output=='full':
                                try:
                                    while True:os.write(write,b'x'*512)
                                except BlockingIOError:pass
                            else:os.close(read);read=None
                        try:
                            run=subprocess.run([str(ROOT/'work/probe_entry_arm64'),str(image),'--bootstrap'],
                                env=env,stdout=subprocess.DEVNULL,stderr=stderr,pass_fds=(control_read,),timeout=5)
                            self.assertEqual(run.returncode,22,run.stderr)
                            evidence=analyze(state/'cpu-events.bin','cpu')
                            self.assertEqual(evidence['state'],'readable')
                            terminal=evidence['terminal_context']
                            self.assertEqual(terminal['kind'],1);self.assertEqual(terminal['available'],1)
                            self.assertGreater(terminal['registers']['rsp'],0)
                            self.assertGreater(terminal['thread'],0)
                            if output=='drained':
                                text=run.stderr.decode();self.assertIn('STOP: guest stack protector detected corruption',text)
                                self.assertIn('raw import state, not validated caller or canary',text)
                                report=summarize(text)
                                self.assertEqual(report['stack_protector_failures'][0]['available'],1)
                                self.assertEqual(report['cpu']['dump_state'],'complete')
                        finally:
                            os.close(control_read);os.close(control_write)
                            if read is not None:os.close(read)
                            if write is not None:os.close(write)


if __name__=='__main__':unittest.main()
