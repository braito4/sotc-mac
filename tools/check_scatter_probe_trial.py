"""Actual scatter diagnostic policy tests; no game data or Metal execution."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
source=r'''
#include "gpu_scatter_probe_policy.h"
#include <cassert>
#include <random>
#include <limits>
int main(){
using namespace SotcScatterProbe;
std::vector<Config> configs;
const std::string layout="fixture_kernel,1,8,80,2,81,10,89,8,13,92,12,64";
assert(Parse(layout,configs)&&configs.size()==1&&configs[0].stride[1]==12);
for(const auto bad:{"","x","x,1", "x,1,8,80,2,81,10,89,0,13,92,12,64", "x,1,8,256,2,81,10,89,8,13,92,12,64", "x,1,8,80,2,81,10,89,8,13,92,12,0", "x,1,8,80,2,81,10,89,8,13,92,12,64,5"})assert(!Parse(bad,configs));
assert(!Parse(layout+";",configs));assert(!Parse(layout+";"+layout,configs));
assert(!Parse("bad-name,1,8,80,2,81,10,89,8,13,92,12,64",configs));
assert(ValidSpan(4096,32,4128)&&!ValidSpan(4096,33,4128));
assert(!ValidSpan(UINT64_MAX,8,UINT64_MAX));
// Last valid element, first invalid element, independent output extents.
std::uint32_t indices[]={0,7,8,UINT32_MAX};
auto r=Inspect(indices,4,2,64,4,{64,96},{8,12});assert(r.checked==2&&!r.invalid&&!r.truncated);
r=Inspect(indices,4,3,64,4,{64,96},{8,12});assert(r.invalid==1&&r.first_thread==2&&r.first_index==8);
r=Inspect(indices,4,4,64,4,{64,96},{8,12});assert(r.invalid==2&&r.maximum==UINT32_MAX);
r=Inspect(indices,4,2,64,4,{64,84},{8,12});assert(r.invalid==1&&r.first_index==7);
r=Inspect(indices,4,99,2,4,{64,96},{8,12});assert(r.active==2&&!r.input_short&&!r.truncated);
r=Inspect(indices,4,5,64,4,{64,96},{8,12});assert(r.input_short&&r.truncated&&r.checked==4);
r=Inspect(indices,1,4,64,4,{64,96},{8,12});assert(!r.input_short&&r.truncated&&r.checked==1);
r=Inspect(nullptr,0,0,64,0,{0,0},{8,12});assert(!r.invalid&&!r.truncated);
// Differential check against byte-end arithmetic using 64-bit products.
std::mt19937 gen(19);
for(unsigned run=0;run<10000;++run){
 std::array<std::uint32_t,64> input;for(auto& i:input)i=gen()%2048;
 if(run%7==0)input[17]=UINT32_MAX;
 U64 size0=gen()%16384,size1=gen()%24576,copied=gen()%65,capacity=gen()%65;
 unsigned count=gen()%100,dispatch=gen()%100;U64 invalid=0,checked=std::min<U64>({count,dispatch,copied,capacity});
 for(U64 i=0;i<checked;++i)invalid+=((U64(input[i])+1)*8>size0||(U64(input[i])+1)*12>size1);
 r=Inspect(input.data(),copied,count,dispatch,capacity,{size0,size1},{8,12});
 assert(r.checked==checked&&r.invalid==invalid);
}
}
'''
with tempfile.TemporaryDirectory(dir=ROOT/'work') as d:
    p=Path(d);(p/'test.cpp').write_text(source)
    subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(ROOT/'runtime/src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: actual parser, full-element bounds, truncation, dispatch limits and 10000 differential cases (ASan/UBSan); GPU snapshots not exercised')
