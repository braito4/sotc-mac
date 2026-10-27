"""Original slot -> atomic consumer controls through the real JIT/interpreter."""
from pathlib import Path
import struct
import tempfile
import unittest
import test_pointer_trace as pointer_fixtures
import platform

ROOT=pointer_fixtures.ROOT


@unittest.skipUnless(platform.system()=='Darwin' and platform.machine()=='arm64','native ARM64 required')
class PointerReleaseTests(unittest.TestCase):
    run_case=pointer_fixtures.PointerTraceTests.run_case

    def image(self,path,mode):
        data=bytearray(16384)
        def relative(at,opcode,target):
            code=bytes.fromhex(opcode);return code+struct.pack('<i',target-at-len(code)-4)
        # Independent synthetic caller. The extra call admits a source watch
        # before the producer; continuations exercise actual block-entry probes.
        data[:34]=b'\x53'+relative(1,'488d1d',512)+bytes.fromhex('4889df')+relative(11,'e8',256)+relative(16,'e8',128)+bytes.fromhex('488b3b')+relative(24,'e8',64)+bytes.fromhex('83f802750b')
        # Valid control checks both old returned count and updated memory.
        data[34:47]=bytes.fromhex('837f40017505488b035bc3cd41')
        # eax=-1; locked add of -1 at an independent object's +64; ret.
        data[64:75]=bytes.fromhex('b8fffffffff00fc14740c3')
        bad=0x123456789abcde00
        if mode=='direct':producer=bytes.fromhex('48b8')+struct.pack('<Q',bad)+bytes.fromhex('488903c3')
        elif mode=='memcpy':
            producer=bytes.fromhex('4889df')+relative(131,'488d35',900)+bytes.fromhex('ba08000000')+relative(143,'488b05',320)+bytes.fromhex('4883ec08ffd04883c408c3')
        else:producer=b'\xc3'
        data[128:128+len(producer)]=producer;data[256]=0xc3
        data[832:836]=struct.pack('<I',3 if mode=='wrong_count' else 2);data[900:908]=struct.pack('<Q',bad)
        relocs=[(320,1,0,0),(512,0,768,0)]
        if mode=='initial':relocs.pop();data[512:520]=struct.pack('<Q',bad)
        name=b'Q3VBxCXhUHs#1#u'
        path.write_bytes(struct.pack('<8s6Q',b'BBPROBE2',len(data),0,2,len(relocs),1,1)+
            struct.pack('<6Q',0,512,5,512,len(data)-512,6)+name.ljust(128,b'\0')+
            b''.join(struct.pack('<4Q',*r) for r in relocs)+data)
        return data

    def test_valid_and_corrupt_sources_in_both_execution_modes(self):
        selection='256:7:0:0:1:1@0,21:3:0:0:1:1@0,64:7:0:0:2:2,29:3:0:0:1:2'
        with tempfile.TemporaryDirectory(prefix='release-source-',dir=ROOT/'work') as folder:
            for jit in ('1','0'):
                for mode in ('valid','wrong_count','initial','direct','memcpy'):
                    with self.subTest(jit=jit,mode=mode):
                        result,report=self.run_case(Path(folder),mode,jit,selection=selection,scenario_profile=True)
                        pointers=report['pre_failure_journals']['pointer']
                        if mode=='wrong_count':
                            self.assertEqual(result.returncode,30,result.stderr)
                            self.assertEqual(pointers['target_probe_hits'],[1,1,1,1])
                            continue
                        if mode=='valid':
                            self.assertEqual(result.returncode,1,result.stderr)
                            self.assertEqual(report['current']['stops'],[])
                            self.assertEqual(pointers['target_probe_hits'],[1,1,1,1])
                            continue
                        self.assertEqual(result.returncode,139,result.stderr)
                        self.assertEqual(pointers['target_probe_hits'],[1,1,1,0])
                        self.assertEqual(report['current']['fatal_reporting']['minimal_snapshots'][0]['address'],0x123456789abcde40)
                        sources=report['fault_pointer_review']['terminal_memory_sources']['candidates']
                        self.assertTrue(any('rbx' in row['labels'] and row['value']==0x123456789abcde00 and row['fault_displacement']==64 for row in sources))
                        terminal=report['pre_failure_journals']['cpu']['terminal_context']
                        self.assertEqual(terminal['kind'],2 if jit=='1' else 4)
                        self.assertEqual(terminal['source'],'translated_fault' if jit=='1' else 'interpreted_fault')
                        copies=[e for e in pointers['events'] if e['actor']!=1]
                        self.assertEqual([e['actor'] for e in copies],[2,3] if mode=='memcpy' else [])
                        incoming=pointers['register_observations'][0]
                        self.assertEqual(incoming['value'],0x123456789abcde00)
                        self.assertEqual(incoming['probe_kind'],'register_value')


if __name__=='__main__':unittest.main()
