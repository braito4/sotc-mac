"""Differential checks of the actual PR11 interval lambdas; no GPU or game."""
import subprocess
import tempfile
from pathlib import Path
from build_hoist_interval_trial import build
from check_buffer_copy_chunks import function

def main():
    rel='gpu/shadps4/video_core/renderer_vulkan/vk_runtime.cpp'
    before=(build.BASE/rel).read_text();after=(build.OUT/rel).read_text()
    assert function(before,'void Runtime::AccessBuffer(')==function(after,'void Runtime::AccessBuffer(')
    def lam(name):
        start=after.index('    const auto '+name+' = ')
        return after[start:after.index('\n    };',start)+7]
    source=r'''
#include <map>
#include <vector>
#include <algorithm>
#include <random>
#include <cassert>
#include <cstdint>
#include <cstdio>
using u64=uint64_t;
int main(){
'''+lam('overlaps')+lam('overlaps_set')+lam('insert_set')+r'''
std::mt19937_64 rng(562711);unsigned checks=0;
for(unsigned batch=0;batch<100;++batch){
 std::map<u64,u64> tree;std::vector<std::pair<u64,u64>> linear;
 for(unsigned n=0;n<100;++n){
  u64 a=(rng()%4<<40)+(rng()%65536),b=a+1+rng()%2048;
  insert_set(tree,a,b);linear.emplace_back(a,b);
  for(unsigned q=0;q<60;++q){
   u64 s=(rng()%4<<40)+(rng()%65536),e=s+1+rng()%2048;
   assert(overlaps(linear,s,e)==overlaps_set(tree,s,e));++checks;
  }
  u64 end=0;bool first=true;for(auto [s,e]:tree){assert(s<e);assert(first||end<s);end=e;first=false;}
 }
}
std::map<u64,u64> tree;insert_set(tree,10,20);insert_set(tree,20,30);insert_set(tree,12,14);
assert(tree.size()==1&&tree.begin()->first==10&&tree.begin()->second==30);
assert(!overlaps_set(tree,0,10));assert(!overlaps_set(tree,30,31));
assert(overlaps_set(tree,29,31));
insert_set(tree,UINT64_MAX-10,UINT64_MAX);
assert(overlaps_set(tree,UINT64_MAX-2,UINT64_MAX));
assert(!overlaps_set(tree,UINT64_MAX-11,UINT64_MAX-10));
// Differential three-hazard decisions (read-after-write, write-after-write, write-after-read).
std::map<u64,u64> reads,writes;std::vector<std::pair<u64,u64>> r,w;
for(unsigned n=0;n<10000;++n){
 u64 src=rng()%65536,dst=rng()%65536,size=1+rng()%128;
 bool old=!overlaps(w,src,src+size)&&!overlaps(w,dst,dst+size)&&!overlaps(r,dst,dst+size);
 bool next=!overlaps_set(writes,src,src+size)&&!overlaps_set(writes,dst,dst+size)&&!overlaps_set(reads,dst,dst+size);
 assert(old==next);
 if(!old){r.clear();w.clear();reads.clear();writes.clear();}
 r.emplace_back(src,src+size);w.emplace_back(dst,dst+size);
 insert_set(reads,src,src+size);insert_set(writes,dst,dst+size);
}
std::printf("PASS: %u overlap comparisons, 10000 batch decisions, adjacent/nested/boundary ranges\n",checks);
}
'''
    with tempfile.TemporaryDirectory() as tmp:
        p=Path(tmp);(p/'fixture.cpp').write_text(source)
        subprocess.run(['clang++','-std=c++23','-O1','-Wall','-Wextra','-Werror',
            '-fsanitize=address,undefined',str(p/'fixture.cpp'),'-o',str(p/'fixture')],check=True)
        subprocess.run([str(p/'fixture')],check=True)
    print('PASS: AccessBuffer/read tracking unchanged; tests cover valid nonempty, nonwrapping ranges')
if __name__=='__main__':main()
