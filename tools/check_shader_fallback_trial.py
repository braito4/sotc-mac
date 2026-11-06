"""Test actual mapping walk without guest memory or GPU access."""
from pathlib import Path
import tempfile,subprocess
ROOT=Path(__file__).resolve().parents[1]
source=r'''
#include "gpu_fallback_mapping.h"
#include <cassert>
int main(){
 using namespace SotcFallbackMapping;
 unsigned calls=0;
 auto contiguous=[&](uintptr_t a,uint64_t* p,uintptr_t* e){++calls;*p=a+8192;*e=(a/4096+1)*4096;return 1;};
 auto s=Capture(4096,8192,contiguous);assert(s.complete&&s.count==2&&s.segments[1].physical==16384);
 s=Capture(4097,1,contiguous);assert(s.complete&&s.count==1);
 calls=0;s=Capture(UINT64_MAX,2,contiguous);assert(!s.complete&&calls==0);
 s=Capture(4096,0,contiguous);assert(!s.complete&&calls==0);
 s=Capture(4096,4096*9,contiguous);assert(!s.complete&&s.count==8&&calls==8);
 auto hole=[](uintptr_t,uint64_t*,uintptr_t*){return 0;};s=Capture(4096,100,hole);assert(!s.complete&&s.count==1&&!s.segments[0].mapped);
 auto broken=[](uintptr_t a,uint64_t* p,uintptr_t* e){*p=0;*e=a;return 1;};s=Capture(4096,100,broken);assert(!s.complete&&s.count==1);
 setenv("SOTC_FALLBACK_MAPPING","1",1);
 for(unsigned i=0;i<300;++i)Record(4096,8192,true,7,99,0,contiguous);
 calls=0;Record(4096,8192,false,7,99,0,contiguous);assert(calls==0);
}
'''
with tempfile.TemporaryDirectory(dir=ROOT/'work') as d:
 p=Path(d);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(ROOT/'runtime/src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 run=subprocess.run([str(p/'test')],capture_output=True,text=True);assert run.returncode==0,run.stderr
 assert run.stderr.count('Writable fallback mapping: record=')==1
 assert run.stderr.count('capture limit reached')==1
print('PASS: contiguous/hole/invalid/overflow/truncation mapping walks, read-only exclusion, deduplication and query cap')
