#pragma once
// Read-only fragmented bindings: gather from imported pool buffers in GPU command order.
// Does not repair writable fallbacks, pin guest allocations or copy back to guest memory.
#include "gpu_fragment_provenance.h"
#include <atomic>
#include <optional>
#include <type_traits>

namespace SotcFragmentGather {
using U64=std::uint64_t;
inline bool Enabled(){static const bool on=[] {const char* p=std::getenv("SOTC_FRAGMENT_GATHER");return p&&!std::strcmp(p,"1");}();return on;}
template<class Buffer> struct View {const Buffer* buffer{};U64 offset{},bytes{};};
template<class Buffer> struct Part {const Buffer* buffer{};U64 source{},destination{},bytes{};};
template<class Buffer> struct Plan {std::vector<Part<Buffer>> parts;U64 generation{};bool complete{};const char* reason="disabled_or_written";};
template<class Buffer,class Generation,class Pending,class Mapping,class Window>
Plan<Buffer> Prepare(bool enabled,bool written,U64 address,U64 size,U64 destination,U64 capacity,
                     Generation generation,Pending pending,Mapping mapping,Window window) {
    Plan<Buffer> result;
    if(!enabled||written)return result;
    result.reason="extent";
    if(!size||address>UINT64_MAX-size||size%4||address%4||destination%4||destination>capacity||size>capacity-destination)return result;
    const U64 before=generation();result.generation=before;result.reason="mapping_generation";
    if(before==UINT64_MAX||(before&1)||pending())return result;
    U64 done=0;
    while(done<size){
        result.reason="mapping_gap_or_budget";
        if(result.parts.size()==256)return result;
        U64 physical=0;uintptr_t end=0;const U64 at=address+done;
        if(!mapping(at,&physical,&end)||end<=at)return result;
        // Demand imports need the full currently mapped part, not just its first
        // byte. Fixed-window resolvers retain their original one-argument API.
        const U64 requested=std::min<U64>(size-done,end-at);
        const auto view=[&] {
            if constexpr(std::is_invocable_v<Window,U64,U64>) return window(physical,requested);
            else return window(physical);
        }();result.reason="pool_window";
        if(!view||!view->buffer||view->offset>=view->bytes||physical%4||view->offset%4)return result;
        const U64 n=std::min<U64>({size-done,end-at,view->bytes-view->offset});
        if(!n||n%4||physical>UINT64_MAX-n)return result;
        result.parts.push_back({view->buffer,view->offset,destination+done,n});done+=n;
    }
    result.reason="mapping_changed";
    if(generation()!=before||pending())return result;
    result.complete=true;result.reason="ordered_copy";return result;
}
// Emission is separate: failed or incomplete plans never issue a partial GPU copy.
template<class Buffer,class Emit> bool Submit(const Plan<Buffer>& plan,Emit emit) {
    if(!plan.complete)return false;
    for(const auto& p:plan.parts)emit(p);
    return true;
}
template<class Query>
void Record(U64 guest,U64 bytes,U64 backend,U64 offset,U64 tick,U64 shader,U64 slot,
            U64 before,const U64* generation,Query query) {
    if(!SotcFragment::Enabled())return;
    SotcFragment::Copy c;c.guest=guest;c.bytes=bytes;c.backend=backend;c.offset=offset;
    c.tick=tick;c.shader=shader;c.slot=slot;c.gpu_copy=true;c.generation_before=before;
    c.mapping=SotcFallbackMapping::Capture(guest,bytes,query);c.generation_after=SotcFragment::Generation(generation);
    // No CPU access to pending GPU output, and no stale CPU-copy association for reused staging.
    SotcFragment::Get().Add(c);
}
inline void Note(bool copied,const char* reason,U64 guest,U64 bytes,size_t parts) {
    static std::atomic<U64> copies{},fallbacks{};
    const U64 n=copied?++copies:++fallbacks;
    if(n<=8||n%256==0)std::fprintf(stderr,"Fragment gather: mode=%s count=%llu reason=%s guest=%#llx bytes=%llu parts=%zu; ordered read only, no writeback or allocation lifetime guarantee\n",
        copied?"gpu":"cpu_fallback",(unsigned long long)n,reason,(unsigned long long)guest,(unsigned long long)bytes,parts);
}
} // namespace SotcFragmentGather
