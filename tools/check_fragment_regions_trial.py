"""Actual region gather resolver and triggered route controls, without native GPU."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FOCUS = r'''
#include "gpu_binding_route.h"
#include <cassert>
#include <thread>
using namespace SotcBindingRoute;
auto packet(U64 shader){
 Begin(shader);Expect(1,1);unsigned char sharp[16]={};
 Bound(1,4096,1<<20,1<<20,true,10,0,1<<20,0,sharp);
 Descriptor(1,10,0,1<<20,0,1);return Take();
}
int main(){
 SotcBufferHistory::Register(10,100,0,1<<20);
 for(unsigned i=0;i<10000;++i){auto p=packet(7);assert(bool(p)==(i<8));}
 assert(queued==8&&waiting==9992&&omitted==0);
 auto producer=packet(42);assert(producer&&producer->shader==42);
 auto consumer=packet(43);assert(consumer&&consumer->shader==43);
 std::thread encode([&]{Scope scope(producer);auto r=Find(1,100,0,1<<20);
   assert(r.guest==4096&&r.route_sequence==producer->id&&r.encoder_thread!=r.thread);
 });encode.join();
 for(unsigned i=2;i<120;++i)assert(packet(i%2?42:43));
 assert(!packet(42)&&!packet(43)&&queued==128&&omitted==2);
 // Concurrent budgets cannot exceed the fixed global cap.
 SotcBindingRouteBudget::Budget other;std::atomic<unsigned> admitted{};
 std::vector<std::thread> threads;
 for(unsigned i=0;i<8;++i)threads.emplace_back([&]{for(unsigned n=0;n<1000;++n)
   admitted+=other.Take(42,42)==SotcBindingRouteBudget::Decision::Admit;});
 for(auto& t:threads)t.join();assert(admitted==120);
 SotcBindingRouteBudget::Budget legacy;unsigned count=0;
 for(unsigned n=0;n<1000;++n)count+=legacy.Take(7,0)==SotcBindingRouteBudget::Decision::Admit;
 assert(count==128);
}
'''
PREFIX = r'''
#include "gpu_fragment_gather.h"
#include "gpu_pool_regions.h"
#include <cassert>
#include <functional>
#include <unordered_map>
using U64=uint64_t;
struct Buffer {U64 physical,bytes;U64 SizeBytes()const{return bytes;}};
struct Cache {
 std::unique_ptr<SotcPoolRegions::State<Buffer>> pool_regions;
 std::vector<std::unique_ptr<Buffer>> pool_windows,retired;
 std::vector<U64> pool_window_starts;
 U64 requests=0,requested_bytes=0;bool mutate=false;U64 generation=2;
 Cache(){pool_regions=std::make_unique<SotcPoolRegions::State<Buffer>>();pool_regions->pool_size=4*SotcPoolRegions::Piece;}
 auto PoolRegion(U64 physical,U64 bytes){
   ++requests;requested_bytes+=bytes;
   if(mutate)generation+=2;
   return pool_regions->Acquire(physical,bytes,[](U64 start,U64 n){return std::make_unique<Buffer>(Buffer{start,n});},
     [&](std::unique_ptr<Buffer> b){retired.push_back(std::move(b));},[](U64,U64,const Buffer&){});
 }
 size_t PoolWindow(U64 physical){auto it=std::upper_bound(pool_window_starts.begin(),pool_window_starts.end(),physical);
   return it==pool_window_starts.begin()?0:size_t(it-pool_window_starts.begin()-1);}
 U64 PoolWindowStart(size_t i){return pool_window_starts[i];}
 auto Resolver(){return RESOLVER;}
};
int main(){
 using namespace SotcFragmentGather;
 Cache cache;const U64 second=2*SotcPoolRegions::Piece+8192;
 auto mapping=[&](uintptr_t at,U64* phys,uintptr_t* end){
   if(at<4096||at>=4160)return false;
   if(at<4128){*phys=4096+at-4096;*end=4128;}else{*phys=second+at-4128;*end=4160;}return true;
 };
 auto gen=[&]{return cache.generation;};auto pending=[] {return false;};
 // Old window-only resolution rejects this valid range because regions have no
 // fixed pool_windows. The exact production resolver below must accept it.
 auto old=[](U64)->std::optional<View<Buffer>>{return std::nullopt;};
 auto failed=Prepare<Buffer>(true,false,4096,64,16,128,gen,pending,mapping,old);
 assert(!failed.complete&&!std::strcmp(failed.reason,"pool_window"));
 auto plan=Prepare<Buffer>(true,false,4096,64,16,128,gen,pending,mapping,cache.Resolver());
 assert(plan.complete&&plan.parts.size()==2&&cache.requests==2&&cache.requested_bytes==64);
 assert(cache.pool_windows.empty()&&cache.pool_regions->regions.size()==2);
 std::unordered_map<U64,unsigned char> physical;
 for(U64 i=0;i<32;++i){physical[4096+i]=0xaa;physical[second+i]=0xbb;}
 std::array<unsigned char,128> destination;destination.fill(0xcc);
 std::array<unsigned char,64> old_snapshot;
 for(U64 i=0;i<32;++i){old_snapshot[i]=physical[4096+i];old_snapshot[32+i]=physical[second+i];}
 std::vector<std::function<void()>> queue;
 queue.push_back([&]{for(U64 i=0;i<32;++i){physical[4096+i]=0x11;physical[second+i]=0x22;}});
 assert(Submit(plan,[&](const auto& p){queue.push_back([&,p]{for(U64 i=0;i<p.bytes;++i)
   destination[p.destination+i]=physical[p.buffer->physical+p.source+i];});}));
 for(auto& op:queue)op();
 for(unsigned i=0;i<32;++i){assert(destination[16+i]==0x11&&destination[48+i]==0x22);assert(old_snapshot[i]==0xaa&&old_snapshot[32+i]==0xbb);}
 assert(destination[15]==0xcc&&destination[80]==0xcc);
 // Full mapped extent is supplied, so a 20MiB mapping imports once, not once per piece.
 auto wide=[](uintptr_t at,U64* phys,uintptr_t* end){*phys=at;*end=24ull<<20;return true;};
 auto big=Prepare<Buffer>(true,false,4096,20ull<<20,0,20ull<<20,gen,pending,wide,cache.Resolver());
 assert(big.complete&&big.parts.size()==1&&cache.requests==3&&cache.requested_bytes==64+(20ull<<20));
 // An old plan's pointer remains alive when a later import merges its region.
 assert(plan.parts[0].buffer->physical==0&&cache.retired.size()==1);
 cache.mutate=true;
 auto changed=Prepare<Buffer>(true,false,4096,64,0,64,gen,pending,mapping,cache.Resolver());
 assert(!changed.complete&&!std::strcmp(changed.reason,"mapping_changed"));
 unsigned emitted=0;assert(!Submit(changed,[&](const auto&){++emitted;}));assert(!emitted);
 cache.mutate=false;
 auto written=Prepare<Buffer>(true,true,4096,64,0,64,gen,pending,mapping,cache.Resolver());
 assert(!written.complete);assert(!Submit(written,[&](const auto&){++emitted;}));assert(!emitted);
 cache.pool_regions.reset();cache.pool_windows.push_back(std::make_unique<Buffer>(Buffer{0,4*SotcPoolRegions::Piece}));cache.pool_window_starts.push_back(0);
 auto fixed=Prepare<Buffer>(true,false,4096,64,0,64,gen,pending,mapping,cache.Resolver());
 assert(fixed.complete&&fixed.parts.size()==2); // fixed-window control still works
}
'''


def resolver():
    path = ROOT / 'work/fragment-regions-trial-20261112/gpu/shadps4/video_core/buffer_cache/buffer_cache.cpp'
    text = path.read_text()
    start = text.index('[&](uint64_t physical, uint64_t bytes) -> std::optional<SotcFragmentGather::View<Buffer>>')
    first = text.index('{', start); depth = 0
    for i in range(first, len(text)):
        if text[i] == '{': depth += 1
        elif text[i] == '}':
            depth -= 1
            if not depth: return text[start:i + 1]
    raise AssertionError('Missing resolver body')


def main():
    with tempfile.TemporaryDirectory(prefix='fragment-regions-', dir=ROOT / 'work') as tmp:
        for name, code in [('gather', PREFIX.replace('RESOLVER', resolver())), ('focus', FOCUS)]:
            source = Path(tmp) / (name + '.cpp'); source.write_text(code); binary = Path(tmp) / name
            subprocess.run(['clang++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-pthread', '-I' + str(ROOT / 'runtime/src'),
                            str(source), '-o', str(binary)], check=True)
            env = dict(os.environ, SOTC_BUFFER_HISTORY='1', SOTC_BINDING_ROUTE='1',
                       SOTC_BINDING_ROUTE_SHADERS='7,42,43', SOTC_BINDING_ROUTE_TRIGGER='42')
            subprocess.run([str(binary)], env=env, check=True)
            if name == 'focus':
                result = subprocess.run([str(binary)], env=dict(env, SOTC_BINDING_ROUTE_TRIGGER='99'))
                assert result.returncode == 24
    print('PASS: actual region resolver fixes empty-window refusal; ordered reads see queued writes, full-span imports, retained plan objects, fixed-window control and changed-map/writable refusal. Trigger keeps late producer/consumer metadata under 128 packets; CPU ASan/UBSan only.')


if __name__ == '__main__': main()
