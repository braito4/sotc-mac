"""Original synthetic linked provider fixture; no firmware, game, GPU or assets."""
from pathlib import Path
import os
import platform
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT/'work/probe_entry_arm64'
NOT_LOADED = 0x805a1001

def fixture(path, mode):
    image = bytearray(16384)
    slots = [256+i*8 for i in range(6)]
    # Two exports, load, loaded, unload and explicit terminating fixture import.
    names = ['mPYgU4oYpuY#z#t','i0VnXM-C9fc#z#t','g8cM39EUZ6o#F#G',
             'fMP5NHUOaMk#F#G','eR2bZFAAU0Q#F#G','fixture-stop#none']
    if mode == 'wrong-scope': names[0] = 'mPYgU4oYpuY#other'
    bindings = [(0,1408,1),(1,1424,1)]
    if mode == 'missing-export': bindings.pop()
    if mode == 'data-export': bindings[1] = (1,1424,2)
    if mode == 'nonexecutable': bindings[1] = (1,8192,1)
    if mode == 'different-module': bindings[1] = (1,9216,1)
    available = mode == 'ready'

    def code_at(base, checks, init=False):
        code = bytearray(); failures=[]
        for index, expected in checks:
            code += b'\xbf'+struct.pack('<I',0xb)
            code += b'\xff\x15'+struct.pack('<i',slots[index]-(base+len(code)+6))
            code += b'\x3d'+struct.pack('<I',expected)+b'\x0f\x85'
            failures.append(len(code)); code += b'\0'*4
        if init:
            code += b'\xb8'+struct.pack('<I',7 if mode == 'failed-init' else 0)+b'\xc3'
        else:
            code += b'\xbf'+struct.pack('<I',0xc0de)
            code += b'\xff\x15'+struct.pack('<i',slots[5]-(base+len(code)+6))+b'\x0f\x0b'
        fail = len(code)
        code += b'\xbf'+struct.pack('<I',0xbad)
        code += b'\xff\x15'+struct.pack('<i',slots[5]-(base+len(code)+6))+b'\x0f\x0b'
        for offset in failures: struct.pack_into('<i',code,offset,fail-(offset+4))
        return code

    checks = [(3,NOT_LOADED),(2,0),(2,0),(3,0),(4,0),(3,0),(4,0),(3,NOT_LOADED),(4,NOT_LOADED)] if available else [(2,NOT_LOADED),(3,NOT_LOADED)]
    code=code_at(0,checks);assert len(code)<256;image[:len(code)]=code
    # Even with native bindings, a load during its initializer must fail.
    init=code_at(1024,[(2,NOT_LOADED)],init=True);image[1024:1024+len(init)]=init
    image[1408:1411]=bytes.fromhex('31c0c3');image[1424:1427]=bytes.fromhex('31c0c3')
    image[8192:8195]=bytes.fromhex('31c0c3');image[9216:9219]=bytes.fromhex('31c0c3')
    words=lambda *v:struct.pack('<'+'Q'*len(v),*v)
    modules=[(1024,12288,1024,0,0,0,0)]
    if mode == 'different-module': modules=[(1024,4096,1024,0,0,0,0),(9216,4096,9216,0,0,0,0)]
    if mode == 'nonexecutable': segments=[(0,8192,5),(8192,8192,6)]
    else: segments=[(0,16384,5)]
    path.write_bytes(b'BBPROBE5'+words(16384,0,len(segments),6,6,1)+words(512,0,0,0,0,len(modules))+
        b''.join(words(*m) for m in modules)+words(len(bindings))+b''.join(words(*b) for b in bindings)+
        b''.join(words(*s) for s in segments)+b''.join(n.encode().ljust(128,b'\0') for n in names)+
        b''.join(words(slots[i],1,i,0) for i in range(6))+image)

@unittest.skipUnless(platform.system()=='Darwin' and PROBE.is_file(), 'requires built macOS diagnostic')
class LinkedModuleTests(unittest.TestCase):
    def test_provider_requires_executable_bindings_and_successful_initializer(self):
        with tempfile.TemporaryDirectory(prefix='sotc linked provider ') as temp:
            boot=Path(temp)/'fixture.bin'
            for mode in ('ready','missing-export','wrong-scope','data-export','nonexecutable','different-module','failed-init'):
                fixture(boot,mode)
                for jit in ('1','0'):
                    result=subprocess.run([str(PROBE),str(boot),'--bootstrap'],capture_output=True,text=True,
                        timeout=15,env=dict(os.environ,BB_JIT=jit))
                    expected = 1 if mode in ('failed-init','data-export','nonexecutable','different-module') else 20
                    self.assertEqual(result.returncode,expected,(mode,jit,result.stdout,result.stderr))
                    self.assertEqual('NGS2 module initialized' in result.stdout,mode=='ready',(mode,result.stdout))
                    if mode=='failed-init': self.assertIn('module initializer failed',result.stderr)
                    elif mode=='data-export': self.assertIn('native binding kind',result.stderr)
                    elif mode=='nonexecutable': self.assertIn('native function is not executable',result.stderr)
                    elif mode=='different-module': self.assertIn('single linked module',result.stderr)
                    else: self.assertIn('Guest first argument: 0xc0de',result.stdout,(mode,jit,result.stdout))

if __name__=='__main__': unittest.main()
