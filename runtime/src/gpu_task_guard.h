/* Original bounded CPU coroutine-frame quarantine, inspired by SotC-PC's
 * 0127e36 diagnostic. No guest allocator/page changes or GPU completion claim.
 * GPL-2.0-or-later, alongside the bloodborne-native/bbport/shadPS4 adapter. */
#ifndef SOTC_GPU_TASK_GUARD_H
#define SOTC_GPU_TASK_GUARD_H
#include <array>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <mutex>
#include <new>
#include <pthread.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "diagnostic_event_journal.h"

namespace SotcTaskGuard {
inline constexpr size_t Budget=8*1024*1024, Slots=512, Quarantine=64;
struct Entry {void* address=nullptr;size_t bytes=0,requested=0;uint64_t generation=0;bool retired=false;};
struct State {
    std::mutex mutex;
    std::array<Entry,Slots> entries{};
    std::array<size_t,Quarantine> queue{};
    size_t head=0,count=0,mapped=0,live=0,quarantined=0,page=0;
    uint64_t next=0,fallbacks=0;
    bool enabled=false;
    SotcEventJournal journal{};
    State() {
        const char* value=std::getenv("SOTC_TASK_GUARD");enabled=value && !std::strcmp(value,"1");
        if(!enabled)return;
        const long host_page=sysconf(_SC_PAGESIZE);
        if(host_page<=0 || static_cast<size_t>(host_page)>Budget || Budget%static_cast<size_t>(host_page)) {enabled=false;return;}
        page=static_cast<size_t>(host_page);
        sotc_event_journal_init(&journal,"task-events.bin",2,256);
        if(journal.map) {journal.map[9]=Budget;journal.map[10]=page;journal.map[11]=Slots;journal.map[12]=Quarantine;}
        std::fprintf(stderr,"CPU task guard: enabled; mapped budget=%zu quarantine=%zu frames; not guest allocation ownership\n",Budget,Quarantine);
    }
};
inline State& Get() {
    // Process-lifetime bounded metadata: late worker teardown must not lock a
    // destroyed mutex. The budget includes all live/quarantined frame mappings.
    static State* state=new State;return *state;
}
inline void Record(State& s,const Entry& e,unsigned event) {
    timespec now{};uint64_t ns=0;
    if(!clock_gettime(CLOCK_MONOTONIC,&now))ns=static_cast<uint64_t>(now.tv_sec)*1000000000+now.tv_nsec;
    uint64_t data[SOTC_JOURNAL_DATA_WORDS]={reinterpret_cast<uintptr_t>(e.address),e.bytes,e.requested,e.generation,event,
        s.live,s.quarantined,s.mapped,s.fallbacks,0,reinterpret_cast<uintptr_t>(pthread_self())};
    sotc_event_journal_record(&s.journal,(event==1 || event==5)?0:1,ns,data);
}
inline void Evict(State& s) {
    const size_t index=s.queue[s.head];s.head=(s.head+1)%Quarantine;--s.count;
    Entry& e=s.entries[index];
    if(munmap(e.address,e.bytes)) {
        // A failed unmap must not lose its ownership record or reduce budget.
        s.head=(s.head+Quarantine-1)%Quarantine;s.queue[s.head]=index;++s.count;
        Record(s,e,6);return;
    }
    s.mapped-=e.bytes;s.quarantined-=e.bytes;Record(s,e,3);e={};
}
inline void* Allocate(size_t size) {
    State& s=Get();if(!s.enabled)return ::operator new(size);
    std::lock_guard lock{s.mutex};
    if(size && size<=Budget && size<=std::numeric_limits<size_t>::max()-(s.page-1)) {
        const size_t bytes=((size+s.page-1)/s.page)*s.page;
        while(s.count && s.mapped>Budget-bytes) {const size_t before=s.count;Evict(s);if(s.count==before)break;}
        size_t slot=Slots;
        for(size_t i=0;i<Slots;++i)if(!s.entries[i].address){slot=i;break;}
        if(slot==Slots && s.count) {
            const size_t index=s.queue[s.head];Evict(s);if(!s.entries[index].address)slot=index;
        }
        if(slot<Slots && s.mapped<=Budget-bytes) {
            void* address=mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
            if(address!=MAP_FAILED) {
                Entry& e=s.entries[slot];e={address,bytes,size,++s.next,false};
                s.mapped+=bytes;s.live+=bytes;Record(s,e,1);return address;
            }
        }
    }
    // Keep real allocation behavior when the diagnostic budget is exhausted.
    // These frames are unguarded, explicitly counted and never mistaken for mmap.
    ++s.fallbacks;Entry e{};e.requested=size;Record(s,e,5);return ::operator new(size);
}
inline void Release(void* address) noexcept {
    if(!address)return;
    State& s=Get();if(!s.enabled){::operator delete(address);return;}
    std::lock_guard lock{s.mutex};
    size_t index=Slots;
    for(size_t i=0;i<Slots;++i)if(s.entries[i].address==address){index=i;break;}
    if(index==Slots){::operator delete(address);return;}
    Entry& e=s.entries[index];
    if(e.retired){std::fputs("STOP: CPU task guard duplicate frame deletion\n",stderr);std::_Exit(23);}
    if(mprotect(address,e.bytes,PROT_NONE)) {
        Record(s,e,4);
        if(!munmap(address,e.bytes)){s.mapped-=e.bytes;s.live-=e.bytes;Record(s,e,3);e={};}
        else Record(s,e,6);
        // If both protection and unmap fail, preserve the owned mapping/budget.
        return;
    }
    e.retired=true;s.live-=e.bytes;s.quarantined+=e.bytes;
    if(s.count==Quarantine) {
        Evict(s);
        if(s.count==Quarantine) {
            // Preserve the current owned protected mapping when the queue
            // cannot advance. It still occupies a slot and the fixed budget.
            Record(s,e,7);return;
        }
    }
    s.queue[(s.head+s.count)%Quarantine]=index;++s.count;Record(s,e,2);
}
} // namespace SotcTaskGuard
#endif
