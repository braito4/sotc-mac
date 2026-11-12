"""CPU/sanitizer checks for real region manager and actual Metal unregister body.

Synthetic only: no Metal device, game buffers or GPU commands are created.
"""
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = r'''
#include "gpu_pool_regions.h"
#include <cassert>
#include <random>
#include <set>
using namespace SotcPoolRegions;
static unsigned live=0;
struct Buffer {
    Size start, bytes, id;
    Buffer(Size s, Size n, Size i):start(s),bytes(n),id(i){++live;}
    ~Buffer(){--live;}
};
int main() {
    constexpr Size GiB=1ull<<30, MiB=1ull<<20;
    assert(!Align(0,0,8*GiB));assert(!Align(UINT64_MAX,8,8*GiB));
    assert(!Align(1,UINT64_MAX,8*GiB));assert(!Align(8*GiB,1,8*GiB));
    assert(Align(UINT64_MAX-1,1,UINT64_MAX)->end==UINT64_MAX);
    assert(Align(31*MiB,MiB,32*MiB)->end==32*MiB);
    assert(Align(32*MiB,1,32*MiB+1)->end==32*MiB+1);
    State<Buffer> state;state.pool_size=6592*MiB;
    Size serial=0,tick=4,completed=0,rewrites=0;
    struct Retired { Size tick; std::unique_ptr<Buffer> buffer; };
    std::vector<Retired> retired;
    auto factory=[&](Size a,Size n){return std::make_unique<Buffer>(a,n,++serial);};
    auto retire=[&](std::unique_ptr<Buffer> b){retired.push_back({tick+1,std::move(b)});};
    std::map<Size,std::pair<Size,Size>> table;
    auto rewrite=[&](Size start,Size end,const Buffer& b){
        ++rewrites;
        for(auto [block,base]:state.block_base)if(base>=start&&base<end)table[block]={b.id,base-start};
    };
    auto get=[&](Size a,Size n){return state.Acquire(a,n,factory,retire,rewrite);};
    auto first=get(GiB,64*1024);auto id=first.first->id;
    state.block_base[12]=GiB;table[12]={id,0};
    // A synthetic 3 GiB binding crossing a 3.5 GiB fixed window. One complete
    // host view replaces the small region, with no staging copy or truncation.
    auto large=get(GiB,3*GiB);
    assert(large.first->bytes==3*GiB && large.second==GiB);
    assert(state.regions.size()==1 && state.imported==3*GiB && rewrites==1);
    assert(table[12].first==large.first->id && table[12].second==0);
    assert(retired.size()==1 && first.first->id==id && live==2);
    auto collect=[&]{std::erase_if(retired,[&](const auto& r){return r.tick<=completed;});};
    completed=4;collect();assert(retired.size()==1); // pending next submission
    completed=5;collect();assert(retired.empty() && live==1);
    auto contained=get(2*GiB,1);assert(contained.first==large.first && serial==2);
    auto separate=get(5*GiB,MiB);assert(state.regions.size()==2);
    assert(separate.first->start==5*GiB && separate.first->bytes==Piece);
    state.block_base[99]=5*GiB;table[99]={separate.first->id,0};
    state.Forget(99,99);table.erase(99);
    auto merged=get(3*GiB,3*GiB);
    assert(merged.first->start==GiB && merged.first->bytes==5*GiB);
    assert(state.imported==5*GiB && state.regions.size()==1);
    assert(!state.block_base.contains(99) && !table.contains(99));
    assert(table[12].first==merged.first->id); // unmaps never resurrect
    const auto before=state.imported,old_serial=serial,old_rewrites=rewrites;
    auto failure=state.Acquire(0,state.pool_size,
        [](Size,Size){return std::unique_ptr<Buffer>{};},retire,rewrite);
    assert(!failure.first && state.imported==before && serial==old_serial && rewrites==old_rewrites);
    assert(!get(state.pool_size-1,2).first);
    assert(!state.At(0).first && !state.At(6*GiB).first);
    // Independent bitmap oracle: active imports equal the union of rounded
    // requests, even across >4GiB; no overlapping double accounting after merge.
    State<Buffer> random_state;random_state.pool_size=6592*MiB;
    std::set<Size> blocks;std::mt19937 rng(20261112);
    auto discard=[](std::unique_ptr<Buffer>){};
    for(unsigned i=0;i<2000;++i){
        Size a=rng()%random_state.pool_size,n=1+rng()%(random_state.pool_size-a);
        auto result=random_state.Acquire(a,n,factory,discard,[](Size,Size,const Buffer&){});
        assert(result.first && a>=result.second && n<=result.first->bytes-(a-result.second));
        auto span=Align(a,n,random_state.pool_size);
        for(Size b=span->start/Piece;b<(span->end+Piece-1)/Piece;++b)blocks.insert(b);
        Size total=0,previous=0;
        for(const auto& [start,r]:random_state.regions){assert(start>=previous);previous=r.end;total+=r.end-start;}
        assert(total==blocks.size()*Piece && total==random_state.imported);
    }
}
'''


