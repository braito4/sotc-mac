#pragma once
#include "gpu_buffer_history.h"
#include <pthread.h>
namespace SotcBindingAudit {
using U64=std::uint64_t;
inline U64 Selected(){static const U64 value=[] {
    const char *on=std::getenv("SOTC_BINDING_AUDIT"),*s=std::getenv("SOTC_BINDING_AUDIT_SHADER");
    if(!on||std::strcmp(on,"1")||!s||!*s)return U64(0);
    const int saved=errno;char *end=nullptr;errno=0;auto n=std::strtoull(s,&end,0);const bool ok=!errno&&end&&!*end;errno=saved;return ok?U64(n):U64(0);
}();return value;}
struct Record {
    U64 sequence{},ns{},thread{},shader{},unified{},logical_slot{},guest{},requested{},bytes{},adjust{};
    bool written{};std::array<unsigned char,16> sharp{};SotcBufferHistory::Span native;
    const char* route_reason{};
    U64 route_sequence{},encoder_thread{};
};
using RoutedFind=Record(*)(U64,U64,U64,U64);
inline thread_local RoutedFind routed_find{};
class Registry {
    std::mutex gate;std::array<Record,64> records{};U64 serial{};
public:
    void Add(Record r){std::lock_guard lock(gate);r.sequence=++serial;records[(r.sequence-1)%records.size()]=r;}
    Record Find(U64 thread,U64 unified,const SotcBufferHistory::Span& span){
        std::lock_guard lock(gate);
        for(U64 i=0;i<std::min<U64>(serial,records.size());++i){const auto& r=records[(serial-i-1)%records.size()];
            if(r.thread!=thread||r.unified!=unified)continue;
            // Only the newest selected binding on this thread is eligible. No stale
            // fallback through an intervening null/changed native range or epoch.
            if(!span.epoch||r.native.epoch!=span.epoch||r.native.metal!=span.metal||r.native.offset!=span.offset||r.native.bytes!=span.bytes)return {};
            return r;
        }return {};
    }
};
inline Registry& Get(){static Registry value;return value;}
inline void Bound(U64 shader,U64 unified,U64 logical_slot,U64 guest,U64 requested,U64 bytes,bool written,
                  U64 backend,U64 aligned,U64 bound_bytes,U64 adjust,const void *sharp){
    if(!Selected()||shader!=Selected())return;
    Record r;r.shader=shader;r.unified=unified;r.logical_slot=logical_slot;r.guest=guest;r.requested=requested;
    r.bytes=bytes;r.adjust=adjust;r.written=written;r.thread=reinterpret_cast<uintptr_t>(pthread_self());
    timespec now;clock_gettime(CLOCK_MONOTONIC,&now);r.ns=U64(now.tv_sec)*1000000000+U64(now.tv_nsec);
    std::memcpy(r.sharp.data(),sharp,r.sharp.size());r.native=SotcBufferHistory::Get().Describe(backend,aligned,bound_bytes);Get().Add(r);
}
inline Record Find(U64 unified,U64 metal,U64 offset,U64 bytes){
    if(routed_find)return routed_find(unified,metal,offset,bytes);
    if(!Selected())return {};
    // Resolve the current native epoch through the existing history registry.
    const auto span=SotcBufferHistory::Get().NativeSpan(metal,offset,bytes);
    return Get().Find(reinterpret_cast<uintptr_t>(pthread_self()),unified,span);
}
inline void Json(std::ostream& out,const Record& r){
    out<<"{\"matched\":"<<(r.sequence?"true":"false");
    if(r.route_reason)out<<",\"reason\":\""<<r.route_reason<<"\",\"route_sequence\":"<<r.route_sequence<<",\"encoder_thread\":"<<r.encoder_thread;
    if(r.sequence){out<<",\"sequence\":"<<r.sequence<<",\"ns\":"<<r.ns<<",\"shader\":"<<r.shader<<",\"unified\":"<<r.unified<<",\"logical_slot\":"<<r.logical_slot<<",\"guest\":"<<r.guest<<",\"requested_bytes\":"<<r.requested<<",\"bytes\":"<<r.bytes<<",\"adjust\":"<<r.adjust<<",\"written\":"<<(r.written?"true":"false")<<",\"sharp_hex\":\"";
        const char* hex="0123456789abcdef";for(auto b:r.sharp)out<<hex[b>>4]<<hex[b&15];out<<'"';}
    out<<'}';
}
inline void Sample(unsigned sequence,unsigned slot,unsigned phase,const Record& r){
    if(!Selected()||!r.sequence||!r.guest||!r.bytes)return;
    using Fn=void(*)(unsigned,unsigned,unsigned,U64,U64);
    static Fn fn=[] {auto p=reinterpret_cast<Fn>(dlsym(RTLD_DEFAULT,"sotc_buffer_binding_audit_v1"));if(!p)std::fprintf(stderr,"Binding audit: host export unavailable\n");return p;}();
    if(fn)fn(sequence,slot,phase,r.guest,std::min<U64>(r.bytes,8ull<<20));
}
}
