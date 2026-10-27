"""Original controller tests with shell exit fixtures; no native renderer."""
import copy
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from run_adaptive_diagnostics import digest,validate_request,run_request

class AdaptiveTests(unittest.TestCase):
    def setup_root(self,root):
        names=['work/probe_entry_arm64','work/gpu-build-a27f0ca477bd/libbbgpu.dylib','work/cpu-pointer-profile.json']
        for name in names:
            p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('original synthetic fixture')
        (root/'logs').mkdir();batch=root/'work/analysis/batch';batch.mkdir(parents=True)
        request=dict(schema=1,round=1,variant='base',seconds=180,reason='original synthetic controller fixture',
                     fingerprints={name:digest(root/name) for name in names})
        return request,batch
    def test_reject_unreviewed_changed_and_unbounded_requests(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);request,batch=self.setup_root(root)
            self.assertEqual(validate_request(request,root),request)
            self.assertEqual(validate_request(dict(request, seconds=500), root)['seconds'], 500)
            self.assertEqual(validate_request(dict(request, round=10001), root)['round'], 10001)
            for changes in [dict(seconds=0),dict(seconds=501),dict(variant='base;echo injected'),dict(round=0),dict(command='anything')]:
                with self.assertRaises(ValueError):validate_request(dict(request,**changes),root)
            bad=copy.deepcopy(request);bad['fingerprints']['../outside']='0'*64
            with self.assertRaises(ValueError):validate_request(bad,root)
            (root/'work/probe_entry_arm64').write_text('changed fixture')
            with self.assertRaises(ValueError):validate_request(request,root)
    def test_finished_child_and_missing_or_unsafe_supervisor_evidence(self):
        for kind,code,line,safe in [
            ('deadline',21,'GPU supervisor: finished elapsed=1s child_returncode=21 exit=21 control_notice_seen=1',True),
            ('fault',139,'GPU supervisor: finished elapsed=1s child_returncode=139 exit=139 control_notice_seen=0',True),
            ('stall',31,'GPU supervisor: finished elapsed=1s child_returncode=21 exit=31 control_notice_seen=1',False),
            ('incomplete',21,'incomplete synthetic log',False),
            ('cleanup',21,'STOP_GPU_SUPERVISOR_CLEANUP: fixture\nGPU supervisor: finished elapsed=1s child_returncode=21 exit=21 control_notice_seen=1',False)]:
            with self.subTest(kind=kind),tempfile.TemporaryDirectory() as folder:
                root=Path(folder);request,batch=self.setup_root(root)
                (root/'prueba_grafica_nativa.command').write_text("#!/bin/bash\ncat > logs/graphics-game-fixture.log <<'EOF'\n"+line+'\nEOF\nexit '+str(code)+'\n')
                result=run_request(request,root,batch)
                self.assertEqual(result['safe_to_continue'],safe)
                self.assertEqual(result['returncode'],code)
                self.assertTrue((batch/'round-01-result.json').is_file())
    def test_incomplete_previous_run_prevents_launch(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);request,batch=self.setup_root(root)
            (root/'logs/graphics-game-previous.log').write_text('active fixture')
            with self.assertRaises(RuntimeError):run_request(request,root,batch)

if __name__=='__main__':unittest.main()
