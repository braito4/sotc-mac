// Original bounded CPU register/lookup and supplied packet observations. No GPU work.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <time.h>
#include <pthread.h>
#include "diagnostic_event_journal.h"
#include "gpu_failure_history.h"
#include "gpu_shader_bind_trace.h"

namespace SotcShaderProvenance {
// ComputeProgram contains dispatch state before its two program-address words.
// Keep the offset checked against the pinned layout; never use the object prefix.
template<class Program> constexpr std::size_t AddressOffset() {
    if constexpr(requires(Program p) {p.dispatch_initiator;p.pad1;}) {
        static_assert(offsetof(Program,pad1)==56,"review changed compute address layout");
        return 48;
    } else return 0;
}
inline bool Enabled() {
    static const bool on=[] {const char* p=std::getenv("SOTC_SHADER_PROVENANCE");return p && !std::strcmp(p,"1");}();
    return on;
}
inline bool Range(std::uint64_t capacity,std::uint64_t first,std::uint64_t words) {
    return first<=capacity && words<=capacity-first;
}
inline void CheckShWrite(std::uint64_t offset,std::uint64_t words,
                         std::uint64_t registers,std::uint64_t compute_words) {
    // Preserve the existing inclusive compute dispatch domain, rejecting a
    // nonempty write at its one-past endpoint instead of overwriting its owner.
    const bool compute=offset>=0x200 && offset-0x200<=compute_words;
    const bool bank=registers>=0x2c00 && offset<=registers-0x2c00 &&
                    words<=registers-0x2c00-offset;
    if (!bank ||
        (compute && !Range(compute_words,offset-0x200,words))) {
        std::fprintf(stderr,"STOP_GPU_REGISTER_RANGE: offset=%llu words=%llu registers=%llu compute_words=%llu\n",
            (unsigned long long)offset,(unsigned long long)words,
            (unsigned long long)registers,(unsigned long long)compute_words);
        Sotc::PreserveCpuFailureHistory();std::_Exit(23);
    }
}
struct Event {
    std::uint64_t seq{},program{},address{},bank{},packet{},first{},words{},hash{};
    std::uint64_t ns{},thread{};
    std::uint32_t kind{},actor{},head0{},head1{},found{},length{},required{};
};
class Recorder {
    struct Slot {
        std::uint64_t program{},total{},suppressed{};std::size_t kept{};
        std::array<Event,8> events{};
    };
    std::mutex mutex;
    std::array<Slot,64> slots{};
    std::uint64_t sequence{},evictions{};std::size_t next{};
    SotcEventJournal journal{};
public:
    Recorder() {
        sotc_event_journal_init(&journal,"shader-events.bin",2,512);
        if(journal.map) {journal.map[9]=1;journal.map[15]=8;}
    }
    void LookupCaptureNote(unsigned index) {
        if(journal.map && index>=10 && index<=14)
            __atomic_fetch_add(journal.map+index,1,__ATOMIC_RELAXED);
    }
    void Record(Event event) {
        struct timespec now{};
        const auto self=pthread_self();static_assert(sizeof(self)<=sizeof(event.thread));
        std::memcpy(&event.thread,&self,sizeof(self));
        if(!clock_gettime(CLOCK_MONOTONIC,&now))
            event.ns=std::uint64_t(now.tv_sec)*1000000000+now.tv_nsec;
        std::lock_guard lock(mutex);
        Slot* slot=nullptr;
        for(auto& candidate:slots)if(candidate.kept && candidate.program==event.program) {slot=&candidate;break;}
        if(!slot) {
            slot=&slots[next];next=(next+1)%slots.size();
            evictions+=slot->kept!=0;*slot={};slot->program=event.program;
        }
        if(slot->kept && event.kind==2) {
            const auto& last=slot->events[slot->kept-1];
            if(last.kind==2 && last.actor==event.actor && last.address==event.address && last.found==event.found &&
               last.head0==event.head0 && last.head1==event.head1 && last.length==event.length &&
               last.hash==event.hash && last.required==event.required &&
               last.bank==event.bank && last.packet==event.packet && last.first==event.first && last.words==event.words) {
                ++slot->suppressed;sotc_event_journal_suppressed(&journal);return;
            }
        }
        event.seq=++sequence;
        if(slot->kept==slot->events.size()) {
            for(std::size_t i=1;i<slot->kept;++i)slot->events[i-1]=slot->events[i];
        } else ++slot->kept;
        slot->events[slot->kept-1]=event;++slot->total;
        // Persist a separate bounded tail of writes and lookups before any
        // terminating path. Signals never query the recorder mutex or loader.
        if(journal.map) {
            const std::uint64_t evidence[SOTC_JOURNAL_DATA_WORDS]={event.program,event.address,
                event.bank,event.packet,event.first,event.words,event.hash,
                std::uint64_t(event.kind)|(std::uint64_t(event.actor)<<32),
                std::uint64_t(event.head0)|(std::uint64_t(event.head1)<<32),
                std::uint64_t(event.length)|(std::uint64_t(event.found)<<32)|(std::uint64_t(event.required)<<33),event.thread};
            sotc_event_journal_record(&journal,event.kind==1 ? 0 : 1,event.ns,evidence);
        }
    }
private:
    static void Print(std::FILE* out,std::uint64_t program,const Slot& copy,std::uint64_t lost) {
        std::fprintf(out,"Shader provenance: program=%#llx retained=%zu total=%llu omitted=%llu slot_evictions=%llu suppressed=%llu; numeric CPU observations, not ownership or GPU execution\n",
            (unsigned long long)program,copy.kept,(unsigned long long)copy.total,
            (unsigned long long)(copy.total-copy.kept),(unsigned long long)lost,(unsigned long long)copy.suppressed);
        for(std::size_t i=0;i<copy.kept;++i) {
            const auto& e=copy.events[i];
            std::fprintf(out,"Shader provenance event: seq=%llu program=%#llx address=%#llx kind=%u actor=%u bank=%#llx packet=%#llx first=%llu words=%llu found=%u head=%08x,%08x length=%u hash=%016llx required=%u ns=%llu thread=%#llx\n",
                (unsigned long long)e.seq,(unsigned long long)e.program,(unsigned long long)e.address,
                e.kind,e.actor,(unsigned long long)e.bank,(unsigned long long)e.packet,
                (unsigned long long)e.first,(unsigned long long)e.words,e.found,e.head0,e.head1,e.length,
                (unsigned long long)e.hash,e.required,(unsigned long long)e.ns,(unsigned long long)e.thread);
            if(e.kind==2 && (e.actor==1 || e.actor==2))
                std::fprintf(out,"Shader lookup metadata: seq=%llu metadata_offset=%llu hint=%llu scanned=%llu\n",
                    (unsigned long long)e.seq,(unsigned long long)e.bank,
                    (unsigned long long)e.packet,(unsigned long long)e.first);
        }
    }
public:
    void Report(std::FILE* out,std::uint64_t program) {
        Slot copy{};std::uint64_t lost{};
        {std::lock_guard lock(mutex);lost=evictions;
         for(const auto& slot:slots)if(slot.kept && slot.program==program) {copy=slot;break;}}
        Print(out,program,copy,lost);
    }
    void ReportAddress(std::FILE* out,std::uint64_t address) {
        // Snapshot numeric matches before any host failure-history callback can
        // change state. No guest reads, shader recovery or inferred ownership.
        std::array<Slot,8> copies{};std::size_t count{},omitted{};
        std::uint64_t lost{};
        {std::lock_guard lock(mutex);lost=evictions;
         for(const auto& slot:slots) {
            bool match=false;
            for(std::size_t i=0;i<slot.kept;++i)match|=slot.events[i].address==address;
            if(!match)continue;
            if(count<copies.size())copies[count++]=slot;else ++omitted;
         }}
        std::fprintf(out,"Shader decode provenance: origin=%#llx matched_programs=%zu omitted_programs=%zu; bounded numeric address matches, not ownership or writer proof\n",
            (unsigned long long)address,count,omitted);
        for(std::size_t i=0;i<count;++i)Print(out,copies[i].program,copies[i],lost);
    }
};
inline Recorder& Get() {static Recorder recorder;return recorder;}
template<class Program>
inline void Write(const Program& p,const void* bank,const void* packet,
                  std::uint64_t first,std::uint64_t words,unsigned actor,std::size_t packet_words=0) {
    if(!Enabled() || !words)return;
    constexpr auto address_offset=AddressOffset<Program>();
    static_assert(sizeof(Program)>=address_offset+sizeof(std::uint64_t));
    std::uint64_t raw{};
    std::memcpy(&raw,reinterpret_cast<const std::byte*>(&p)+address_offset,sizeof(raw));
    std::uint32_t header{};std::array<std::uint32_t,2> payload{};
    const bool packet_available=packet && packet_words>=2;
    bool pair_available=false;
    if(packet_available) {
        std::memcpy(&header,packet,sizeof(header));
        const auto object=reinterpret_cast<std::uintptr_t>(&p),base=reinterpret_cast<std::uintptr_t>(bank);
        // Only SET_SH_REG has this payload layout. Other observer packets
        // retain their header and register snapshot without inventing a pair.
        if((header>>30)==3 && ((header>>8)&255)==0x76 && object>=base && (object-base)%4==0) {
            const auto index=(object-base+address_offset)/4+((actor==3 || actor==4) ? 0x200 : 0);
            if(first<=index && index-first<words && words-(index-first)>=2 &&
               index-first<=packet_words-2 && 2<=packet_words-2-(index-first)) {
                std::memcpy(payload.data(),static_cast<const std::uint32_t*>(packet)+2+(index-first),sizeof(payload));
                pair_available=true;
            }
        }
    }
    const auto observed_address=reinterpret_cast<std::uintptr_t>(p.template Address<const std::uint32_t*>());
    Get().Record(Event{.program=reinterpret_cast<std::uintptr_t>(&p),
        .address=observed_address,
        .bank=reinterpret_cast<std::uintptr_t>(bank),.packet=reinterpret_cast<std::uintptr_t>(packet),
        // New actor IDs distinguish valid compute address words from the legacy
        // dispatch-prefix observations (3/4); old journals remain unknown.
        .first=first,.words=words,.hash=raw,.kind=1,
        .actor=address_offset ? (actor==3 ? 6u : actor==4 ? 7u : actor) : actor,
        .head0=payload[0],.head1=payload[1],.found=unsigned(pair_available),.length=header,
        .required=unsigned(packet_available)});
    if(pair_available)SotcShaderBind::Observe(reinterpret_cast<std::uintptr_t>(&p),observed_address,
        reinterpret_cast<std::uintptr_t>(packet),raw,address_offset?(actor==3?6u:actor==4?7u:actor):actor,
        first,words,reinterpret_cast<std::uintptr_t>(bank));
}
template<class Registers>
inline void Graphics(const Registers& regs,std::uint64_t first,std::uint64_t words,
                     const void* packet,unsigned actor,std::size_t packet_words=0) {
    if(!Enabled() || !words)return;
    const auto observe=[&](const auto& p) {
        const auto index=(reinterpret_cast<std::uintptr_t>(&p)-reinterpret_cast<std::uintptr_t>(&regs))/4;
        if(first<index+2 && index<first+words)Write(p,&regs,packet,first,words,actor,packet_words);
    };
    observe(regs.ps_program);observe(regs.vs_program);observe(regs.gs_program);
    observe(regs.es_program);observe(regs.hs_program);observe(regs.ls_program);
}
template<class Program,class Result>
inline void Lookup(const Program& p,const Result& result,bool required,std::uintptr_t address) {
    if(!Enabled())return;
    Get().Record(Event{.program=reinterpret_cast<std::uintptr_t>(&p),
        .address=address,
        // Actor 2 adds explicit association status in the unused words field.
        .bank=result.offset,.packet=result.hint,.first=result.scanned,.words=std::uint64_t(result.association),
        .hash=result.info.shader_hash,.kind=2,.actor=2,.head0=result.head[0],.head1=result.head[1],
        .found=unsigned(result.found),.length=unsigned(result.info.length),.required=unsigned(required)});
}
} // namespace SotcShaderProvenance
