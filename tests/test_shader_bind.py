"""Original shader bind prefix retention and conservative offline joins."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from analyze_event_journal import analyze
from analyze_shader_bind import review


class ShaderBindTests(unittest.TestCase):
    def test_real_journal_separates_bad_prefix_and_preserves_unavailable(self):
        source=r'''
#include "gpu_shader_bind_trace.h"
#include <cassert>
extern "C" std::uint64_t runtime_memory_clamp(std::uintptr_t,std::uint64_t){return 0;}
extern "C" int runtime_memory_is_mapped(std::uintptr_t,std::uint64_t){return 0;}
extern "C" void runtime_memory_read_backing(std::uintptr_t,void*,std::uint64_t){std::abort();}
int main(int argc,char**){
    if(argc>1){SotcShaderBind::Observe(9,256,8,1,1,8,2,7);return 0;}
    SotcShaderBind::Recorder recorder;
    unsigned reads=0;bool unavailable=false;
    auto read=[&](std::uintptr_t at,void* out,std::size_t bytes){
        ++reads;assert(bytes==8);if(unavailable)return false;
        std::uint64_t head=at==256?0x8877665501020304ull:0x88776655beeb03ffull;
        std::memcpy(out,&head,8);return true;
    };
    recorder.Observe(1,256,3,1,1,8,2,7,read);
    for(unsigned i=0;i<600;++i)recorder.Observe(2,512,4,2,1,8,2,7,read);
    unavailable=true;recorder.Observe(3,768,5,3,6,0x20c,2,7,read);
    assert(reads==602);return 0;
}
'''
        with tempfile.TemporaryDirectory(dir=ROOT/'work') as folder:
            folder=Path(folder);path=folder/'test.cpp';path.write_text(source);binary=folder/'test'
            subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-pthread',
                '-I'+str(ROOT/'runtime/src'),str(path),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],env=dict(os.environ,SOTC_DIAGNOSTIC_JOURNAL='1',BB_GPU_USER_DIR=str(folder)),check=True)
            report=analyze(folder/'shader-bind-events.bin','shader_bind');self.assertEqual(report['state'],'readable')
            bad=[e for e in report['events'] if e['channel']==1]
            self.assertEqual(len(bad),2);self.assertEqual(bad[0]['head'],[0x01020304,0x88776655])
            self.assertFalse(bad[1]['head_available']);self.assertEqual(report['retained'],514)
            (folder/'shader-bind-events.bin').unlink()
            subprocess.run([str(binary),'missing-backend'],env=dict(os.environ,SOTC_SHADER_BIND_TRACE='1',
                SOTC_DIAGNOSTIC_JOURNAL='1',BB_GPU_USER_DIR=str(folder)),check=True)
            missing=analyze(folder/'shader-bind-events.bin','shader_bind')
            self.assertEqual(len(missing['events']),1)
            self.assertFalse(missing['events'][0]['head_available'])

    def test_join_requires_identity_time_thread_and_unique_observation(self):
        write=dict(program=1,address=256,packet=3,hash=1,actor=6,thread=42,ns=100)
        lookup=dict(program=1,address=256,thread=42,head=[3,4],ns=300)
        event=dict(program=1,address=256,packet=3,raw_address_register=1,actor=6,thread=42,
                   head_available=True,head=[3,4],prologue_present=False,ns=200)
        origin=dict(address_write=write,lookup=lookup)
        def result(events):return review(origin,dict(state='readable',events=events))
        self.assertEqual(result([event])['observation'],'same_prefix_without_recognized_prologue')
        self.assertEqual(result([dict(event,head=[1,2])])['observation'],'sampled_prefix_changed')
        self.assertEqual(result([dict(event,head_available=False)])['state'],'bind_prefix_unavailable')
        for key,value in [('thread',99),('packet',9),('ns',400),('actor',1),('raw_address_register',9)]:
            self.assertEqual(result([dict(event,**{key:value})])['state'],'matching_bind_absent')
        self.assertEqual(result([event,event])['state'],'ambiguous_bind')
        self.assertEqual(review(origin,dict(state='absent'))['state'],'absent')


if __name__=='__main__':unittest.main()
