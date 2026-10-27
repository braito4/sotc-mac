// Original bounded submission observations; no game, shader execution or GPU.
#include "gpu_shader_submit.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include <sys/mman.h>
using namespace SotcShaderSubmit;
using Words=std::vector<std::uint32_t>;
static std::map<std::uintptr_t,Words> memory;
static std::atomic<unsigned> backing_reads{};
static std::uint64_t physical_offset=4096;
extern "C" int runtime_memory_pool_phys(std::uintptr_t at,std::uint64_t* physical,std::uintptr_t* end) {
    const auto i=memory.find(at);if(i==memory.end())return 0;
    *physical=physical_offset;*end=at+i->second.size()*4;return 1;
}
extern "C" std::uint64_t runtime_memory_clamp(std::uintptr_t at,std::uint64_t n) {
    for(const auto& [base,data]:memory)if(at>=base && at-base<data.size()*4)
        return std::min<std::uint64_t>(n,data.size()*4-(at-base));
    return 0;
}
extern "C" int runtime_memory_is_mapped(std::uintptr_t at,std::uint64_t n) {
    return runtime_memory_clamp(at,n)==n;
}
extern "C" void runtime_memory_read_backing(std::uintptr_t at,void* out,std::uint64_t n) {
    ++backing_reads;assert(runtime_memory_is_mapped(at,n));
    for(const auto& [base,data]:memory)if(at>=base && at-base<data.size()*4) {
        std::memcpy(out,reinterpret_cast<const char*>(data.data())+at-base,n);return;
    }
    assert(false);
}
static Words Packet(std::uint32_t index=8,std::uint64_t address=0x10000000) {
    const auto raw=address>>8;
    return {0xc0027600,index,std::uint32_t(raw),std::uint32_t(raw>>32)};
}
int main(int argc,char** argv) {
    assert(argc==2);const std::string mode=argv[1];
    memory[0x10000000]={0xbeeb03ff,4};auto packet=Packet();
    if(mode=="disabled" || mode=="journal-disabled" || mode=="existing") {
        assert(!Begin(packet,0,true) && !backing_reads);return 0;
    }
    if(mode=="guarded") {
        const auto page=std::size_t(sysconf(_SC_PAGESIZE));
        auto* area=static_cast<char*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0));
        assert(area!=MAP_FAILED && !mprotect(area+page,page,PROT_NONE));
        auto* last=reinterpret_cast<std::uint32_t*>(area+page-4);*last=0xc0027600;
        auto c=Begin(std::span<const std::uint32_t>(last,1),0,true);
        assert(c && !c->count && c->gaps&IncompletePacket && !backing_reads);
        packet=Packet(8,reinterpret_cast<std::uintptr_t>(area+page));
        c=Begin(packet,0,true);assert(c && c->count==1 && !(c->samples[0].flags&1));
        assert(!backing_reads && !munmap(area,page*2));
    } else if(mode=="partial") {
        packet={0xc0017600,8,0x100000};auto c=Begin(packet,0,true);
        assert(c && !c->count && c->gaps&PartialPair && !backing_reads);
    } else if(mode=="nested" || mode=="repeated") {
        memory[0x20000000]=packet;packet={0xc0023f00,0x20000000,0,4};
        if(mode=="repeated") {const auto again=packet;packet.insert(packet.end(),again.begin(),again.end());}
        auto c=Begin(packet,0,true);assert(c && c->count==1 && ((c->samples[0].flags>>8)&255)==1);
        assert(bool(c->gaps&RepeatedPacket)==(mode=="repeated"));
        Consume(c,reinterpret_cast<const void*>(std::uintptr_t(0x20000000)));
    } else if(mode=="depth") {
        memory[0x20000000]={0xc0023f00,0x20000000,0,4};
        packet=memory[0x20000000];auto c=Begin(packet,0,true);
        assert(c && !c->count && c->gaps&DepthLimit);
    } else if(mode=="unavailable-ib") {
        packet={0xc0023f00,0x20000000,0,4};auto c=Begin(packet,0,true);
        assert(c && !c->count && c->gaps&Unavailable && !backing_reads);
    } else if(mode=="budget") {
        Words late(5000,0x80000000);
        late.insert(late.end(),packet.begin(),packet.end());
        auto c=Begin(late,0,true);assert(c && c->count==1 && !(c->gaps&PacketLimit));
        assert(c->packets==5001);
        assert(c->samples[0].packet==reinterpret_cast<std::uintptr_t>(late.data()+5000));
        Words many;
        constexpr unsigned Extra=300;
        for(unsigned i=0;i<Samples+Extra;++i)many.insert(many.end(),packet.begin(),packet.end());
        c=Begin(many,0,true);assert(c && c->count==Samples && c->gaps&SampleLimit);
        assert(c->tail_replacements==Extra);
        const auto base=reinterpret_cast<std::uintptr_t>(many.data());
        assert(c->samples.front().packet==base);
        assert(c->samples[HeadSamples-1].packet==base+(HeadSamples-1)*packet.size()*4);
        assert(c->samples[HeadSamples].packet==base+(Samples+Extra-TailSamples)*packet.size()*4);
        assert(c->samples.back().packet==base+(Samples+Extra-1)*packet.size()*4);
        Words padding(PacketBudget+1,0x80000000);c=Begin(padding,0,true);
        assert(c && c->packets==PacketBudget && c->gaps&PacketLimit);
        Words large(WordBudget+1,0);
        for(unsigned i=0;i<large.size();i+=16384)large[i]=0xfffe1000;
        c=Begin(large,0,true);assert(c && c->gaps&IncompletePacket);
        large.resize(WordBudget+16384);large[WordBudget]=0xfffe1000;
        c=Begin(large,0,true);assert(c && c->gaps&WordLimit);
    } else if(mode=="pool") {
        std::vector<std::shared_ptr<Capture>> held;
        for(unsigned i=0;i<Slots;++i) {held.push_back(Begin(packet,0,true));assert(held.back());}
        const auto before=backing_reads.load();assert(!Begin(packet,0,true) && backing_reads==before+1);
        const auto snapshot=SotcShaderOriginCache::Find(0x10000000);
        assert(snapshot.state==5 && snapshot.count==SotcShaderOriginCache::Ways && snapshot.total==Slots+1);
        assert(snapshot.matches[snapshot.count-1].capture==Slots+1);
        held.clear();assert(Begin(packet,0,true));
    } else if(mode=="concurrent") {
        std::mutex lock;std::set<std::uint64_t> ids;std::vector<std::thread> threads;
        for(unsigned t=0;t<8;++t)threads.emplace_back([&] {
            for(unsigned i=0;i<16;++i) {
                auto c=Begin(packet,0,true);assert(c && c->count==1);Consume(c,packet.data());
                std::lock_guard guard(lock);assert(ids.insert(c->id).second);
            }
        });
        for(auto& thread:threads)thread.join();assert(ids.size()==128);
    } else {
        assert(mode=="changed" || mode=="stable" || mode=="bad-at-submit" || mode=="exit" || mode=="compute" || mode=="payload-changed" || mode=="tail-wrap" || mode=="unavailable-at-consume" || mode=="changed-recognized" || mode=="remapped");
        if(mode=="compute")packet=Packet(0x20c);
        if(mode=="bad-at-submit")memory[0x10000000]={0x1234,0x5678};
        auto c=Begin(packet,mode=="compute" ? 3 : 0,true);assert(c && c->count==1);
        const auto original=c->samples[0].head;
        if(mode=="changed")memory[0x10000000]={0x1234,0x5678};
        if(mode=="changed-recognized")memory[0x10000000]={0xbeeb03ff,9};
        if(mode=="remapped"){physical_offset=8192;memory[0x10000000]={0x1234,0x5678};}
        if(mode=="unavailable-at-consume")memory.clear();
        if(mode=="payload-changed")packet[2]=0x200000;
        if(mode=="tail-wrap")for(unsigned i=0;i<600;++i)assert(Begin(packet,0,true));
        if(mode=="exit")std::_Exit(53);
        assert(c->samples[0].head==original);Consume(c,packet.data());
        if(mode=="stable")assert(backing_reads==(SotcShaderConsume::Enabled()?2u:1u));
        assert(bool(c->samples[0].flags&2)==(mode=="compute"));
        std::printf("capture=%llu head=%llx\n",(unsigned long long)c->id,(unsigned long long)original);
    }
    std::printf("PASS: %s\n",mode.c_str());
}
