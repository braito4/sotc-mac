"""Bounded resource evidence, complete vs deferred lifetimes, and report input safety."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('memory_report', ROOT/'tools/analyze_visual_trace.py')
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)


@unittest.skipUnless(shutil.which('clang++'), 'C++ compiler required')
class MemoryTraceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix='memory trace CPU ')
        cls.root = Path(cls.temporary.name)
        cls.binary = cls.root/'fixture'
        subprocess.run(['clang++','-std=c++20','-O1','-Wall','-Wextra','-Werror','-pthread',
                        '-I'+str(ROOT/'runtime/src'),str(ROOT/'tests/test_memory_trace.cpp'),
                        '-o',str(cls.binary)],check=True,capture_output=True,text=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def run_fixture(self, name, mode='normal', enabled='1'):
        root = self.root/name
        root.mkdir()
        result = subprocess.run([str(self.binary),mode],check=True,capture_output=True,text=True,timeout=20,
                                env=dict(os.environ,BB_GPU_USER_DIR=str(root),SOTC_MEMORY_TRACE=enabled))
        return root, result

    @staticmethod
    def read(path):
        return [json.loads(line) for line in path.read_text().splitlines()]

    def test_disabled_does_not_create_output(self):
        root, result = self.run_fixture('disabled',enabled='0')
        self.assertEqual(list(root.iterdir()), [])
        self.assertNotIn('MEMORY_TRACE',result.stderr)

    def test_retired_images_remain_live_until_actual_destroy(self):
        root, _ = self.run_fixture('lifetimes')
        directory = root/'memory'
        last = self.read(directory/'summary.jsonl')[-1]
        self.assertEqual(last['event'],'memory_failure')
        self.assertEqual((last['code'],last['native_allocated_bytes']),(8,111))
        self.assertEqual((last['allocated_count'],last['destroyed_count'],last['live_count']),(3,1,2))
        self.assertEqual(last['live_bytes'],72<<20)
        self.assertEqual(last['tracked_bytes'],last['live_bytes'])
        self.assertEqual(last['state_bytes'],[0,8<<20,0,64<<20])
        self.assertEqual(last['tracking_overflow'],0)
        self.assertEqual([x['id'] for x in last['largest']],[1,3])
        self.assertGreater(last['largest'][0]['retired_ns'],0)
        self.assertEqual(last['large_copy_requests'],1)
        self.assertEqual(last['large_copy_completed_bytes'],3<<30)
        self.assertEqual(last['gc_kept_tiled_download'],1)
        self.assertEqual(last['gc_kept_download_no_pressure'],1)
        self.assertEqual(last['gc_kept_unresolved_gpu'],1)
        self.assertEqual(last['gc_kept_window_sample'],23)
        self.assertEqual(last['gc_freed_window_sample'],24)
        buffers = self.read(directory/'buffers.jsonl')
        self.assertEqual(buffers[0]['event'],'pool_window_cross')
        self.assertTrue(all(x['shader']==123 and x['slot']==4 for x in buffers))
        self.assertTrue(all(x['binding_address']==4096 for x in buffers))
        self.assertEqual(last['observation_errors'],0)
        self.assertEqual(buffers[-1]['elapsed_ns'],77)
        visual=root/'visual';visual.mkdir()
        rendered=report.render_memory(visual)
        self.assertIn('pool_window_cross',rendered)
        self.assertIn('Largest images still allocated',rendered)
        self.assertIn('8192',rendered)
        legacy=dict(last)
        for key in ('kept','freed'):
            legacy['gc_'+key+'_total']=legacy.pop('gc_'+key+'_window_sample')
        (directory/'summary.jsonl').write_text(json.dumps(legacy)+'\n')
        self.assertIn('gc_kept_window_sample',report.render_memory(visual))
        self.assertNotIn('<td>gc_kept_total</td>',report.render_memory(visual))
        with (directory/'buffers.jsonl').open('a') as out:
            out.write(json.dumps({'schema':1,'monotonic_ns':10,'event':'<script>alert(1)</script>'})+'\n{"partial":')
        rendered=report.render_memory(visual)
        self.assertIn('&lt;script&gt;',rendered)
        self.assertNotIn('<script>',rendered)
        self.assertIn('partial record ignored',rendered)

    def test_caps_rotation_concurrency_and_final_failure_survive(self):
        root, _ = self.run_fixture('stress',mode='stress')
        directory = root/'memory'
        for name in ('images.jsonl','buffers.jsonl'):
            self.assertLessEqual((directory/name).stat().st_size,4096)
            self.assertTrue(self.read(directory/name))
        self.assertLessEqual((directory/'summary.previous.jsonl').stat().st_size,32768)
        self.assertLessEqual((directory/'summary.jsonl').stat().st_size,32768)
        last = self.read(directory/'summary.jsonl')[-1]
        self.assertEqual((last['allocated_count'],last['destroyed_count'],last['live_count']),(400,400,0))
        self.assertEqual(last['tracked_count'],0)
        self.assertEqual(last['unmatched_frees'],0)
        self.assertGreater(last['dropped_records'][0],0)
        self.assertGreater(last['dropped_records'][1],0)
        self.assertEqual(last['large_copy_requests'],100)
        self.assertEqual(last['event'],'memory_failure')

    def test_existing_or_redirected_directory_preserved(self):
        root=self.root/'redirected';root.mkdir()
        outside=self.root/'outside';outside.mkdir()
        (root/'memory').symlink_to(outside,target_is_directory=True)
        subprocess.run([str(self.binary),'normal'],check=True,capture_output=True,text=True,timeout=10,
                       env=dict(os.environ,BB_GPU_USER_DIR=str(root),SOTC_MEMORY_TRACE='1'))
        self.assertEqual(list(outside.iterdir()),[])
        visual=root/'visual';visual.mkdir()
        self.assertEqual(report.load_memory(visual)['summary'],[])

    def test_diagnostic_exceptions_preserve_execution_and_mark_incomplete_evidence(self):
        root,_=self.run_fixture('errors',mode='errors')
        last=self.read(root/'memory/summary.jsonl')[-1]
        self.assertEqual(last['observation_errors'],1)


if __name__ == '__main__':
    unittest.main()
