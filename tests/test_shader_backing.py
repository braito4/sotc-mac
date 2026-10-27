"""Original protected overlap fixtures for backing writes, no game/GPU."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from analyze_event_journal import analyze
from analyze_shader_backing import review

class BackingTests(unittest.TestCase):
    def test_numeric_candidates_keep_different_registration_explicit(self):
        origin=dict(lookup=dict(address=256,ns=500,head=[3,4]))
        submission=dict(sample=dict(submit_ns=100,capture_id=42,packet=8))
        event=dict(field=256,ns=200,capture_id=43,packet=16,head_available=True,head=[3,4])
        def run(events):return review(origin,submission,dict(state='readable',events=events))
        result=run([event]);self.assertEqual(result['state'],'numeric_overlap_candidates')
        self.assertFalse(result['events'][0]['registration_matches_pre_enqueue'])
        self.assertTrue(result['events'][0]['head_matches_lookup'])
        for change in [dict(field=512),dict(ns=50),dict(ns=600)]:
            self.assertEqual(run([dict(event,**change)])['state'],'matching_candidates_absent')
        prior=dict(event,ns=50,capture_id=42,packet=8,phase='prior_writable_buffer_binding')
        scan=dict(field=256,ns=100,capture_id=42,packet=8,phase='prior_binding_scan')
        result=run([prior,scan]);self.assertEqual(result['state'],'numeric_overlap_candidates')
        self.assertEqual(result['events'][0]['phase'],'prior_writable_buffer_binding')
        self.assertEqual(result['prior_binding_scan']['phase'],'prior_binding_scan')

    def test_protected_prefix_overlap_and_result_observations(self):
        source=r'''
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "cpu_fault_diagnostics.h"
#include "cpu_field_trace.h"
#include "cpu_pointer_trace.h"
#include "shader_backing_trace.h"
unsigned runtime_memory_trap_reasons(uintptr_t a){(void)a;return 0;}
int bootstrap_cpu_failure_state(SotcGuestFailureState*s){memset(s,0,sizeof(*s));return 0;}
static void(*observer)(uintptr_t,const void*,uint64_t,unsigned,int,uintptr_t);
void sotc_memory_set_backing_write_observer(void(*fn)(uintptr_t,const void*,uint64_t,unsigned,int,uintptr_t)){observer=fn;}
int main(void){
 assert(!sotc_pointer_init("0:0:0:0:2:2"));
 sotc_event_journal_init(&sotc_pointer_journal,"pointer-events.bin",3,128);
 sotc_shader_prefix_init();sotc_shader_backing_init();assert(observer);
 uint64_t* memory=bb_low_map(16384,PROT_READ|PROT_WRITE);assert(memory);
 uint64_t* dst=memory+32;*dst=UINT64_C(0x2cbeeb03ff);
 uint64_t input=UINT64_C(0x1122334455667788);
 sotc_shader_backing_binding((uintptr_t)dst-4,16,84,2,16,0);
 sotc_shader_prefix_register((uintptr_t)dst,8,42);
 sotc_shader_backing_prefix_registered((uintptr_t)dst,8,42);
 errno=ERANGE;observer((uintptr_t)dst,&input,8,0,0,123);assert(errno==ERANGE);
 memcpy(dst,&input,8);observer((uintptr_t)dst,&input,8,1,1,123);
 sotc_shader_backing_cpu_write((uintptr_t)dst,input,0,8,0xabc,1);
 sotc_shader_backing_cpu_write((uintptr_t)dst+4,UINT64_C(0x8877665544332211),UINT64_C(0x1020304050607080),16,0xdef,2);
 sotc_shader_backing_cpu_write((uintptr_t)dst+128,input,0,8,0x122,1); /* same tracked bucket, no overlap */
 sotc_shader_backing_cpu_write((uintptr_t)dst+4096,input,0,8,0x123,1); /* eligible, non-overlap */
 observer((uintptr_t)dst+3,&input,2,0,0,124);observer((uintptr_t)dst+3,&input,2,1,0,124);
 observer((uintptr_t)dst,&input,0,0,0,123);observer((uintptr_t)dst+256,&input,8,0,0,123);
 void* guard=bb_low_map(16384,PROT_NONE);assert(guard);
 sotc_shader_prefix_register((uintptr_t)guard,16,43);observer((uintptr_t)guard,&input,8,0,0,125);
 observer((uintptr_t)dst,NULL,8,2,0,126);
 sotc_shader_backing_observe((uintptr_t)dst,&input,8,3,0,127,999);
 sotc_shader_backing_binding((uintptr_t)dst,8,85,3,16,0);
 sotc_shader_backing_binding((uintptr_t)dst,8,86,4,17,1);
 sotc_shader_backing_binding((uintptr_t)dst,8,87,5,18,2);
 sotc_shader_backing_binding((uintptr_t)dst,8,88,6,19,3);
 sotc_shader_backing_binding((uintptr_t)dst,8,89,7,20,4);
 for(uint64_t i=0;i<=SOTC_SHADER_BINDING_HISTORY;++i)
   sotc_shader_backing_binding_history(UINT64_C(0x100000)+i*256,64,UINT64_C(0xabc),5,64,0);
 sotc_shader_prefix_register(UINT64_C(0x200000),9,44);
 sotc_shader_backing_prefix_registered(UINT64_C(0x200000),9,44);
 sotc_shader_backing_binding_history(UINT64_C(0x300000),64,UINT64_C(0xdef),7,64,3);
 sotc_shader_prefix_register(UINT64_C(0x300000),10,45);
 sotc_shader_backing_prefix_registered(UINT64_C(0x300000),10,45);
 assert(sotc_shader_backing_journal.map[256]==3&&sotc_shader_backing_journal.map[257]==2);
 assert(sotc_shader_backing_journal.map[258]==1);
 assert(sotc_shader_backing_journal.map[259]==1);
 assert(sotc_shader_backing_journal.map[260]==4&&sotc_shader_backing_journal.map[261]==2);
 assert(sotc_shader_backing_journal.map[262]==1&&sotc_shader_backing_journal.map[263]==1);
 assert(sotc_shader_backing_journal.map[274]==2&&sotc_shader_backing_journal.map[275]==1);
 assert(sotc_shader_backing_journal.map[276]==1&&sotc_shader_backing_journal.map[277]==1);
 assert(sotc_shader_backing_journal.map[285]==1&&sotc_shader_backing_journal.map[286]==1);
 assert(sotc_shader_backing_journal.map[287]==1&&sotc_shader_backing_journal.map[288]==1);
 assert(sotc_shader_backing_journal.map[289]==1&&sotc_shader_backing_journal.map[290]==1);
 assert(sotc_shader_backing_journal.map[278]==4096&&sotc_shader_backing_journal.map[279]==4104);
 assert(sotc_shader_backing_journal.map[280]==0&&sotc_shader_backing_journal.map[281]==8);
 assert(sotc_shader_backing_journal.map[282]==3&&sotc_shader_backing_journal.map[284]==3);
 return 0;
}
'''
        with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
            p=Path(folder);(p/'test.c').write_text(source)
            subprocess.run(['clang','-O1','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function','-pthread',
                '-I'+str(ROOT/'runtime/src'),'-I'+str(ROOT/'runtime/upstream/src'),
                '-I'+str(ROOT/'runtime/upstream/recomp/src/cpu'),'-I'+str(ROOT/'work/deps-arm64/include'),
                str(p/'test.c'),str(ROOT/'runtime/src/platform_arm64.c'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],env=dict(os.environ,BB_GPU_USER_DIR=str(p),
                SOTC_DIAGNOSTIC_JOURNAL='1',SOTC_SHADER_COPY_TRACE='1'),check=True)
            result=analyze(p/'shader-backing-events.bin','shader_backing')
            self.assertEqual(result['state'],'readable');self.assertEqual(result['incomplete_or_stale'],0)
            events=result['events'];self.assertEqual(len(events),20)
            cpu=[event for event in events if event['phase']=='cpu_direct_store']
            self.assertEqual(len(cpu),2)
            self.assertEqual((cpu[0]['cpu_store_route'],cpu[0]['value_low'],cpu[0]['value_high'],cpu[0]['guest_instruction']),
                ('scalar',0x1122334455667788,0,0xabc))
            self.assertEqual((cpu[1]['cpu_store_route'],cpu[1]['write_bytes'],cpu[1]['value_low'],cpu[1]['value_high'],cpu[1]['guest_instruction']),
                ('vector',16,0x8877665544332211,0x1020304050607080,0xdef))
            self.assertEqual(result['observation_counters']['cpu_direct_stores'],dict(scalar=1,vector=1))
            self.assertEqual(result['eligible_call_counters']['cpu_direct_stores'],dict(scalar=3,vector=1))
            self.assertEqual(result['eligible_call_counters']['cpu_direct_store_candidates'],{
                'shader_page':dict(scalar=2,vector=1),'shader_bucket':dict(scalar=2,vector=1)})
            ordinary=[event for event in events if event['phase'] not in ('prior_binding_scan','prior_writable_buffer_binding','cpu_direct_store')]
            self.assertEqual(ordinary[0]['head'],[0xbeeb03ff,0x2c]);self.assertEqual(ordinary[1]['head'],[0x55667788,0x11223344])
            self.assertTrue(ordinary[1]['write_succeeded']);self.assertFalse(ordinary[3]['write_succeeded'])
            self.assertFalse(ordinary[4]['head_available']);self.assertEqual(ordinary[1]['capture_id'],42)
            self.assertEqual(ordinary[1]['host_caller_candidate'],123)
            self.assertEqual(ordinary[5]['phase'],'write_notification');self.assertIsNone(ordinary[5]['write_succeeded'])
            self.assertEqual(ordinary[6]['phase'],'gpu_write_intent');self.assertEqual(ordinary[6]['gpu_write_source'],'eop')
            self.assertEqual(ordinary[6]['fence_sequence'],999);self.assertIsNone(ordinary[6]['write_succeeded'])
            self.assertEqual(result['eligible_call_counters']['before'],4)
            self.assertEqual(result['eligible_call_counters']['gpu_by_source']['eop'],1)
            self.assertEqual(ordinary[7]['phase'],'writable_buffer_binding');self.assertIsNone(ordinary[7]['write_succeeded'])
            self.assertEqual(ordinary[7]['channel'],1);self.assertEqual(ordinary[7]['shader'],85)
            self.assertEqual(ordinary[7]['descriptor_slot'],3);self.assertEqual(ordinary[7]['declared_bytes'],16)
            self.assertEqual(ordinary[7]['binding_route'],'classic')
            self.assertEqual(ordinary[8]['phase'],'writable_buffer_binding');self.assertEqual(ordinary[8]['shader'],86)
            self.assertEqual(ordinary[8]['descriptor_slot'],4);self.assertEqual(ordinary[8]['declared_bytes'],17)
            self.assertEqual(ordinary[8]['binding_route'],'paged')
            self.assertEqual(ordinary[9]['phase'],'writable_buffer_binding');self.assertEqual(ordinary[9]['shader'],87)
            self.assertEqual(ordinary[9]['descriptor_slot'],5);self.assertEqual(ordinary[9]['declared_bytes'],18)
            self.assertEqual(ordinary[9]['binding_route'],'storage_image')
            self.assertEqual(ordinary[10]['phase'],'writable_buffer_binding');self.assertEqual(ordinary[10]['shader'],88)
            self.assertEqual(ordinary[10]['descriptor_slot'],6);self.assertEqual(ordinary[10]['declared_bytes'],19)
            self.assertEqual(ordinary[10]['binding_route'],'render_target')
            self.assertEqual(ordinary[11]['phase'],'writable_buffer_binding');self.assertEqual(ordinary[11]['shader'],89)
            self.assertEqual(ordinary[11]['descriptor_slot'],7);self.assertEqual(ordinary[11]['declared_bytes'],20)
            self.assertEqual(ordinary[11]['binding_route'],'storage_image_resolved')
            scans=[event for event in events if event['phase']=='prior_binding_scan']
            priors=[event for event in events if event['phase']=='prior_writable_buffer_binding']
            self.assertEqual(len(scans),3);self.assertEqual(len(priors),3)
            scan=next(event for event in scans if event['capture_id']==42)
            prior=next(event for event in priors if event['capture_id']==42)
            self.assertEqual(scan['history_matches'],1);self.assertEqual(scan['history_stable_records'],1)
            self.assertEqual(prior['shader'],84);self.assertEqual(prior['binding_address']+4,prior['field'])
            self.assertEqual(prior['binding_bytes'],16)
            self.assertEqual(prior['binding_route'],'classic');self.assertEqual(prior['channel'],0)
            wrapped=next(event for event in priors if event['capture_id']==44)
            self.assertEqual(wrapped['shader'],0xabc);self.assertEqual(wrapped['field'],0x200000)
            target=next(event for event in priors if event['capture_id']==45)
            self.assertEqual(target['shader'],0xdef);self.assertEqual(target['field'],0x300000)
            self.assertEqual(target['binding_route'],'render_target')
            self.assertEqual(result['prior_binding_history']['retrospective_matches'],3)
            self.assertEqual(result['prior_binding_history']['replacements'],8)
            self.assertEqual(result['eligible_call_counters']['writable_buffer_bindings_by_route'],{'classic':2,'paged':1,'storage_image':1,'render_target':1,'storage_image_resolved':1})
            self.assertEqual(result['observation_counters']['writable_buffer_bindings_by_route'],{'classic':1,'paged':1,'storage_image':1,'render_target':1,'storage_image_resolved':1})

if __name__=='__main__':unittest.main()