def main():
    source = ROOT / 'work/pool-regions-trial-20261112/gpu/shadps4/video_core/renderer_metal/mtl_frame.mm'
    text = source.read_text()
    body = text[text.index('void BufferGone(VkBuffer buffer) {'):text.index('\nvoid DeviceAddressTaken')]
    # Compile the actual production body using harmless pointer/vector protocol
    # doubles. This checks both the disabled path and native strong-list removal.
    body = body.replace('id<MTLBuffer>', 'FakeBuffer*').replace('nil', 'nullptr')
    body = body.replace('(__bridge void*)', '(void*)').replace('gone.length', 'gone->length')
    body = body.replace('found->second.buffer.length', 'found->second.buffer->length')
    body = body.replace('[residency removeAllocation:gone]', 'residency->removeAllocation(gone)')
    body = body.replace('[residency commit]', 'residency->commit()')
    fixture = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <vector>
using VkBuffer=uintptr_t;
struct FakeBuffer {unsigned length=16;};
struct Ref {FakeBuffer* buffer;};
struct Residency {unsigned removed=0,commits=0; void removeAllocation(FakeBuffer*){++removed;}void commit(){++commits;}};
std::map<VkBuffer,Ref> registered;std::shared_mutex registered_mutex;std::mutex maps_mutex;
std::vector<FakeBuffer*> address_buffers,declared_buffers;
std::atomic<unsigned> registered_generation{},address_generation{};
Residency storage;Residency* residency=&storage;
namespace SotcBufferChain {template<class... T>void Note(T...) {}}
namespace SotcFragment {unsigned removed=0;void Remove(uintptr_t){++removed;}}
namespace SotcBufferHistory {unsigned removed=0;void Remove(uintptr_t){++removed;}}
''' + body + r'''
int main(){
 FakeBuffer a,b;registered[1]={&a};registered[2]={&b};
 address_buffers={&a,&b};declared_buffers={&a,&b};
 BufferGone(1);assert(!registered.contains(1)&&registered.contains(2)&&registered_generation==1);
 assert(SotcFragment::removed==1 && SotcBufferHistory::removed==1);
 bool enabled=std::getenv("SOTC_POOL_REGIONS") && !std::strcmp(std::getenv("SOTC_POOL_REGIONS"),"1");
 assert(address_buffers.size()==(enabled?1:2));assert(declared_buffers.size()==(enabled?1:2));
 assert(storage.removed==(enabled?1:0)&&storage.commits==(enabled?1:0));
 assert(address_generation==(enabled?1:0));BufferGone(1);assert(registered_generation==1);
 residency=nullptr;BufferGone(2);assert(registered.empty());
 if(enabled){assert(address_buffers.empty()&&declared_buffers.empty()&&address_generation==2);}
}
'''
    with tempfile.TemporaryDirectory(prefix='pool-region-check-', dir=ROOT / 'work') as temp:
        for name, content in [('core', CORE), ('unregister', fixture)]:
            src = Path(temp) / (name + '.cpp'); src.write_text(content)
            binary = Path(temp) / name
            subprocess.run(['clang++', '-std=c++20', '-O1', '-g', '-fsanitize=address,undefined',
                            '-fno-omit-frame-pointer', '-I' + str(ROOT / 'runtime/src'), str(src),
                            '-o', str(binary)], check=True)
            for enabled in ('0', '1') if name == 'unregister' else ('1',):
                subprocess.run([str(binary)], env={**os.environ, 'SOTC_POOL_REGIONS': enabled}, check=True)
    print('PASS: region bounds/merges, 3 GiB crossing, >4 GiB ranges, deferred lifetime, alias rebinding, unmap/failure and 2000 union-oracle cases; actual unregister enabled/control; ASan/UBSan. CPU only.')


if __name__ == '__main__':
    main()
