"""Actual observation-window controls: repeated requests cannot exhaust future windows."""
from pathlib import Path
import tempfile,subprocess
ROOT=Path(__file__).resolve().parents[1]
source=r'''
#include "gpu_fallback_mapping_window.h"
#include <cassert>
int main(){
 using namespace SotcFallbackMapping;ProbeWindow b;
 assert(b.Admit(100,1,5));
 for(unsigned i=0;i<100000;++i)assert(!b.Admit(101,1,5));
 assert(b.count==1&&b.duplicates==100000&&b.skipped==0);
 for(uint64_t i=2;i<=32;++i)assert(b.Admit(101,i,5));
 assert(!b.Admit(102,33,5)&&b.skipped==1);
 assert(!b.Admit(10000000099ULL,33,5));
 assert(b.Admit(10000000100ULL,33,5)&&b.count==1&&b.skipped==0);
 assert(b.Admit(10000000101ULL,1,5));
 assert(b.Admit(10000000102ULL,1,6));
 for(uint64_t window=2;window<180;++window){
  assert(b.Admit(window*10000000000ULL+100,1,5));
  assert(!b.Admit(window*10000000000ULL+101,1,5));
 }
 assert(b.Admit(1,1,5)); // reset on clock rollback in the injectable policy
 setenv("SOTC_FALLBACK_MAPPING","1",1);unsigned calls=0;
 auto query=[&](uintptr_t a,uint64_t* p,uintptr_t* e){++calls;*p=a;*e=a+4096;return 1;};
 RecordWindow(4096,32,false,1,0,0,query);assert(calls==0);
 RecordWindow(4096,32,true,1,0,0,query);assert(calls==1);
 RecordWindow(4096,32,true,2,0,0,query);assert(calls==1);
}
'''
with tempfile.TemporaryDirectory(dir=ROOT/'work') as d:
 p=Path(d);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(ROOT/'runtime/src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 run=subprocess.run([str(p/'test')],capture_output=True,text=True);assert run.returncode==0,run.stderr
 assert run.stderr.count('Writable fallback mapping window:')==1
print('PASS: 100000 duplicates, capacity, exact interval boundary, 30-minute continued sampling, read-only exclusion and query deduplication')
