"""Actual policy stress and allocation-only source isolation; no game/GPU execution."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
source=r'''
#include "staging_size_classes.h"
#include <cassert>
#include <thread>
#include <vector>
#include <limits>
int main(){
using namespace SotcStagingClasses;
std::atomic<uint64_t> used{0};
assert(Select(17*MiB,20*MiB,false,used)==20*MiB&&used==0);
assert(Select(16*MiB,16*MiB,true,used)==16*MiB&&used==0);
assert(Select(65*MiB,80*MiB,true,used)==80*MiB&&used==0);
assert(Select(UINT64_MAX,UINT64_MAX,true,used)==UINT64_MAX&&used==0);
assert(Select(17*MiB,16*MiB,true,used)==16*MiB&&used==0);
assert(Select(17*MiB,20*MiB,true,used)==32*MiB&&used==12*MiB);
assert(Select(32*MiB,32*MiB,true,used)==32*MiB&&used==12*MiB);
used=PaddingBudget;assert(Select(17*MiB,20*MiB,true,used)==20*MiB);
used=PaddingBudget+1;assert(Select(17*MiB,20*MiB,true,used)==20*MiB);
used=0;std::atomic<uint64_t> sum{0};std::vector<std::thread> workers;
for(unsigned t=0;t<8;++t)workers.emplace_back([&]{
 for(unsigned i=0;i<10000;++i){
  const auto selected=Select(17*MiB,20*MiB,true,used);
  assert(selected==20*MiB||selected==32*MiB);
  sum.fetch_add(selected-20*MiB);
 }
});
for(auto& t:workers)t.join();assert(sum==used&&used<=PaddingBudget&&used==60*MiB);
// Independent populations model new pools; class capacity always covers the request.
for(uint64_t request=16*MiB+1;request<=64*MiB;request+=7919){
 const auto step=std::max(std::bit_floor(request)/4,uint64_t(65536));
 const auto legacy=(request+step-1)&~(step-1);used=0;
 const auto result=Select(request,legacy,true,used);
 assert(result>=request&&result>=legacy&&result-legacy==used&&used<=PaddingBudget);
 assert(std::has_single_bit(result)||result==legacy);
}
}
'''
with tempfile.TemporaryDirectory(dir=ROOT/'work') as d:
 p=Path(d);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-pthread','-I'+str(ROOT/'runtime/src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
rel='gpu/shadps4/video_core/renderer_vulkan/vk_staging_buffer_pool.cpp'
old=(ROOT/'work/gpu-source-994846bb7bb5'/rel).read_text()
new=(ROOT/'work/strategy-staging-20261105'/rel).read_text()
restored=new.replace('#include "staging_size_classes.h"\n','').replace('        const u64 allocation_size=SotcStagingClasses::AllocationSize(size,RoundAllocationSize(size));\n','').replace('.allocation=allocation_size, .tick=', '.allocation=RoundAllocationSize(size), .tick=').replace('BackendDevice::From(instance), 0, allocation_size,','BackendDevice::From(instance), 0, RoundAllocationSize(size),')
assert restored==old, 'Unexpected lifetime/reuse/control source change'
print('PASS: boundary/overflow/control cases, 80000 concurrent charges, class coverage; lifetime/reuse source unchanged')
