// Original bounded pre-enqueue shader-head observations. Diagnostics only.
// No shader acceptance, relocation, guest ownership or GPU readback is added.
#pragma once
#include "diagnostic_event_journal.h"
#include "gpu_shader_prefix_bridge.h"
#include "gpu_shader_consume_trace.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <span>
#include <pthread.h>
#include <time.h>

extern "C" std::uint64_t runtime_memory_clamp(std::uintptr_t,std::uint64_t);
extern "C" int runtime_memory_is_mapped(std::uintptr_t,std::uint64_t);
extern "C" void runtime_memory_read_backing(std::uintptr_t,void*,std::uint64_t);

namespace SotcShaderSubmit {
inline constexpr std::size_t Slots=16, Samples=512, PacketBudget=4096,
                             WordBudget=262144, DepthBudget=4;
inline constexpr std::uint64_t Summary=1ull<<63;
enum Gap : std::uint64_t {
    PacketLimit=1, WordLimit=2, SampleLimit=4, DepthLimit=8,
    Unavailable=16, IncompletePacket=32, OtherLayout=64, PartialPair=128,
    RepeatedPacket=256
};
enum Counter : unsigned { Attempts,Admitted,PoolRefused,AllocationFailed,
                          ScanFailed,LimitedCaptures,UnreadableHeads,CounterCount };
inline bool Enabled() {
    static const bool on=[] {
        const auto* p=std::getenv("SOTC_SHADER_SUBMIT_TRACE");
        const auto* j=std::getenv("SOTC_DIAGNOSTIC_JOURNAL");
        return p && !std::strcmp(p,"1") && j && !std::strcmp(j,"1");
    }();
    return on;
}
inline std::uint64_t Now() {
    timespec t{};return clock_gettime(CLOCK_MONOTONIC,&t) ? 0 : std::uint64_t(t.tv_sec)*1000000000+t.tv_nsec;
}
inline std::uint64_t Thread() {
    const auto self=pthread_self();std::uint64_t value{};
    static_assert(sizeof(self)<=sizeof(value));std::memcpy(&value,&self,sizeof(self));return value;
}
struct Sample {
    std::uint64_t packet{},raw{},origin{},head{},flags{},ns{},context{},physical{UINT64_MAX};
};
struct Capture {
    std::uint64_t id{},root{},words{},submitter{},started{},gaps{};
    std::size_t count{},packets{},scanned_words{};
    unsigned queue{};
    std::array<Sample,Samples> samples{};
    struct Buffer {std::uintptr_t base;std::size_t words;bool owned;};
    // Reader checks guest mappings; owned=true is only our existing vector copy.
    // Nested IB reads always use the checked backing reader. The observations
    // can race CPU stores/remapping and do not model conditional execution.
    template<class Reader> void Scan(Buffer buffer,unsigned depth,Reader& read) {
        if(depth>DepthBudget) {gaps|=DepthLimit;return;}
        std::size_t at{};
        const auto get=[&](std::size_t offset,void* out,std::size_t n) {
            if(offset>buffer.words || n/4>buffer.words-offset || n%4 ||
               offset>UINTPTR_MAX/4 || buffer.base>UINTPTR_MAX-offset*4 ||
               n>UINTPTR_MAX-(buffer.base+offset*4))return false;
            return read(buffer.base+offset*4,out,n,buffer.owned);
        };
        while(at<buffer.words) {
            if(packets>=PacketBudget) {gaps|=PacketLimit;return;}
            std::uint32_t header{};
            if(!get(at,&header,4)) {gaps|=Unavailable;return;}
            const auto type=header>>30;
            if(type!=2 && type!=3) {gaps|=OtherLayout;return;}
            const std::size_t total=type==2 ? 1 : std::size_t((header>>16)&0x3fff)+2;
            if(total>buffer.words-at) {gaps|=IncompletePacket;return;}
            if(total>WordBudget-scanned_words) {gaps|=WordLimit;return;}
            ++packets;scanned_words+=total;
            const auto opcode=(header>>8)&255;
            if(type==3 && opcode==0x76 && total>=2) {
                std::uint32_t reg{};
                if(!get(at+1,&reg,4)) {gaps|=Unavailable;return;}
                if(reg&0xffff0000u) {gaps|=OtherLayout;at+=total;continue;}
                constexpr std::array<std::uint32_t,7> address_regs={8,0x48,0x88,0xc8,0x108,0x148,0x20c};
                for(auto index:address_regs) {
                    const auto payload=total-2;
                    if(!(std::uint64_t(reg)<std::uint64_t(index)+2 && index<std::uint64_t(reg)+payload))continue;
                    if(reg>index || std::size_t(index-reg)+2>payload) {gaps|=PartialPair;continue;}
                    bool repeated=false;
                    for(std::size_t i=0;i<count;++i) {
                        if(samples[i].packet==buffer.base+at*4 && (samples[i].context>>32)==index) {
                            samples[i].flags|=4;gaps|=RepeatedPacket;repeated=true;break;
                        }
                    }
                    if(repeated)continue;
                    if(count==samples.size()) {gaps|=SampleLimit;return;}
                    std::uint64_t raw{};
                    if(!get(at+2+(index-reg),&raw,8)) {gaps|=Unavailable;continue;}
                    const auto origin=(raw&((1ull<<40)-1))<<8;
                    std::uint64_t head{};
                    const bool available=origin && read(origin,&head,8,false);
                    samples[count++]={buffer.base+at*4,raw,origin,head,
                        std::uint64_t(available)|(std::uint64_t(index==0x20c)<<1)|
                        (std::uint64_t(depth)<<8)|(std::uint64_t(queue)<<16),Now(),
                        std::uint64_t(header)|(std::uint64_t(index)<<32),
                        SotcShaderConsume::Enabled()?SotcShaderPrefixBridge::Physical(origin):UINT64_MAX};
                }
            } else if(type==3 && opcode==0x3f) {
                if(total!=4) {gaps|=OtherLayout;at+=total;continue;}
                std::array<std::uint32_t,3> ib{};
                if(!get(at+1,ib.data(),sizeof(ib))) {gaps|=Unavailable;at+=total;continue;}
                // Match the pinned IB address/size layout, without following
                // an unbounded chain or trusting reserved address bits.
                if((ib[0]&3) || (ib[1]&0xffff0000u)) {gaps|=OtherLayout;at+=total;continue;}
                const auto address=std::uint64_t(ib[0])|(std::uint64_t(ib[1])<<32);
                Scan(Buffer{std::uintptr_t(address),std::size_t(ib[2]&0xfffff),false},depth+1,read);
            }
            at+=total;
        }
    }
};
class Recorder {
    struct Slot {Capture capture;bool used{};};
    std::array<Slot,Slots> slots{};
    std::mutex mutex;
    std::atomic<std::uint64_t> sequence{};
    SotcEventJournal journal{};
public:
    Recorder() {
        sotc_event_journal_init(&journal,"shader-submit-events.bin",2,512);
        if(journal.map) {
            journal.map[9]=1;journal.map[10]=Slots;journal.map[11]=Samples;
            journal.map[12]=PacketBudget;journal.map[13]=DepthBudget;
            journal.map[14]=WordBudget;journal.map[15]=8;
        }
    }
    void Note(Counter which) {
        if(journal.map)__atomic_fetch_add(journal.map+256+which,1,__ATOMIC_RELAXED);
    }
    bool Available() const {return journal.map!=nullptr;}
    std::shared_ptr<Capture> Acquire() noexcept {
        Note(Attempts);std::size_t selected=slots.size();
        {std::lock_guard lock(mutex);
         for(std::size_t i=0;i<slots.size();++i)if(!slots[i].used) {slots[i].used=true;selected=i;break;}}
        if(selected==slots.size()) {Note(PoolRefused);return {};}
        try {
            // The pool bounds live observations; the control block is the only
            // allocation. It never extends the lifetime of a guest allocation.
            auto result=std::shared_ptr<Capture>(&slots[selected].capture,[this,selected](Capture*) {
                std::lock_guard lock(mutex);slots[selected].used=false;
            });
            *result={};result->id=sequence.fetch_add(1,std::memory_order_relaxed)+1;
            Note(Admitted);return result;
        } catch(...) {
            // shared_ptr invokes the deleter when control-block allocation fails.
            Note(AllocationFailed);return {};
        }
    }
    void Publish(const Capture& c,const Sample& s,unsigned phase) {
        const std::uint64_t values[SOTC_JOURNAL_DATA_WORDS]={c.id,s.packet,s.raw,s.origin,s.head,s.flags,
            s.ns,c.submitter,c.root,phase ? Thread() : 0,s.context};
        sotc_event_journal_record(&journal,phase,phase ? Now() : s.ns,values);
    }
    void SummaryOf(const Capture& c) {
        const std::uint64_t values[SOTC_JOURNAL_DATA_WORDS]={c.id,0,c.count,c.packets,c.scanned_words,
            Summary,c.started,c.submitter,c.root,c.gaps,c.words};
        sotc_event_journal_record(&journal,0,Now(),values);
        if(c.gaps)Note(LimitedCaptures);
    }
};
static_assert(sizeof(Recorder)<640*1024,"submission observation pool must stay bounded");
inline Recorder& Get() {static Recorder recorder;return recorder;}
template<class Reader>
std::shared_ptr<Capture> BeginWith(std::span<const std::uint32_t> input,unsigned queue,bool owned,Reader&& reader) noexcept {
    if(!Enabled())return {};
    auto& recorder=Get();if(!recorder.Available())return {};
    auto c=recorder.Acquire();if(!c)return {};
    c->root=reinterpret_cast<std::uintptr_t>(input.data());c->words=input.size();
    c->queue=queue;c->submitter=Thread();c->started=Now();
    try {
        c->Scan({std::uintptr_t(c->root),input.size(),owned},0,reader);
        for(std::size_t i=0;i<c->count;++i) {
            auto& sample=c->samples[i];
            if((sample.flags&5)==1 && std::uint32_t(sample.head)==0xbeeb03ffu) {
                SotcShaderPrefixBridge::Register(sample.origin,sample.packet,c->id);
                if(SotcShaderConsume::Enabled())sample.flags|=1ull<<24;
            }
            if(SotcShaderConsume::Enabled())
                sample.flags|=std::uint64_t(SotcShaderPrefixBridge::Snapshot(sample.origin).state+1)<<25;
            recorder.Publish(*c,c->samples[i],0);
            if(!(c->samples[i].flags&1))recorder.Note(UnreadableHeads);
        }
        // Immutable once submitted. No TLS scope spans a coroutine suspension.
        std::sort(c->samples.begin(),c->samples.begin()+c->count,
                  [](const Sample& a,const Sample& b){return a.packet<b.packet;});
        recorder.SummaryOf(*c);return c;
    } catch(...) {recorder.Note(ScanFailed);return {};}
}
inline std::shared_ptr<Capture> Begin(std::span<const std::uint32_t> input,unsigned queue,bool owned=false) noexcept {
    return BeginWith(input,queue,owned,[](std::uintptr_t address,void* out,std::size_t bytes,bool host_owned) {
        if(address>UINTPTR_MAX-bytes)return false;
        if(host_owned) {std::memcpy(out,reinterpret_cast<const void*>(address),bytes);return true;}
        if(runtime_memory_clamp(address,bytes)!=bytes || !runtime_memory_is_mapped(address,bytes))return false;
        runtime_memory_read_backing(address,out,bytes);return true;
    });
}
inline void Consume(const std::shared_ptr<Capture>& capture,const void* packet) noexcept {
    if(!capture)return;
    const auto address=reinterpret_cast<std::uintptr_t>(packet);
    const auto end=capture->samples.begin()+capture->count;
    auto it=std::lower_bound(capture->samples.begin(),end,address,
                            [](const Sample& s,std::uint64_t p){return s.packet<p;});
    for(;it!=end && it->packet==address;++it) {
        SotcShaderConsume::Observe(capture->id,it->packet,it->origin,it->head,it->flags,it->context,it->physical,it->ns);
        Get().Publish(*capture,*it,1);
    }
}
} // namespace SotcShaderSubmit
