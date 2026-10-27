"""Exact-input variant selection, owner launcher isolation and offline evidence."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from check_cpu_experiment import check
from analyze_diagnostic_run import cpu_experiment_review


class CpuExperimentTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='cpu comparison ')
        self.root=Path(self.temp.name);self.image=self.root/'original.bin';self.profile=self.root/'profile.json'
        data=bytes([0x90])*256
        self.image.write_bytes(struct.pack('<8s9Q',b'BBPROBE2',256,0,1,0,0,0,0,256,5)+data)
        self.spec=dict(schema=1,image_sha256=hashlib.sha256(self.image.read_bytes()).hexdigest(),variants={
            name:dict(begin=16+i*16,end=32+i*16,sha256=hashlib.sha256(data[16+i*16:32+i*16]).hexdigest())
            for i,name in enumerate(('caller','buffer','table','field578'))})
        self.write_profile()

    def tearDown(self):self.temp.cleanup()

    def write_profile(self):self.profile.write_text(json.dumps(self.spec))

    def test_each_selection_and_base_match_exact_input(self):
        for name in ('base','caller','buffer','table','field578'):
            row=check(self.profile,name,self.image)
            self.assertEqual(row['status'],'matched')
            self.assertEqual(row['variant'],name)
        with self.assertRaises(ValueError):check(self.profile,'caller',self.image,'0x20:0x30')
        self.image.write_bytes(self.image.read_bytes()[:-1]+b'\0')
        with self.assertRaises(ValueError):check(self.profile,'base',self.image)

    def test_foreign_function_hash_and_non_executable_range_are_rejected(self):
        self.spec['variants']['caller']['sha256']='0'*64;self.write_profile()
        with self.assertRaises(ValueError):check(self.profile,'caller',self.image)
        data=bytearray(self.image.read_bytes());struct.pack_into('<Q',data,72,6);self.image.write_bytes(data)
        self.spec['image_sha256']=hashlib.sha256(data).hexdigest();self.write_profile()
        with self.assertRaises(ValueError):check(self.profile,'buffer',self.image)

    def test_malformed_profiles_do_not_select_a_variant(self):
        original=json.dumps(self.spec)
        for begin,end in [(True,32),(32,16),(-1,32),(0,65537),(0,512*1024*1024+1)]:
            self.spec=json.loads(original);self.spec['variants']['caller'].update(begin=begin,end=end);self.write_profile()
            with self.assertRaises(ValueError):check(self.profile,'caller')
        for text in ('null','[]','{}',' '*32769):
            self.profile.write_text(text)
            with self.assertRaises((ValueError,TypeError)):check(self.profile,'caller')

    def test_saved_validation_is_private_exclusive_and_no_follow(self):
        output=self.root/'check.json';cmd=[sys.executable,str(ROOT/'tools/check_cpu_experiment.py'),
            '--profile',str(self.profile),'--variant','caller','--image',str(self.image),'--output',str(output)]
        run=subprocess.run(cmd,capture_output=True,text=True);self.assertEqual(run.returncode,0,run.stderr)
        self.assertEqual(output.stat().st_mode&0o777,0o600);before=output.read_bytes()
        self.assertEqual(subprocess.run(cmd,capture_output=True).returncode,24);self.assertEqual(output.read_bytes(),before)
        output.unlink();target=self.root/'target';target.write_text('untouched');output.symlink_to(target)
        self.assertEqual(subprocess.run(cmd,capture_output=True).returncode,24);self.assertEqual(target.read_text(),'untouched')

    def test_owner_launcher_selects_one_variant_and_explicit_base(self):
        root=self.root/'owner';(root/'work').mkdir(parents=True);(root/'tools').mkdir();(root/'packaging').mkdir()
        shutil.copy2(ROOT/'prueba_grafica_nativa.command',root/'launch.command')
        shutil.copy2(ROOT/'tools/check_cpu_experiment.py',root/'tools/check_cpu_experiment.py')
        shutil.copy2(self.profile,root/'work/cpu-experiment-profile.json')
        (root/'work/cpu-experiment-selection.txt').write_text('caller\n')
        (root/'packaging/test-arm64.sh').write_text('''#!/bin/bash
python3 - <<'PYCODE'
import json,os
with open(os.environ['TEST_CAPTURE'],'w') as out:
 json.dump({k:os.getenv(k) for k in ['SOTC_CPU_EXPERIMENT','SOTC_CPU_INTERPRET_RANGE','BB_JIT_DENY','BB_JIT','BB_SUBMIT_AHEAD']},out)
PYCODE
''')
        output=root/'env.json';env=dict(os.environ,TEST_CAPTURE=str(output),BB_JIT_DENY='mem',SOTC_CPU_INTERPRET_RANGE='foreign',BB_SUBMIT_AHEAD='99')
        for arg,label,interval in [(None,'caller','0x10:0x20'),('buffer','buffer','0x20:0x30'),('table','table','0x30:0x40'),('field578','field578','0x40:0x50'),('base','base',None)]:
            run=subprocess.run(['bash',str(root/'launch.command')]+([] if arg is None else [arg]),env=env,capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stderr)
            row=json.loads(output.read_text());self.assertEqual(row['SOTC_CPU_EXPERIMENT'],label);self.assertEqual(row['SOTC_CPU_INTERPRET_RANGE'],interval)
            self.assertIsNone(row['BB_JIT_DENY']);self.assertEqual(row['BB_JIT'],'1')
            self.assertEqual(row['BB_SUBMIT_AHEAD'],'0')
        output.unlink();(root/'work/cpu-experiment-selection.txt').write_text('caller;touch invalid')
        run=subprocess.run(['bash',str(root/'launch.command')],env=env,capture_output=True,text=True)
        self.assertEqual(run.returncode,24);self.assertFalse(output.exists())

    def test_offline_review_distinguishes_validated_entry_absence_and_mismatch(self):
        self.assertIsNone(cpu_experiment_review('ordinary legacy log',None))
        state=self.root/'state';state.mkdir();record=check(self.profile,'caller',self.image)
        (state/'cpu-experiment-check.json').write_text(json.dumps(record))
        text='GPU supervisor: settings='+json.dumps(dict(SOTC_CPU_EXPERIMENT='caller',SOTC_CPU_INTERPRET_RANGE='0x10:0x20'))+'\n'
        configured='CPU region interpreter configured: begin=0x10 end=0x20\n'
        entered='CPU region interpreter entered: offset=0x10\n'
        self.assertEqual(cpu_experiment_review(text+configured,state)['state'],'validated_not_observed')
        self.assertEqual(cpu_experiment_review(text+configured+entered,state)['state'],'validated_entered')
        self.assertEqual(cpu_experiment_review(text+configured+entered+entered,state)['state'],'evidence_mismatch')
        self.assertEqual(cpu_experiment_review(text+configured+entered.replace('0x10','0x30'),state)['state'],'evidence_mismatch')
        record['variant']='table';(state/'cpu-experiment-check.json').write_text(json.dumps(record))
        self.assertEqual(cpu_experiment_review(text+configured+entered,state)['state'],'evidence_mismatch')
        field_record=check(self.profile,'field578',self.image)
        (state/'cpu-experiment-check.json').write_text(json.dumps(field_record))
        field_text='GPU supervisor: settings='+json.dumps(dict(SOTC_CPU_EXPERIMENT='field578',SOTC_CPU_INTERPRET_RANGE='0x40:0x50'))+'\n'
        field_configured='CPU region interpreter configured: begin=0x40 end=0x50\n'
        field_entered='CPU region interpreter entered: offset=0x40\n'
        self.assertEqual(cpu_experiment_review(field_text+field_configured,state)['state'],'validated_not_observed')
        self.assertEqual(cpu_experiment_review(field_text+field_configured+field_entered,state)['state'],'validated_entered')
        self.assertEqual(cpu_experiment_review(field_text+field_configured+field_entered.replace('0x40','0x60'),state)['state'],'evidence_mismatch')
        record=check(self.profile,'base',self.image);(state/'cpu-experiment-check.json').write_text(json.dumps(record))
        baseline='GPU supervisor: settings='+json.dumps(dict(SOTC_CPU_EXPERIMENT='base',SOTC_CPU_INTERPRET_RANGE=None))+'\n'
        self.assertEqual(cpu_experiment_review(baseline,state)['state'],'validated_base')


if __name__=='__main__':unittest.main()
