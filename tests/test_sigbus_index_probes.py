"""Original index*3/table fixture and 26-probe journal capacity; no game/GPU."""
import json
import os
from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile
import unittest
import test_index_capture as reader_fixture

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from analyze_diagnostic_run import collect
from check_cpu_experiment import payload


@unittest.skipUnless(platform.system()=='Darwin' and platform.machine()=='arm64','native ARM64 required')
class SigbusIndexTests(unittest.TestCase):
    def test_scaled_index_fault_and_full_probe_capacity(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
            for mode in ('0','1'):
                for value in (2,0x13579bdf,0xe13579bd):
                    state=Path(folder)/(mode+'-'+str(value));state.mkdir();image=state/'boot.bin'
                    selection,fields,spec=reader_fixture.NativeIndexTests().image(image,value)
                    with image.open('rb') as source:start,_=payload(source)
                    data=bytearray(image.read_bytes());body=data[start:]
                    old=bytes.fromhex('488d04db');assert body.count(old)==1
                    at=body.index(old);data[start+at:start+at+4]=bytes.fromhex('488d045b')
                    struct.pack_into('<Q',data,start+1024+2*24+96,0x42)
                    data[start+320:start+344]=b'\x90'*24;image.write_bytes(data)
                    selection=','.join([f'{320+i}:0:0:0:2:2' for i in range(24)]+selection.split(','))
                    spec['selection']=selection
                    for key,config in [('signatures',selection),('field_signatures',fields)]:
                        spec[key]=[dict(site=hex(at),bytes=bytes(data[start+at:start+at+8]).hex())
                            for at in sorted({int(row.split(':')[0],0) for row in config.split(',')})]
                    spec['index_capture']['register']={'fast_value':24,'index_value':25}
                    spec['index_capture']['stride']=24
                    profile=state/'profile.json';profile.write_text(json.dumps(spec))
                    subprocess.run([sys.executable,str(ROOT/'tools/check_pointer_profile.py'),'--image',str(image),
                        '--profile',str(profile),'--selection',selection,'--field-selection',fields,
                        '--output',str(state/'pointer-profile-check.json')],check=True,capture_output=True,text=True)
                    env=dict(os.environ,BB_JIT=mode,BB_DMEM_MB='16',BB_DMEM_EXTRA_MB='0',BB_GPU_USER_DIR=str(state),
                        SOTC_DIAGNOSTIC_JOURNAL='1',SOTC_CPU_FIELD_TRACE=fields,SOTC_CPU_POINTER_TRACE=selection,
                        SOTC_CPU_FAULT_FIELDS='')
                    for name in ('SOTC_CPU_INTERPRET_RANGE','BB_JIT_DENY','SOTC_SKIP_INTRO_SITE'):env.pop(name,None)
                    log=state/'run.log'
                    run=subprocess.run([sys.executable,str(ROOT/'tools/run_gpu_diagnostic.py'),'--seconds','5','--log',str(log),
                        '--',str(ROOT/'work/probe_entry_arm64'),str(image),'--bootstrap'],env=env,capture_output=True,text=True,timeout=10)
                    self.assertIn(run.returncode,(1,) if value==2 else (138,139),run.stdout+run.stderr)
                    report=collect(log,state);roles=report['index_capture']['roles']
                    self.assertEqual(len(report['pre_failure_journals']['pointer']['channels']),28)
                    self.assertEqual(roles['index_value']['observations'][-1]['uint32'],value)
                    self.assertEqual(roles['after_scale']['observations'][-1]['value'],value*3)
                    base=roles['object_base']['observations'][-1]['object']
                    effective=roles['effective_address']['observations'][-1]
                    self.assertEqual(effective['object'],base+value*24+96)
                    if value==2:self.assertEqual(effective['value'],0x42)
                    else:self.assertEqual(report['current']['fatal_reporting']['minimal_snapshots'][0]['address'],effective['object'])


if __name__=='__main__':unittest.main()
