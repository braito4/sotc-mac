"""Ordered read-only gather planner/submit and provenance tests, no native GPU."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
SOURCE=r'''
#include "gpu_fragment_gather.h"
#include <cassert>
#include <functional>
#include <random>
struct Buffer {std::vector<unsigned char> bytes;};
int main() {
 using namespace SotcFragmentGather;
 Buffer pool{std::vector<unsigned char>(4096)},dest{std::vector<unsigned char>(128,0xcc)};
 U64 generation=2;bool pending=false;
 const auto gen=[&]{return generation;};const auto dirty=[&]{return pending;};
 const auto mapping=[](uintptr_t at,U64* physical,uintptr_t* end){
   if(at<4096||at>=4160)return false;
   if(at<4128){*physical=64+at-4096;*end=4128;}else{*physical=2048+at-4128;*end=4160;}return true;
 };
 const auto window=[&](U64 physical)->std::optional<View<Buffer>> {return View<Buffer>{&pool,physical,pool.bytes.size()};};
 auto make=[&](bool on=true,bool written=false,U64 address=4096,U64 size=64,U64 at=16,U64 capacity=128){
   return Prepare<Buffer>(on,written,address,size,at,capacity,gen,dirty,mapping,window);
 };
 auto plan=make();assert(plan.complete&&plan.parts.size()==2);
 assert(plan.parts[0].source==64&&plan.parts[1].source==2048&&plan.parts[1].destination==48);
 // Old CPU snapshot observes stale backing; GPU queue must see the preceding write instead.
 std::array<unsigned char,64> old{};
 std::memcpy(old.data(),pool.bytes.data()+64,32);std::memcpy(old.data()+32,pool.bytes.data()+2048,32);
 std::vector<std::function<void()>> queue;
 queue.push_back([&]{std::memset(pool.bytes.data()+64,0x11,32);std::memset(pool.bytes.data()+2048,0x22,32);});
 assert(Submit(plan,[&](const auto& p){queue.push_back([&,p]{std::memcpy(dest.bytes.data()+p.destination,p.buffer->bytes.data()+p.source,p.bytes);});}));
 assert(dest.bytes[16]==0xcc&&old[0]==0);
 for(auto& f:queue)f();
 assert(dest.bytes[16]==0x11&&dest.bytes[47]==0x11&&dest.bytes[48]==0x22&&dest.bytes[79]==0x22);
 assert(dest.bytes[15]==0xcc&&dest.bytes[80]==0xcc&&old[0]==0); // Extents and old negative control.
 const auto refuse=[&](const auto& p){assert(!p.complete);unsigned emitted=0;assert(!Submit(p,[&](auto){++emitted;}));assert(!emitted);};
 refuse(make(false));refuse(make(true,true));refuse(make(true,false,4096,0));
 refuse(make(true,false,4096,64,100));refuse(make(true,false,4097));
 refuse(make(true,false,4096,65));refuse(make(true,false,UINT64_MAX-3,8));
 refuse(make(true,false,4096,68)); // A valid first mapping must not produce a partial copy.
 generation=3;refuse(make());generation=UINT64_MAX;refuse(make());generation=2;pending=true;refuse(make());pending=false;
 unsigned calls=0;
 auto changed=Prepare<Buffer>(true,false,4096,64,16,128,[&]{return ++calls==1?U64(2):U64(4);},dirty,mapping,window);refuse(changed);
 auto missing=[](U64)->std::optional<View<Buffer>>{return {};};
 refuse(Prepare<Buffer>(true,false,4096,64,16,128,gen,dirty,mapping,missing));
 const auto split_window=[&](U64 physical)->std::optional<View<Buffer>>{
   if(physical<2048)return View<Buffer>{&pool,physical,80};return View<Buffer>{&pool,physical,pool.bytes.size()};
 };
 // Real window splitting: different imported objects cover adjacent physical pieces.
 Buffer second{std::vector<unsigned char>(4096)};
 auto windows=[&](U64 physical)->std::optional<View<Buffer>>{
   if(physical<80)return View<Buffer>{&pool,physical,80};return View<Buffer>{&second,physical-80,second.bytes.size()};
 };
 auto split=Prepare<Buffer>(true,false,4096,64,16,128,gen,dirty,mapping,windows);
 assert(split.complete&&split.parts.size()==3&&split.parts[1].buffer==&second);
 refuse(Prepare<Buffer>(true,false,4096,64,16,128,gen,dirty,mapping,split_window));
 auto tiny=[](uintptr_t at,U64* phys,uintptr_t* end){*phys=at;*end=at+4;return true;};
 auto wide=[&](U64 physical)->std::optional<View<Buffer>>{return View<Buffer>{&pool,physical,UINT64_MAX};};
 refuse(Prepare<Buffer>(true,false,4096,4*257,0,4*257,gen,dirty,tiny,wide));
 // Random complete maps preserve exact byte order and destination coverage, no clamping.
 std::mt19937 random(81);
 for(unsigned test=0;test<2000;++test){
   const U64 n=4*(1+random()%128),cut=4*(1+random()%(n/4));
   auto map=[&](uintptr_t at,U64* phys,uintptr_t* end){const U64 off=at-4096;
     if(off>=n)return false;*phys=off<cut?64+off:2048+off-cut;*end=off<cut?4096+cut:4096+n;return true;};
   auto p=Prepare<Buffer>(true,false,4096,n,8,n+8,gen,dirty,map,window);assert(p.complete);
   U64 done=0;for(const auto& part:p.parts){assert(part.destination==8+done);assert(part.source==(done<cut?64+done:2048+done-cut));done+=part.bytes;}assert(done==n);
 }
 setenv("SOTC_FRAGMENT_PROVENANCE","1",1);SotcFragment::Register(100,200,0);
 const unsigned char cpu[64]={1};SotcFragment::Copied(4096,64,100,16,1,false,42,1,cpu,2,&generation,mapping);
 assert(SotcFragment::Get().Find(200,16,64).copy.head_bytes==64);
 Record(4096,64,100,16,2,42,1,2,&generation,mapping);
 const auto newest=SotcFragment::Get().Find(200,16,64);assert(newest.found&&newest.complete&&newest.copy.gpu_copy&&newest.copy.head_bytes==0&&!newest.copy.written);
 std::puts("PASS: ordered gather sees queued writes while CPU snapshot is stale; exact mapping/window coverage, no partial emission, read-only gate, generations, bounded plans and pending-output provenance");
}
'''
with tempfile.TemporaryDirectory(dir=ROOT/'work') as tmp:
    p=Path(tmp);(p/'test.cpp').write_text(SOURCE)
    subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-I'+str(ROOT/'runtime/src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: gather CPU protocol fixtures under ASan/UBSan; not native Metal/game validation')
