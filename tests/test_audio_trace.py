"""Synthetic audio timing and actual guest-output metadata, without sound/GPU."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).parents[1]


class AudioTraceTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('clang'), 'C compiler required')
    def test_gap_wait_reset_and_deadline_metrics(self):
        code = r'''
#include "audio_trace_metrics.h"
#include "audio_queue_policy.h"
#include <assert.h>
int main(void) {
    assert(sotc_audio_queue_limit(256,8,4,1024,48000)==32768);
    assert(sotc_audio_queue_limit(256,8,4,1024,44100)==40960);
    assert(sotc_audio_queue_limit(256,1,2,1024,48000)==2048);
    assert(sotc_audio_queue_limit(2048,8,4,1024,48000)==65536);
    assert(sotc_audio_queue_limit(256,8,4,9216,48000)==294912);
    assert(sotc_audio_queue_limit(256,8,4,9472,48000)==-1);
    assert(sotc_audio_queue_limit(256,8,4,0,48000)==-1);
    assert(sotc_audio_queue_limit(256,8,4,INT_MAX,1)==-1);
    assert(sotc_audio_queue_limit(256,8,4,1024,0)==-1);
    SotcAudioMetrics s={0}; sotc_audio_reset(&s,100);
    sotc_audio_wait(&s,10,0,0); sotc_audio_wait(&s,20,1024,0);
    sotc_audio_wait(&s,30,-1,1);
    assert(s.input_samples==2 && s.input_zero_samples==1 && s.input_min==0 && s.input_max==1024);
    assert(s.queue_wait_ns==60 && s.max_queue_wait_ns==30 && s.queue_wait_failures==1);
    sotc_audio_submit(&s,1000,100,1000,2,8192);
    assert(s.buffers==1 && !s.max_gap_ns && !s.late_over_period);
    sotc_audio_submit(&s,1400,100,1100,3,8192);
    assert(s.max_gap_ns==400 && s.gaps_over_two_periods==1);
    assert(s.max_late_ns==300 && s.late_over_period==1 && s.max_lock_wait_ns==3);
    assert(s.bytes==16384);
    sotc_audio_reset(&s,1500);
    assert(s.last_submit_ns==1400 && !s.buffers && !s.queue_wait_ns && s.input_min==INT_MAX);
    sotc_audio_submit(&s,1600,100,1600,0,8192);
    assert(s.max_gap_ns==200 && !s.gaps_over_two_periods); /* Strictly greater than 2 periods. */
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); (root/'metrics.c').write_text(code)
            subprocess.run(['clang', '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror',
                            '-I'+str(ROOT/'runtime/src'), str(root/'metrics.c'), '-o', str(root/'metrics')], check=True)
            subprocess.run([str(root/'metrics')], check=True)

    @unittest.skipUnless((ROOT/'work/test_audio_output').is_file(), 'built native AudioOut fixture required')
    def test_real_guest_output_keeps_pcm_and_failure_contracts_with_tracing(self):
        for jit in ('0', '1'):
            with self.subTest(jit=jit), tempfile.TemporaryDirectory(prefix='audio metadata ') as temp:
                root = Path(temp)
                run = subprocess.run([str(ROOT/'work/test_audio_output')],
                    env=dict(os.environ, BB_JIT=jit, BB_GPU_USER_DIR=str(root), SOTC_AUDIO_TRACE='1'),
                    capture_output=True, text=True, timeout=20)
                self.assertEqual(run.returncode, 0, run.stdout+run.stderr)
                events = [json.loads(line) for line in (root/'audio.jsonl').read_text().splitlines()]
                self.assertTrue(events)
                windows = [e for e in events if e['event'] == 'audio_window']
                self.assertTrue(any(e['accepted_buffers'] > 0 for e in windows))
                self.assertTrue(any(e['device_frames'] == 1024 and e['input_queue_limit_bytes'] == 32768 for e in windows))
                self.assertTrue(any(e['input_queue_zero_samples'] > 0 for e in windows))
                self.assertTrue(any(e['queue_wait_failures'] == 1 and e['max_queue_wait_us'] >= 1_900_000 for e in windows))
                self.assertTrue(any(e['event'] == 'audio_output_error' and e['result'] < 0 for e in events))
                states = [e for e in events if e['event'] == 'audio_backend_state']
                timeouts = [e for e in states if e['stage'] == 'queue_timeout']
                self.assertEqual(len(timeouts), 3)
                self.assertEqual([e['input_queue_bytes'] for e in timeouts], [65536, 0, 0])
                self.assertTrue(all(e['failed_wait_first_bytes'] == 65536 and
                                    e['failed_wait_last_bytes'] == 65536 and
                                    e['failed_wait_min_bytes'] == 65536 and
                                    e['failed_wait_changes'] == 0 and e['failed_wait_polls'] > 1
                                    for e in timeouts))
                self.assertTrue(all(e['paused_result'] == 1 and e['device_format_ok'] == 1
                                    and e['device_frames'] == 1024 for e in timeouts))
                self.assertTrue(all(e['provider_error'] == '' and not e['provider_error_captured']
                                    for e in timeouts))
                put = [e for e in states if e['stage'] == 'put']
                self.assertEqual(len(put), 3)
                self.assertTrue(all(e['provider_error'] == 'synthetic "put"\\failure\n' for e in put))
                self.assertTrue(all(e['failure_group'] <= 8 for e in states))
                self.assertFalse(any('underrun' in str(e) or 'pcm' in e for e in events))
                self.assertLessEqual(len(events), 1024)

    @unittest.skipUnless((ROOT/'work/test_audio_output').is_file(), 'built native AudioOut fixture required')
    def test_missing_optional_getters_keep_output_and_mark_unknown(self):
        for jit in ('0', '1'):
            with self.subTest(jit=jit), tempfile.TemporaryDirectory(prefix='audio optional ') as temp:
                run = subprocess.run([str(ROOT/'work/test_audio_output'), '--missing-diagnostics'],
                    env=dict(os.environ, BB_JIT=jit, BB_GPU_USER_DIR=temp, SOTC_AUDIO_TRACE='1'),
                    capture_output=True, text=True, timeout=20)
                self.assertEqual(run.returncode, 0, run.stdout+run.stderr)
                events = [json.loads(line) for line in (Path(temp)/'audio.jsonl').read_text().splitlines()]
                states = [e for e in events if e['event'] == 'audio_backend_state']
                self.assertTrue(states)
                self.assertTrue(all(e['paused_result'] == -1 and e['available_output_bytes'] == -1
                                    and not e['provider_error_captured'] for e in states))


if __name__ == '__main__':
    unittest.main()
