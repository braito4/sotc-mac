#pragma once
// Bounded observations of fallback copies; never repairs memory or claims GPU execution.
#include "gpu_fallback_mapping.h"
#include "gpu_buffer_history.h"
#include <algorithm>
#include <deque>
#include <unordered_map>
#include <vector>

namespace SotcFragment {
using U64=std::uint64_t;
inline bool Enabled() {
    static const bool on=[] {const char* p=std::getenv("SOTC_FRAGMENT_PROVENANCE");return p && !std::strcmp(p,"1");}();
    return on;
}
inline bool Overlap(U64 a,U64 n,U64 b,U64 m) {
    return n && m && (a>=b ? a-b<m : b-a<n);
}
inline U64 Generation(const U64* pointer) {
    return pointer ? __atomic_load_n(pointer,__ATOMIC_ACQUIRE) : UINT64_MAX;
}
struct Copy {
    U64 sequence{},guest{},bytes{},backend{},offset{},tick{},shader{},slot{};
    U64 metal{},metal_base{},registration{},generation_before{},generation_after{};
    bool written{},gpu_copy{};
    unsigned head_bytes{};
    std::array<unsigned char,64> head{};
    SotcFallbackMapping::Snapshot mapping;
};
struct Match {
    bool found{},complete{};
    Copy copy;
    std::vector<Copy> prior_writes;
    U64 total{},evicted{},alias_omitted{},writers_omitted{};
};
class Registry {
    struct Alias {U64 metal,base,registration;};
    std::mutex mutex;
    std::unordered_map<U64,Alias> aliases;
    std::deque<Copy> copies;
    U64 serial{},registration{},evicted{},alias_omitted{};
    size_t copy_limit,alias_limit;
public:
    explicit Registry(size_t history=4096,size_t live=8192):copy_limit(history),alias_limit(live) {}
    void Register(U64 backend,U64 metal,U64 base) {
        std::lock_guard lock(mutex);
        if (!aliases.contains(backend) && aliases.size()>=alias_limit) {++alias_omitted;return;}
        aliases[backend]={metal,base,++registration};
    }
    void Remove(U64 backend) {std::lock_guard lock(mutex);aliases.erase(backend);}
    void Add(Copy c) {
        std::lock_guard lock(mutex);c.sequence=++serial;
        if (auto it=aliases.find(c.backend);it!=aliases.end()) {
            c.metal=it->second.metal;c.metal_base=it->second.base;c.registration=it->second.registration;
        }
        if (!copy_limit) {++evicted;return;}
        if (copies.size()==copy_limit) {copies.pop_front();++evicted;}
        copies.push_back(c);
    }
    Match Find(U64 metal,U64 offset,U64 bytes) {
        std::lock_guard lock(mutex);
        Match result;result.total=serial;result.evicted=evicted;result.alias_omitted=alias_omitted;
        // A newer partial overwrite invalidates an older complete association.
        for (auto it=copies.rbegin();it!=copies.rend();++it) {
            const auto live=aliases.find(it->backend);
            if (it->metal!=metal || !it->registration || live==aliases.end() ||
                live->second.registration!=it->registration || it->offset>UINT64_MAX-it->metal_base) continue;
            const U64 start=it->metal_base+it->offset;
            if (!Overlap(start,it->bytes,offset,bytes)) continue;
            result.found=true;result.copy=*it;
            result.complete=offset>=start && offset-start<=it->bytes && bytes<=it->bytes-(offset-start);
            break;
        }
        if (!result.found) return result;
        for (auto it=copies.rbegin();it!=copies.rend();++it) {
            if (it->sequence>=result.copy.sequence || !it->written ||
                !Overlap(it->guest,it->bytes,result.copy.guest,result.copy.bytes)) continue;
            if (result.prior_writes.size()<8) result.prior_writes.push_back(*it);
            else ++result.writers_omitted;
        }
        return result;
    }
};
inline Registry& Get() {static auto* registry=new Registry;return *registry;}
inline void Register(U64 backend,U64 metal,U64 base) {if(Enabled())Get().Register(backend,metal,base);}
inline void Remove(U64 backend) {if(Enabled())Get().Remove(backend);}

template<class Query>
inline void Copied(U64 guest,U64 size,U64 backend,U64 offset,U64 tick,bool written,
                   U64 shader,U64 slot,const void* data,U64 before,const U64* generation,Query query) {
    SotcBufferHistory::CpuCopy(backend,offset,size,guest,shader,slot,tick,query);
    if (!Enabled()) return;
    Copy c;c.guest=guest;c.bytes=size;c.backend=backend;c.offset=offset;c.tick=tick;
    c.written=written;c.shader=shader;c.slot=slot;c.generation_before=before;
    c.head_bytes=static_cast<unsigned>(std::min<U64>(size,c.head.size()));
    if (c.head_bytes) std::memcpy(c.head.data(),data,c.head_bytes);
    c.mapping=SotcFallbackMapping::Capture(guest,size,query);
    c.generation_after=Generation(generation);
    Get().Add(c);
}
inline void Print(const char* role,const Copy& c) {
    const bool stable=c.generation_before!=UINT64_MAX && !(c.generation_before&1) &&
                      c.generation_before==c.generation_after;
    std::fprintf(stderr,"Fragment copy: role=%s sequence=%llu guest=%#llx bytes=%llu backend=%#llx offset=%llu metal=%#llx metal_base=%llu registration=%llu tick=%llu shader=%#llx slot=%llu written=%u generation_before=%llu generation_after=%llu mapping_stable_during_observation=%u mapping_complete=%u\n",
        role,(unsigned long long)c.sequence,(unsigned long long)c.guest,(unsigned long long)c.bytes,
        (unsigned long long)c.backend,(unsigned long long)c.offset,(unsigned long long)c.metal,
        (unsigned long long)c.metal_base,(unsigned long long)c.registration,(unsigned long long)c.tick,
        (unsigned long long)c.shader,(unsigned long long)c.slot,unsigned(c.written),
        (unsigned long long)c.generation_before,(unsigned long long)c.generation_after,unsigned(stable),unsigned(c.mapping.complete));
    std::fprintf(stderr,"  fragment copy mode: %s; head_available_bytes=%u\n",c.gpu_copy?"gpu_ordered":"cpu",c.head_bytes);
    std::fprintf(stderr,"  fragment CPU copy head:");
    for (unsigned i=0;i+4<=c.head_bytes;i+=4) {std::uint32_t v;std::memcpy(&v,c.head.data()+i,4);std::fprintf(stderr," %08x",v);}
    std::fprintf(stderr,"\n");
    for (unsigned i=0;i<c.mapping.count;++i) {
        const auto& m=c.mapping.segments[i];
        std::fprintf(stderr,"  fragment mapping: start=%#llx physical=%#llx end=%#llx mapped=%u\n",
            (unsigned long long)m.start,(unsigned long long)m.physical,(unsigned long long)m.end,unsigned(m.mapped));
    }
}
inline void Report(U64 slot,U64 metal,U64 offset,U64 bytes,const void* gpu_head,U64 gpu_bytes) {
    if (!Enabled()) return;
    const auto r=Get().Find(metal,offset,bytes);
    std::fprintf(stderr,"Fragment provenance: slot=%llu metal=%#llx offset=%llu bytes=%llu lookup=%s full_span=%u observed_copies=%llu evicted=%llu alias_omitted=%llu prior_writes=%zu prior_writes_omitted=%llu; copy/binding observations, not completed producer writes or allocation lifetimes\n",
        (unsigned long long)slot,(unsigned long long)metal,(unsigned long long)offset,(unsigned long long)bytes,
        r.found?"matched":"unavailable",unsigned(r.complete),(unsigned long long)r.total,
        (unsigned long long)r.evicted,(unsigned long long)r.alias_omitted,r.prior_writes.size(),(unsigned long long)r.writers_omitted);
    if (!r.found) return;
    Print("consumer_source",r.copy);
    const U64 start=r.copy.offset+r.copy.metal_base;
    U64 compared=0;bool equal=false;
    if (r.complete && gpu_head && offset>=start && offset-start<r.copy.head_bytes) {
        const U64 skip=offset-start;
        compared=std::min<U64>({gpu_bytes,bytes,r.copy.head_bytes-skip});
        equal=compared && std::memcmp(gpu_head,r.copy.head.data()+skip,compared)==0;
    }
    std::fprintf(stderr,"Fragment CPU/GPU head comparison: slot=%llu compared_bytes=%llu equal=%u; bounded prefix only\n",
        (unsigned long long)slot,(unsigned long long)compared,unsigned(equal));
    for (const auto& c:r.prior_writes) Print("prior_writable_fallback",c);
    std::fflush(stderr);
}
} // namespace SotcFragment

namespace SotcFragment {
inline void DumpHistory(unsigned slot,U64 metal,U64 offset,U64 bytes) {
    if(!SotcBufferHistory::Enabled())return;
    const auto match=Get().Find(metal,offset,bytes);
    U64 guest=0,guest_bytes=0;
    if(match.found&&match.complete){
        const U64 delta=offset-(match.copy.metal_base+match.copy.offset);
        if(delta<=UINT64_MAX-match.copy.guest&&SotcBufferHistory::SpanValid(match.copy.guest+delta,bytes)){
            guest=match.copy.guest+delta;guest_bytes=bytes;
        }
    }
    SotcBufferHistory::Dump(slot,metal,offset,bytes,guest,guest_bytes);
}
}
