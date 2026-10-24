"""Synthetic four-byte reader, saved index and address calculation; no game/GPU."""
import copy
import hashlib
import json
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
from analyze_index_capture import SCALAR_ROLES,REGISTER_ROLES,review,validate
from analyze_diagnostic_run import collect


def annotations():
    return dict(schema=1,scalar=dict(zip(SCALAR_ROLES,range(11))),
                register=dict(zip(REGISTER_ROLES,range(2))),stride=72,bias=96,width=32)


class IndexReviewTests(unittest.TestCase):
    def test_invalid_annotations_and_settings_are_rejected(self):
        spec=annotations();validate(spec,11,2)
        for role,value in [('fast_cursor',True),('fast_end',99),('fast_buffer',0)]:
            broken=copy.deepcopy(spec);broken['scalar'][role]=value
            with self.assertRaises(ValueError):validate(broken,11,2)
        checked=dict(status='matched',index_capture=spec)
        self.assertEqual(review(checked,{}, {},'x','y')['state'],'rejected_annotations')
        self.assertIsNone(review({}, {}, {},'',''))

    def test_missing_legacy_threads_and_qword_width_are_explicit(self):
        fields=','.join(str(n) for n in range(11));selection='a,b'
        checked=dict(status='matched',index_capture=annotations(),
            selection_sha256=hashlib.sha256(selection.encode()).hexdigest(),
            field_selection_sha256=hashlib.sha256(fields.encode()).hexdigest())
        event=dict(channel=4,seq=1,ns=1,site=1,object=2,field=3,value=0xabc00000002,status=3,thread=1)
        result=review(checked,dict(state='readable',events=[event]),dict(state='absent'),selection,fields)
        row=result['roles']['read_output']['observations'][0]
        self.assertIsNone(row['host_thread']);self.assertEqual(row['uint32'],2)
        self.assertEqual(row['value'],event['value'])
        self.assertEqual(result['roles']['fast_value']['state'],'journal_unavailable')


