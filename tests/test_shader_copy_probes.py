"""Original prefix watch/copy fixtures and conservative association controls."""
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
from analyze_shader_copy import review,activity
from analyze_shader_watch import review as watch_review, transition_review


class ShaderCopyReviewTests(unittest.TestCase):
    def test_watch_snapshot_matches_and_unavailable_states(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
            path=Path(folder)/'watch.txt'
            rows=dict(schema=1,address=256,state=5,watched_address=256,packet=8,capture_id=42,
                registered_ns=100,last_readable_available=1,last_readable_value=(4<<32)|3,last_readable_ns=200,stamp=1)
            path.write_text(''.join(f'{k}={v}\n' for k,v in rows.items()))
            origin=dict(lookup=dict(address=256,ns=300),address_write=dict(packet=8))
            result=watch_review(path,origin,dict(sample=dict(capture_id=42)))
            self.assertEqual(result['state'],'matched');self.assertTrue(result['packet_matches_lookup_write'])
            self.assertEqual(result['latest_head'],[3,4]);self.assertTrue(result['capture_matches_pre_enqueue'])
            without_time=watch_review(path,dict(lookup=dict(address=256),address_write=dict(packet=8)),{})
            self.assertEqual(without_time['state'],'matched')
            self.assertIsNone(without_time['registered_before_lookup'])
            self.assertIsNone(without_time['latest_observation_before_lookup'])
            rows['state']=2;path.write_text(''.join(f'{k}={v}\n' for k,v in rows.items()))
            self.assertEqual(watch_review(path,origin,{})['state'],'address_not_watched')
            rows['address']=512;path.write_text(''.join(f'{k}={v}\n' for k,v in rows.items()))
            self.assertEqual(watch_review(path,origin,{})['state'],'invalid')
    def test_only_exact_registration_and_time_matches_are_candidates(self):
        origin=dict(lookup=dict(address=256,head=[3,4],ns=500))
        submit=dict(state='observed',sample=dict(packet=8,capture_id=42,submit_ns=100,head=[1,2]))
        event=dict(actor=7,field=256,registration_context=dict(packet=8,capture_id=42),ns=300,status=8,
                   value=(4<<32)|3,thread=77)
        counters=dict(registrations=1)
        def run(e):return review(origin,submit,dict(state='readable',events=[e],shader_prefix_watches=counters))
        self.assertTrue(run(event)['events'][0]['matches_lookup'])
        self.assertFalse(run(event)['events'][0]['matches_pre_enqueue'])
        for key,value in [('actor',3),('field',512),('ns',600),('registration_context',dict(packet=8,capture_id=43))]:
            self.assertEqual(run(dict(event,**{key:value}))['state'],'matching_writes_absent')
        self.assertEqual(review(origin,{},dict(state='readable',shader_prefix_watches=counters))['state'],'pre_enqueue_capture_unavailable')
        self.assertEqual(review(origin,submit,dict(state='readable'))['state'],'watch_unavailable')
        self.assertEqual(activity(dict(state='absent'))['state'],'absent')
        self.assertEqual(activity(dict(state='readable',shader_prefix_watches=counters,events=[event]))['retained_candidates'],1)


@unittest.skipUnless(platform.system()=='Darwin' and platform.machine()=='arm64','native ARM64 required')
class ShaderCopyNativeTests(unittest.TestCase):
    def test_real_protected_prefix_observer_and_admission_limits(self):
        source=r'''
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include "cpu_fault_diagnostics.h"
#include "cpu_field_trace.h"
#include "cpu_pointer_trace.h"
unsigned runtime_memory_trap_reasons(uintptr_t p){(void)p;return 0;}
int bootstrap_cpu_failure_state(SotcGuestFailureState* s){memset(s,0,sizeof(*s));s->available=1;s->rip=0x1234;return 1;}
int main(void){
    assert(!sotc_pointer_init("0:0:0:0:2:2"));
    sotc_event_journal_init(&sotc_pointer_journal,"pointer-events.bin",3,128);
    assert(sotc_pointer_journal.map);sotc_shader_prefix_init();assert(sotc_shader_prefix_enabled);
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    void* memory=bb_low_map(page,PROT_READ|PROT_WRITE);assert(memory);
    uint64_t* dst=(uint64_t*)((char*)memory+256);*dst=UINT64_C(0x0000002cbeeb03ff);
    uint64_t input=UINT64_C(0x1122334455667788);uint64_t address=(uintptr_t)dst;
    sotc_shader_prefix_register(address,8,42);
    uint64_t snapshot[8]={0};assert(sotc_shader_prefix_snapshot(address,snapshot)==5);
    assert(snapshot[0]==address&&snapshot[1]==8&&snapshot[2]==42&&snapshot[3]&&snapshot[4]==0);
    sotc_pointer_copy(address,8,2,(uintptr_t)&input);memcpy(dst,&input,8);
    sotc_pointer_copy(address,8,3,(uintptr_t)&input);
    assert(sotc_shader_prefix_snapshot(address,snapshot)==5&&snapshot[4]==1&&snapshot[5]==input);
    sotc_pointer_copy(address+3,2,4,0xaa);memset((char*)dst+3,0xaa,2);
    sotc_pointer_copy(address+3,2,5,0xaa);
    sotc_pointer_copy(address,0,2,0);sotc_pointer_copy(address+256,8,2,0);
    void* guard=bb_low_map(page,PROT_NONE);assert(guard);
    sotc_shader_prefix_register((uintptr_t)guard,16,43);sotc_pointer_copy((uintptr_t)guard,8,2,0);
    unsigned hash=sotc_shader_prefix_bucket(address);uint64_t collision=address+256;
    for(unsigned n=0;n<4;++n){
        while(sotc_shader_prefix_bucket(collision)!=hash)collision+=256;
        sotc_shader_prefix_register(collision,24,44+n);collision+=256;
    }
    assert(sotc_shader_prefix_snapshot(address,snapshot)==2);
    uint64_t total=sotc_pointer_journal.map[5];sotc_pointer_copy(address,8,2,0);
    assert(sotc_pointer_journal.map[5]==total);
    atomic_store(&sotc_shader_prefix_bucket_busy[hash],1);
    sotc_shader_prefix_register(address,32,45);
    assert(sotc_pointer_journal.map[191]==1&&sotc_pointer_journal.map[190]>=1);
    assert(sotc_pointer_journal.map[240]==2&&sotc_pointer_journal.map[241]==1&&
           sotc_pointer_journal.map[242]==1&&sotc_pointer_journal.map[243]==1);
    assert(sotc_pointer_journal.map[160]==0); /* Code prefixes are not pointer-link anomalies. */
    munmap(memory,page);munmap(guard,page);return 0;
}
'''
        with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
            folder=Path(folder);c=folder/'fixture.c';c.write_text(source);binary=folder/'fixture'
            subprocess.run(['clang','-arch','arm64','-O1','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function','-pthread',
                '-I'+str(ROOT/'runtime/src'),'-I'+str(ROOT/'runtime/upstream/src'),
                '-I'+str(ROOT/'runtime/upstream/recomp/src/cpu'),'-I'+str(ROOT/'work/deps-arm64/include'),
                str(c),str(ROOT/'runtime/src/platform_arm64.c'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],env=dict(os.environ,BB_GPU_USER_DIR=str(folder),
                SOTC_DIAGNOSTIC_JOURNAL='1',SOTC_SHADER_COPY_TRACE='1'),check=True)
            journal=analyze(folder/'pointer-events.bin','pointer');self.assertEqual(journal['state'],'readable')
            lifecycle=analyze(folder/'shader-watch-events.bin','shader_watch')
            self.assertEqual(lifecycle['state'],'readable')
            arms=[e for e in lifecycle['events'] if e['action']=='arm']
            target=arms[0]['address']
            history=transition_review(lifecycle,target)
            self.assertEqual([e['action'] for e in history['events']],['arm','evict','registration_dropped'])
            self.assertEqual(history['continuity'],'known_gap_or_registration_failure')
            self.assertEqual(history['events'][0]['capture_id'],42)
            self.assertEqual(history['events'][1]['watch_generation'],history['events'][0]['watch_generation'])
            events=journal['events'];self.assertEqual([e['actor'] for e in events],[6,7,8,9,6])
            self.assertEqual(events[0]['value'],0x0000002cbeeb03ff)
            self.assertEqual(events[1]['value'],0x1122334455667788)
            self.assertFalse(events[-1]['status']&8)
            self.assertEqual(events[1]['registration_context'],dict(packet=8,capture_id=42))

    def test_dynamic_renderer_bridge_reaches_exported_host_callback(self):
        source=r'''
#include "gpu_shader_submit.h"
#include <cassert>
static unsigned calls;
extern "C" void sotc_shader_prefix_watch(std::uint64_t a,std::uint64_t p,std::uint64_t c){assert(a==0x10000000&&p&&c);++calls;}
extern "C" int sotc_shader_prefix_watch_snapshot(std::uint64_t a,std::uint64_t* out){out[0]=a;errno=ENOENT;return 5;}
int main(){
    errno=E2BIG;auto snapshot=SotcShaderPrefixBridge::Snapshot(0x10000000);
    assert(snapshot.state==5&&snapshot.values[0]==0x10000000&&errno==E2BIG);
    std::array<std::uint32_t,4> packet={0xc0027600,8,0x100000,0};
    std::uint64_t head=0x2cbeeb03ffull;bool readable=true;
    auto read=[&](std::uintptr_t at,void* out,std::size_t n,bool owned){
        if(owned){std::memcpy(out,(void*)at,n);return true;}
        assert(at==0x10000000&&n==8);if(!readable)return false;std::memcpy(out,&head,8);return true;
    };
    auto c=SotcShaderSubmit::BeginWith(packet,0,true,read);assert(c&&c->count==1);
    unsigned expected=std::strcmp(std::getenv("SOTC_SHADER_COPY_TRACE"),"1")?0:1;assert(calls==expected);
    head=0x1122334455667788ull;c=SotcShaderSubmit::BeginWith(packet,0,true,read);assert(c&&calls==expected);
    readable=false;c=SotcShaderSubmit::BeginWith(packet,0,true,read);assert(c&&calls==expected);
}
'''
        with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
            folder=Path(folder);cpp=folder/'test.cpp';cpp.write_text(source);binary=folder/'test'
            subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-Wl,-export_dynamic',
                '-I'+str(ROOT/'runtime/src'),str(cpp),'-o',str(binary)],check=True)
            for setting in ('0','1'):
                state=folder/setting;state.mkdir()
                subprocess.run([str(binary)],env=dict(os.environ,SOTC_SHADER_COPY_TRACE=setting,
                    SOTC_SHADER_SUBMIT_TRACE='1',SOTC_DIAGNOSTIC_JOURNAL='1',BB_GPU_USER_DIR=str(state)),check=True)


if __name__=='__main__':unittest.main()
