"""Original array-entry controls, including the first loop iteration and 24 probes."""
from pathlib import Path
import platform
import struct
import tempfile
import unittest
import test_pointer_trace as fixtures

ROOT=fixtures.ROOT


@unittest.skipUnless(platform.system()=='Darwin' and platform.machine()=='arm64','native ARM64 required')
class PointerEntriesTests(unittest.TestCase):
    run_case=fixtures.PointerTraceTests.run_case

    def image(self,path,mode):
        image=bytearray(16384);bad=0x123456789abcde00
        # mov rcx,array; mov r8d,3; loop: mov rsi,[rcx]; add rcx,8;
        # mov eax,[rsi+196]; dec r8d; jnz loop; ret
        image[:35]=bytes.fromhex('48b9')+bytes(8)+bytes.fromhex('41b803000000488b314883c1088b86c400000041ffc875eec3')
        relocs=[(2,0,512,0)]+[(512+i*8,0,768+i*256,0) for i in range(3)]
        for i in range(3):struct.pack_into('<I',image,768+i*256+196,0x31+i)
        if mode=='arraybad':relocs.pop(0);struct.pack_into('<Q',image,2,bad)
        elif mode in ('firstbad','secondbad'):
            index=0 if mode=='firstbad' else 1
            relocs.remove((512+index*8,0,768+index*256,0));struct.pack_into('<Q',image,512+index*8,bad)
        path.write_bytes(struct.pack('<8s6Q',b'BBPROBE2',len(image),0,2,len(relocs),0,1)+
            struct.pack('<6Q',0,256,5,256,len(image)-256,6)+
            b''.join(struct.pack('<4Q',*row) for row in relocs)+image)
        return image

    def test_entry_before_load_and_operand_after_load_in_both_modes(self):
        base=['16:1:0:0:1:1@0','23:1:-8:0:1:2','23:6:0:0:2:2']
        # All 24 independent channels share real instruction boundaries, while
        # register-only controls add no guest reads or persistent watches.
        base += [f'16:{reg}:0:0:2:2' for reg in range(16)]
        base += [f'23:{reg}:0:0:2:2' for reg in range(5)]
        self.assertEqual(len(base),24)
        with tempfile.TemporaryDirectory(prefix='pointer-entries-',dir=ROOT/'work') as folder:
            for jit in ('1','0'):
                for mode,iterations in [('valid',3),('firstbad',1),('secondbad',2),('arraybad',1)]:
                    with self.subTest(jit=jit,mode=mode):
                        result,report=self.run_case(Path(folder),mode,jit,selection=','.join(base),scenario_profile=True)
                        pointer=report['pre_failure_journals']['pointer']
                        self.assertEqual(pointer['state'],'readable')
                        self.assertEqual(result.returncode,1 if mode=='valid' else 139,result.stderr)
                        self.assertEqual(pointer['target_probe_hits'][0],iterations)
                        self.assertEqual(pointer['target_probe_hits'][1:3],[0,0] if mode=='arraybad' else [iterations]*2)
                        self.assertEqual(pointer['target_probe_hits'][3:19],[iterations]*16)
                        self.assertEqual(pointer['target_probe_hits'][19:],[0]*5 if mode=='arraybad' else [iterations]*5)
                        before=[e for e in pointer['events'] if e['channel']==0]
                        after=[e for e in pointer['events'] if e['channel']==1]
                        if mode=='arraybad':
                            self.assertFalse(before[0]['status']&8)
                            self.assertEqual(pointer['watches_armed'],0)
                        elif mode=='valid':
                            self.assertEqual([e['field'] for e in before],[e['field'] for e in after])
                            self.assertEqual([e['value'] for e in before],[e['value'] for e in after])
                        else:
                            self.assertEqual(before[-1]['value'],0x123456789abcde00)
                            self.assertEqual(before[-1]['field'],after[-1]['field'])
                            self.assertEqual(after[-1]['value'],before[-1]['value'])
                            self.assertEqual(report['current']['fatal_reporting']['minimal_snapshots'][0]['address'],0x123456789abcdec4)
                            incoming=[e for e in pointer['register_observations'] if e['channel']==2]
                            self.assertEqual(incoming[-1]['value'],0x123456789abcde00)
                            if report['current']['fatal_reporting']['guest_context_complete']:
                                candidate=report['fault_pointer_review']['terminal_register_source_candidates'][0]
                                self.assertEqual(candidate['fault_displacement'],196)
                                self.assertIn('rsi',candidate['matching_registers'])
                            else:
                                # A probe on the faulting instruction routes it
                                # through bbcpu_step; retain pre-access evidence
                                # without inventing a translated fault snapshot.
                                self.assertEqual(report['fault_pointer_review']['terminal_register_source_candidates'],[])


@unittest.skipUnless(platform.system()=='Darwin' and platform.machine()=='arm64','native ARM64 required')
class ReturnedEntryTests(unittest.TestCase):
    run_case=fixtures.PointerTraceTests.run_case

    def image(self,path,mode):
        data=bytearray(16384)
        # Independent getter returns an object; caller loads its member and
        # passes it to a consumer. The pre-load probe is inside a decoded block.
        data[:22]=bytes.fromhex('488d3d')+struct.pack('<i',512-7)+b'\xe8'+struct.pack('<i',64-12)+bytes.fromhex('488b7060')+b'\xe8'+struct.pack('<i',128-21)+b'\xc3'
        data[64:69]=bytes.fromhex('488b4720c3')
        data[128:140]=bytes.fromhex('554889e5488b164889d05dc3')
        relocs=[(544,0,768,0)]
        if mode=='returned_valid':relocs.append((864,0,1024,0))
        else:struct.pack_into('<Q',data,864,0x123456789abcde00)
        struct.pack_into('<Q',data,1024,42)
        path.write_bytes(struct.pack('<8s6Q',b'BBPROBE2',len(data),0,2,len(relocs),0,1)+
            struct.pack('<6Q',0,256,5,256,len(data)-256,6)+
            b''.join(struct.pack('<4Q',*r) for r in relocs)+data)
        return data

    def test_returned_object_member_is_captured_before_argument_load(self):
        selection='12:0:96:0:1:1@0,128:6:0:0:2:2'
        with tempfile.TemporaryDirectory(prefix='returned-entry-',dir=ROOT/'work') as folder:
            for jit in ('1','0'):
                for mode in ('returned_valid','returned_bad'):
                    with self.subTest(jit=jit,mode=mode):
                        result,report=self.run_case(Path(folder),mode,jit,selection=selection,scenario_profile=True)
                        self.assertEqual(result.returncode,1 if mode=='returned_valid' else 139,result.stderr)
                        p=report['pre_failure_journals']['pointer']
                        self.assertEqual(p['target_probe_hits'],[1,1])
                        source=next(e for e in p['events'] if e['channel']==0)
                        incoming=p['register_observations'][0]
                        self.assertEqual(source['value'],incoming['value'])
                        self.assertTrue(source['watch_admitted'])
                        if mode=='returned_bad':
                            self.assertEqual(source['value'],0x123456789abcde00)
                            self.assertEqual(report['fault_pointer_review']['matching_source_observations'][0]['field'],source['field'])


if __name__=='__main__':unittest.main()
