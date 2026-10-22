"""CPU-only evidence and report checks, including incomplete logs and unsafe paths."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).parents[1]
spec = importlib.util.spec_from_file_location('visual', ROOT/'tools/analyze_visual_trace.py')
visual = importlib.util.module_from_spec(spec)
spec.loader.exec_module(visual)


class VisualReportTests(unittest.TestCase):
    def test_partial_tail_escaped_errors_and_strict_local_images(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)/'visual'
            root.mkdir()
            (root/'frame-000001.ppm').write_bytes(b'P6\n1 1\n255\n\xff\0\0')
            event = {'schema': 1, 't_s': 0, 'event': 'pixels', 'file': 'frame-000001.ppm'}
            bad = dict(event, t_s=1, event='native_command_error', description='<script>alert(1)</script>')
            (root/'frames.jsonl').write_text(json.dumps(event)+'\n'+json.dumps(bad)+'\n{"partial":')
            report = visual.render(root)
            self.assertIn('data:image/png;base64,', report)
            self.assertIn('&lt;script&gt;', report)
            self.assertNotIn('<script>', report)
            self.assertIn('partial record ignored', report)
            audio = {'schema': 1, 'event': 'audio_window', 'boundary': '<script>', 'accepted_buffers': 470,
                     'frames': 256, 'window_us': 5_000_000}
            (root.parent/'audio.jsonl').write_text(json.dumps(audio)+'\n{"partial":')
            try:
                self.assertIn('Audio delivery evidence', visual.render(root))
                self.assertIn('&lt;script&gt;', visual.render(root))
                self.assertIn('24064.0', visual.render(root))
                self.assertEqual(len(visual.load_audio(root)), 1)
            finally:
                (root.parent/'audio.jsonl').unlink()
            self.assertTrue(visual.preview_png(root, event['file']).startswith(b'\x89PNG'))
            for name in ('../frame-000001.ppm', '/etc/passwd', 'frame-1.ppm'):
                with self.assertRaises(ValueError): visual.preview_png(root, name)
            (root/'frame-000002.ppm').symlink_to(root/'frame-000001.ppm')
            with self.assertRaises(ValueError): visual.preview_png(root, 'frame-000002.ppm')
            (root/'frame-000003.ppm').write_bytes(b'P6\n0 1\n255\n')
            with self.assertRaises(ValueError): visual.preview_png(root, 'frame-000003.ppm')
            (root/'frames.jsonl').write_text(json.dumps(dict(event, t_s=float('nan'))))
            self.assertEqual(visual.load_events(root)[0], [])

    @unittest.skipUnless(shutil.which('clang++'), 'C++ compiler required')
    def test_real_recorder_is_bounded_and_capture_records_survive_rotation(self):
        with tempfile.TemporaryDirectory(prefix='visual CPU fixture ') as temp:
            root = Path(temp)
            binary = root/'fixture'
            subprocess.run(['clang++', '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror', '-pthread',
                            '-I'+str(ROOT/'runtime/src'), str(ROOT/'tests/test_visual_trace.cpp'),
                            '-o', str(binary)], check=True, capture_output=True, text=True)
            env = dict(os.environ, BB_GPU_USER_DIR=str(root), SOTC_VISUAL_TRACE='0', SOTC_VISUAL_CAPTURE='1')
            subprocess.run([str(binary)], env=env, check=True, capture_output=True, text=True)
            self.assertFalse((root/'visual').exists())
            env['SOTC_VISUAL_TRACE'] = '1'
            subprocess.run([str(binary)], env=env, check=True, capture_output=True, text=True, timeout=20)
            directory = root/'visual'
            for name in ('frames.previous.jsonl', 'frames.jsonl'):
                self.assertLessEqual((directory/name).stat().st_size, 4 << 20)
            events, warnings = visual.load_events(directory)
            self.assertFalse(warnings)
            pixels = [e for e in events if e['event'] == 'pixels']
            self.assertEqual(len(pixels), 2)
            self.assertEqual(pixels[1]['identical_preview'], 1)
            self.assertEqual(events[-1]['event'], 'native_command_error')
            self.assertEqual(events[-1]['counters']['draw_requests'], 100)
            self.assertEqual(events[-1]['counters']['missing_pipeline_draws'], 100)
            self.assertEqual(events[-1]['staging_allocations'], [1,2,3,4,5,6])
            self.assertEqual(events[-1]['staging_reusable_bytes'], [10,11,12])
            self.assertGreaterEqual(events[-1]['staging_sample_age_ms'], 0)
            self.assertEqual(len(list(directory.glob('frame-*.ppm'))), 2)
            subprocess.run([os.sys.executable, str(ROOT/'tools/analyze_visual_trace.py'), str(directory)],
                           check=True, capture_output=True, text=True)
            self.assertIn('2 completed pixel samples', (directory/'report.html').read_text())


if __name__ == '__main__':
    unittest.main()
