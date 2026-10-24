// Original bounded fresh prefix observations at packet consumption. CPU only.
#pragma once
#include "diagnostic_event_journal.h"
#include "gpu_shader_prefix_bridge.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <time.h>

extern "C" std::uint64_t runtime_memory_clamp(std::uintptr_t,std::uint64_t);
extern "C" int runtime_memory_is_mapped(std::uintptr_t,std::uint64_t);
extern "C" void runtime_memory_read_backing(std::uintptr_t,void*,std::uint64_t);

namespace SotcShaderConsume {
inline bool Enabled() {
    static const bool on=[] {
        const char *p=std::getenv("SOTC_SHADER_COPY_TRACE"),*j=std::getenv("SOTC_DIAGNOSTIC_JOURNAL");
        return p&&!std::strcmp(p,"1")&&j&&!std::strcmp(j,"1");
    }();return on;
}
class Recorder {
    SotcEventJournal journal{};
    SotcEventJournal mappings{};
public:
    Recorder() {
        sotc_event_journal_init(&journal,"shader-consume-events.bin",2,512);
        if(journal.map){journal.map[9]=1;journal.map[15]=8;}
        sotc_event_journal_init(&mappings,"shader-mapping-events.bin",2,512);
        if(mappings.map){mappings.map[9]=1;mappings.map[15]=8;}
    }
    template<class Reader> void Observe(std::uint64_t capture,std::uint64_t packet,
            std::uint64_t address,std::uint64_t previous,std::uint64_t flags,
            std::uint64_t context,Reader&& read,std::uint64_t physical=UINT64_MAX,std::uint64_t sampled_ns=0) {
        if(!journal.map)return;
        std::uint64_t head{},thread{};
        const bool available=address&&address<=UINTPTR_MAX-8&&read(address,&head,8);
        const bool changed=(flags&1)&&available&&head!=previous;
        const bool prologue=available&&std::uint32_t(head)==0xbeeb03ffu;
        const auto snapshot=SotcShaderPrefixBridge::Snapshot(address);
        const auto current_physical=changed?SotcShaderPrefixBridge::Physical(address):UINT64_MAX;
        const auto self=pthread_self();static_assert(sizeof(self)<=sizeof(thread));
        std::memcpy(&thread,&self,sizeof(self));timespec now{};
        const std::uint64_t ns=clock_gettime(CLOCK_MONOTONIC,&now)?0:
            std::uint64_t(now.tv_sec)*1000000000+now.tv_nsec;
        flags|=std::uint64_t(available)<<28|std::uint64_t(prologue)<<29|
            std::uint64_t(changed)<<30|std::uint64_t(snapshot.state+1)<<32;
        const std::uint64_t values[SOTC_JOURNAL_DATA_WORDS]={capture,packet,address,previous,
            head,flags,thread,context,snapshot.values[1],snapshot.values[2],snapshot.values[7]};
        // Keep a separate tail for changed, unrecognized and unavailable heads.
        sotc_event_journal_record(&journal,changed||!prologue?1:0,ns,values);
        __atomic_fetch_add(journal.map+256,1,__ATOMIC_RELAXED);
        if(changed)__atomic_fetch_add(journal.map+257,1,__ATOMIC_RELAXED);
        if(!available)__atomic_fetch_add(journal.map+258,1,__ATOMIC_RELAXED);
        if(changed&&mappings.map) {
            const std::uint64_t status=std::uint64_t(physical!=UINT64_MAX)|
                (std::uint64_t(current_physical!=UINT64_MAX)<<1);
            const std::uint64_t mapping[SOTC_JOURNAL_DATA_WORDS]={capture,packet,address,physical,
                current_physical,sampled_ns,ns,thread,previous,head,status};
            sotc_event_journal_record(&mappings,status==3&&physical==current_physical?0:1,ns,mapping);
        }
    }
};
inline Recorder& Get(){static Recorder recorder;return recorder;}
inline void Observe(std::uint64_t capture,std::uint64_t packet,std::uint64_t address,
        std::uint64_t previous,std::uint64_t flags,std::uint64_t context,
        std::uint64_t physical=UINT64_MAX,std::uint64_t sampled_ns=0) {
    if(!Enabled())return;
    const int saved=errno;
    Get().Observe(capture,packet,address,previous,flags,context,
        [](std::uintptr_t at,void* out,std::size_t bytes) {
            if(runtime_memory_clamp(at,bytes)!=bytes||!runtime_memory_is_mapped(at,bytes))return false;
            runtime_memory_read_backing(at,out,bytes);return true;
        },physical,sampled_ns);
    errno=saved;
}
} // namespace SotcShaderConsume
