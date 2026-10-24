// Original bounded prefix probes at shader address binding. Diagnostic only.
#pragma once
#include "diagnostic_event_journal.h"
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <pthread.h>
#include <time.h>

extern "C" std::uint64_t runtime_memory_clamp(std::uintptr_t,std::uint64_t);
extern "C" int runtime_memory_is_mapped(std::uintptr_t,std::uint64_t);
extern "C" void runtime_memory_read_backing(std::uintptr_t,void*,std::uint64_t);

namespace SotcShaderBind {
inline bool Enabled() {
    static const bool on=[] {
        const char *p=std::getenv("SOTC_SHADER_BIND_TRACE"),*j=std::getenv("SOTC_DIAGNOSTIC_JOURNAL");
        return p&&!std::strcmp(p,"1")&&j&&!std::strcmp(j,"1");
    }();return on;
}
class Recorder {
    SotcEventJournal journal{};
public:
    Recorder() {
        sotc_event_journal_init(&journal,"shader-bind-events.bin",2,512);
        if(journal.map){journal.map[9]=1;journal.map[15]=8;}
    }
    template<class Reader> void Observe(std::uint64_t program,std::uint64_t address,
            std::uint64_t packet,std::uint64_t raw,unsigned actor,std::uint64_t first,
            std::uint64_t words,std::uint64_t bank,Reader&& read) {
        if(!journal.map)return;
        std::uint64_t head{},thread{};
        const bool readable=address&&address<=UINTPTR_MAX-8&&read(address,&head,8);
        const bool prologue=readable&&std::uint32_t(head)==0xbeeb03ffu;
        const auto self=pthread_self();static_assert(sizeof(self)<=sizeof(thread));
        std::memcpy(&thread,&self,sizeof(self));timespec now{};
        const std::uint64_t ns=clock_gettime(CLOCK_MONOTONIC,&now)?0:
            std::uint64_t(now.tv_sec)*1000000000+now.tv_nsec;
        const std::uint64_t values[SOTC_JOURNAL_DATA_WORDS]={program,address,packet,raw,head,
            std::uint64_t(readable)|(std::uint64_t(prologue)<<1),thread,actor,first,words,bank};
        // Separate histories keep ordinary valid binds from evicting every
        // anomalous/unavailable observation. Neither channel proves validity.
        sotc_event_journal_record(&journal,prologue?0:1,ns,values);
    }
};
inline Recorder& Get(){static Recorder recorder;return recorder;}
inline void Observe(std::uint64_t program,std::uint64_t address,std::uint64_t packet,
        std::uint64_t raw,unsigned actor,std::uint64_t first,std::uint64_t words,std::uint64_t bank) {
    if(!Enabled())return;
    Get().Observe(program,address,packet,raw,actor,first,words,bank,
        [](std::uintptr_t at,void* out,std::size_t bytes){
            if(runtime_memory_clamp(at,bytes)!=bytes||!runtime_memory_is_mapped(at,bytes))return false;
            runtime_memory_read_backing(at,out,bytes);return true;
        });
}
} // namespace SotcShaderBind
