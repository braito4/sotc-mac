"""No GPU: mutually exclusive probes and fingerprinted controller dispatch."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from run_adaptive_diagnostics import digest, validate_request, run_request, MEMORY_PROFILES
ROOT = Path(__file__).resolve().parents[1]

class MemoryProfiles(unittest.TestCase):
    def test_profiles_clear_inherited_probes(self):
        script = ROOT / 'prueba_memoria.command'
        code = script.read_text().split('exec bash ')[0]
        code += "\npython3 -c 'import os,json; print(json.dumps(dict(os.environ)))'\n"
        for profile in MEMORY_PROFILES:
            env = dict(os.environ, SOTC_MEMORY_TRACE='1', SOTC_GPU_SUBMIT_TRACE='1',
                       SOTC_GPU_ENCODER_PROGRESS='1', SOTC_CPU_POINTER_TRACE='stale',
                       SOTC_SHADER_COPY_TRACE='1', SOTC_AUDIO_TRACE='1', SOTC_METAL_INFLIGHT='8')
            result = subprocess.run(['bash','-c',code,str(script),profile],env=env,
                                    capture_output=True,text=True,check=True)
            values = json.loads(result.stdout.splitlines()[-1])
            self.assertEqual(values['SOTC_MEMORY_TRACE'],str(int(profile=='memory-resources')))
            for key in ['SOTC_GPU_SUBMIT_TRACE','SOTC_GPU_ENCODER_PROGRESS']:
                self.assertEqual(values[key],str(int(profile=='memory-render')))
            for key in ['SOTC_SHADER_COPY_TRACE','SOTC_AUDIO_TRACE','SOTC_DIAGNOSTIC_JOURNAL']:
                self.assertEqual(values[key],'0')
            self.assertEqual(values['SOTC_CPU_POINTER_TRACE'],'')
            self.assertEqual(values['SOTC_METAL_INFLIGHT'],'1')
            self.assertEqual(values['SOTC_SKIP_INTRO'],'1')

    def test_reviewed_profile_dispatch_and_rejections(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); (root/'logs').mkdir(); batch=root/'batch';batch.mkdir()
            names=['work/probe_entry_arm64','work/gpu-build-a27f0ca477bd/libbbgpu.dylib',
                   'prueba_memoria.command','packaging/test-arm64.sh','tools/run_adaptive_diagnostics.py',
                   'work/startup-no-intro-profile.json']
            for name in names:
                p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('fixture')
            (root/'prueba_memoria.command').write_text('''#!/bin/bash
[[ "$1" == memory-resources ]] || exit 99
printf 'GPU supervisor: finished child_returncode=21 exit=21 control_notice_seen=1\\n' > logs/graphics-game-fixture.log
exit 21
''')
            request=dict(schema=2,round=1,variant='base',seconds=500,reason='memory resources fixture',
                         profile='memory-resources',fingerprints={n:digest(root/n) for n in names})
            self.assertEqual(validate_request(request,root),request)
            for change in [dict(profile='all'),dict(profile='memory-render;echo bad'),dict(variant='caller')]:
                with self.assertRaises(ValueError):validate_request(dict(request,**change),root)
            row=run_request(request,root,batch)
            self.assertTrue(row['safe_to_continue']);self.assertEqual(row['profile'],'memory-resources')
            (root/'prueba_memoria.command').write_text('changed')
            with self.assertRaises(ValueError):validate_request(request,root)

if __name__=='__main__': unittest.main()