@unittest.skipUnless(platform.system()=='Darwin' and platform.machine()=='arm64','native ARM64 required')
class NativeIndexTests(unittest.TestCase):
    def image(self,path,index):
        data=bytearray(16384);body=bytearray();sites={};relocs=[]
        def emit(code):body.extend(bytes.fromhex(code))
        def site(name):sites[name]=len(body);emit('90')
        def imm(code,target):
            emit(code);relocs.append((len(body),0,target,0));body.extend(bytes(8))
        emit('554889e54883ec20')
        imm('49bc',512) # R12: reader
        emit('4c8d7df04c89e74c89fe')
        emit('e8');at=len(body);body.extend(struct.pack('<i',256-(at+4)))
        emit('8b5df0');site('read')
        emit('48895de8');site('before')
        emit('488d04db488945e8');site('after')
        imm('49bc',1024);site('object')
        emit('4d8d6cc460');site('effective')
        emit('498b45004889ec5dc3')
        data[:len(body)]=body
        reader=bytearray(bytes.fromhex('554889e5534154415641574889fb4989f74c8b33488b4310428b0430'))
        sites['fast']=256+len(reader);reader.extend(bytes.fromhex('904983c6044c8933418907415f415e415c5b5dc3'))
        data[256:256+len(reader)]=reader
        struct.pack_into('<3Q',data,512,0,4,0);relocs.append((528,0,768,0))
        struct.pack_into('<I',data,768,index);struct.pack_into('<Q',data,1024+2*72+96,0x42)
        fields=[f'{sites["fast"]}:3:0:0',f'{sites["fast"]}:3:8:0',f'{sites["fast"]}:3:16:0',f'{sites["fast"]}:15:0:0',
            f'{sites["read"]}:5:-16:0',f'{sites["read"]}:12:0:0',f'{sites["read"]}:12:16:0',
            f'{sites["before"]}:5:-24:0',f'{sites["after"]}:5:-24:0',f'{sites["object"]}:12:0:0',f'{sites["effective"]}:13:0:0']
        selection=f'{sites["fast"]}:0:0:0:2:2,{sites["read"]}:3:0:0:2:2'
        path.write_bytes(struct.pack('<8s6Q',b'BBPROBE2',len(data),0,2,len(relocs),0,1)+
            struct.pack('<6Q',0,512,5,512,len(data)-512,6)+
            b''.join(struct.pack('<4Q',*r) for r in relocs)+data)
        fields=','.join(fields)
        def signatures(config):
            return [dict(site=str(at),bytes=bytes(data[at:at+8]).hex()) for at in sorted({int(row.split(':')[0]) for row in config.split(',')})]
        return selection,fields,dict(selection=selection,field_selection=fields,signatures=signatures(selection),
            field_signatures=signatures(fields),index_capture=annotations())

    def test_real_reader_and_invalid_index_in_both_backends(self):
        with tempfile.TemporaryDirectory(prefix='index-capture-',dir=ROOT/'work') as folder:
            for mode in ('0','1'):
                for value,status in [(2,1),(0x13579bdf,139)]:
                    with self.subTest(mode=mode,value=value):
                        state=Path(folder)/(mode+'-'+str(value));state.mkdir();image=state/'boot.bin'
                        selection,fields,spec=self.image(image,value);profile=state/'profile.json'
                        profile.write_text(json.dumps(spec))
                        subprocess.run([sys.executable,str(ROOT/'tools/check_pointer_profile.py'),'--image',str(image),
                            '--profile',str(profile),'--selection',selection,'--field-selection',fields,
                            '--output',str(state/'pointer-profile-check.json')],check=True,capture_output=True,text=True)
                        env=dict(os.environ,BB_JIT=mode,BB_DMEM_MB='16',BB_DMEM_EXTRA_MB='0',BB_GPU_USER_DIR=str(state),
                            SOTC_DIAGNOSTIC_JOURNAL='1',SOTC_CPU_FIELD_TRACE=fields,SOTC_CPU_POINTER_TRACE=selection,
                            SOTC_CPU_FAULT_FIELDS='',SOTC_DIAGNOSTIC_SECONDS='5')
                        env.pop('SOTC_CPU_INTERPRET_RANGE',None);env.pop('BB_JIT_DENY',None)
                        log=state/'run.log'
                        run=subprocess.run([sys.executable,str(ROOT/'tools/run_gpu_diagnostic.py'),'--seconds','5','--log',str(log),
                            '--',str(ROOT/'work/probe_entry_arm64'),str(image),'--bootstrap'],env=env,capture_output=True,text=True,timeout=10)
                        if value==2:self.assertEqual(run.returncode,status,run.stdout+run.stderr)
                        else:self.assertIn(run.returncode,(138,139),run.stdout+run.stderr)
                        combined=collect(log,state)
                        report=combined['index_capture'];self.assertEqual(report['state'],'validated_configuration')
                        roles=report['roles']
                        for role in SCALAR_ROLES+REGISTER_ROLES:
                            self.assertEqual(roles[role]['state'],'retained',role)
                        for role in REGISTER_ROLES:
                            row=roles[role]['observations'][-1]
                            self.assertEqual(row['uint32'],value);self.assertTrue(row['upper_bits_zero'])
                        self.assertEqual(roles['before_scale']['observations'][-1]['value'],value)
                        self.assertEqual(roles['after_scale']['observations'][-1]['value'],value*9)
                        host=roles['fast_value']['observations'][-1]['host_thread']
                        self.assertTrue(host)
                        self.assertTrue(all(roles[role]['observations'][-1]['host_thread']==host for role in SCALAR_ROLES))
                        base=roles['object_base']['observations'][-1]['object']
                        self.assertEqual(roles['effective_address']['observations'][-1]['object'],base+value*72+96)
                        self.assertEqual(roles['effective_address']['observations'][-1]['value_readable'],value==2)
                        if value!=2:
                            self.assertEqual(combined['current']['fatal_reporting']['minimal_snapshots'][0]['address'],base+value*72+96)


if __name__=='__main__':unittest.main()
