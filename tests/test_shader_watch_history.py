"""Synthetic lifecycle retention, contention and disabled controls; no guest input."""
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from analyze_event_journal import analyze
from analyze_shader_watch import transition_review
from analyze_shader_watch_summary import review as summary_review

@unittest.skipUnless(platform.system()=='Darwin' and platform.machine()=='arm64','ARM64 required')
class WatchHistoryTests(unittest.TestCase):
    def test_retention_threads_and_disabled(self):
        source=r'''
#include <assert.h>
#include <pthread.h>
#include "cpu_fault_diagnostics.h"
#include "cpu_field_trace.h"
#include "cpu_pointer_trace.h"
unsigned runtime_memory_trap_reasons(uintptr_t p){(void)p;return 0;}
int bootstrap_cpu_failure_state(SotcGuestFailureState* s){memset(s,0,sizeof(*s));return 0;}
static void* worker(void* unused){
    (void)unused;
    for(unsigned i=0;i<2000;++i)sotc_shader_prefix_register(0x100000,100+i,1100+i);
    return NULL;
}
int main(void){
    assert(!sotc_pointer_init("0:0:0:0:2:2"));
    sotc_event_journal_init(&sotc_pointer_journal,"pointer-events.bin",3,128);
    sotc_shader_prefix_init();
    if(!sotc_shader_prefix_enabled){sotc_shader_prefix_register(0x100000,1,2);assert(!sotc_shader_watch_journal.map);return 0;}
    const uint64_t target=0x100000;
    sotc_shader_prefix_register(target,10,1010);
    sotc_shader_prefix_register(target,20,1020);
    uint64_t out[8];assert(sotc_shader_prefix_snapshot(target,out)==5&&out[1]==20&&out[2]==1020);
    unsigned bucket=sotc_shader_prefix_bucket(target);uint64_t other=target+256;
    for(unsigned i=0;i<4;++i){
        while(sotc_shader_prefix_bucket(other)!=bucket)other+=256;
        sotc_shader_prefix_register(other,30+i,1030+i);other+=256;
    }
    assert(sotc_shader_prefix_snapshot(target,out)==2);
    sotc_shader_prefix_register(target,40,1040); /* deterministic re-arm before contention */
    /* Heavy registration traffic cannot overwrite the separate eviction channel. */
    pthread_t threads[4];
    for(unsigned i=0;i<4;++i)assert(!pthread_create(threads+i,NULL,worker,NULL));
    for(unsigned i=0;i<4;++i)assert(!pthread_join(threads[i],NULL));
    atomic_store(&sotc_shader_prefix_bucket_busy[bucket],1);
    sotc_shader_prefix_register(target,8000,9000);
    atomic_store(&sotc_shader_prefix_bucket_busy[bucket],0);
    assert(sotc_shader_prefix_snapshot(target,out)==5);
    assert(out[2]==out[1]+1000);
    /* Freeze must not append registrations. */
    uint64_t count=sotc_shader_watch_journal.map[5];atomic_store(&sotc_trace_frozen,1);
    sotc_shader_prefix_register(target,9,1009);assert(sotc_shader_watch_journal.map[5]==count);
    return 0;
}
'''
        with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
            folder=Path(folder);c=folder/'test.c';c.write_text(source);exe=folder/'test'
            subprocess.run(['clang','-arch','arm64','-O1','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function','-pthread',
                '-fsanitize=undefined','-fno-sanitize-recover=all',
                '-I'+str(ROOT/'runtime/src'),'-I'+str(ROOT/'runtime/upstream/src'),
                '-I'+str(ROOT/'runtime/upstream/recomp/src/cpu'),'-I'+str(ROOT/'work/deps-arm64/include'),
                str(c),str(ROOT/'runtime/src/platform_arm64.c'),'-o',str(exe)],check=True)
            for enabled in ('0','1'):
                state=folder/enabled;state.mkdir()
                subprocess.run([str(exe)],env=dict(os.environ,BB_GPU_USER_DIR=str(state),SOTC_DIAGNOSTIC_JOURNAL='1',SOTC_SHADER_COPY_TRACE=enabled),check=True,timeout=20,capture_output=True)
                journal=analyze(state/'shader-watch-events.bin','shader_watch')
                if enabled=='0':
                    self.assertEqual(journal['state'],'absent')
                    self.assertFalse((state/'shader-watch-summary.bin').exists());continue
                self.assertEqual(journal['state'],'readable')
                summary=summary_review(state/'shader-watch-summary.bin',0x100000)
                self.assertEqual(summary['state'],'observed')
                self.assertEqual(summary['summary']['evictions'],1)
                self.assertEqual(summary['summary']['arms'],2)
                self.assertGreater(summary['summary']['refreshes'],512)
                self.assertGreater(summary['summary']['registration_drops'],0)
                self.assertEqual(summary['continuity'],'known_gap_or_registration_failure')
                self.assertEqual(summary['summary']['last_capture'],summary['summary']['last_packet']+1000)

                for event in journal['events']:
                    self.assertEqual(event['capture_id'],event['packet']+1000)
                    self.assertEqual(event['watched_bytes'],8)
                self.assertTrue(any(c['overwritten'] for c in journal['channels']))
                result=transition_review(journal,0x100000)
                self.assertGreater(result['relevant_channel_losses'],0)
                self.assertGreater(result['registration_drops'],0)
                # Forced earlier eviction remains even after >512 refreshes.
                self.assertTrue(any(e['action']=='evict' and e['capture_id']==1020 for e in result['events']))
                self.assertEqual(result['allocation_identity'],'unknown')
                self.assertNotEqual(result['continuity'],'continuous')
                truncated=state/'bad.bin';truncated.write_bytes((state/'shader-watch-events.bin').read_bytes()[:-1])
                self.assertEqual(analyze(truncated,'shader_watch')['state'],'invalid')

if __name__=='__main__':unittest.main()
