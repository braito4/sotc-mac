#pragma once
// Original diagnostic metadata. Copies travel with one queued dispatch; never
// infer association from the newest record on another thread or reused address.
#include "gpu_binding_audit.h"
#include "gpu_binding_route_budget.h"
#include <memory>

namespace SotcBindingRoute {
using U64=std::uint64_t;
using Record=SotcBindingAudit::Record;
inline U64 Now(){timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return U64(t.tv_sec)*1000000000+U64(t.tv_nsec);}
inline U64 Thread(){return reinterpret_cast<uintptr_t>(pthread_self());}
inline bool Enabled(){static const bool on=[] {const char* s=std::getenv("SOTC_BINDING_ROUTE");return s&&!std::strcmp(s,"1");}();return on;}
inline bool Selected(U64 shader){
    static const std::vector<U64> hashes=[] {
        std::vector<U64> out;const char* s=std::getenv("SOTC_BINDING_ROUTE_SHADERS");
        if(!s)return out;
        const int saved=errno;
        while(*s&&out.size()<8){char* end=nullptr;errno=0;U64 n=std::strtoull(s,&end,0);
            if(errno||!n||end==s||(*end&&*end!=',')){out.clear();break;}
            out.push_back(n);s=*end?end+1:end;
        }
        if(*s)out.clear();errno=saved;return out;
    }();
    return Enabled()&&std::find(hashes.begin(),hashes.end(),shader)!=hashes.end();
}
struct Binding {
    Record record;
    U64 backend{},offset{},extent{};
    unsigned descriptors{};
    const char* reason="unobserved_path";
};
struct Packet {
    U64 id{},shader{},thread{},prepared_ns{},queued_ns{};
    std::array<Binding,32> bindings{};
    unsigned count{};
    bool truncated{};
};
inline std::atomic<U64> serial{},queued{},omitted{};
inline U64 Trigger(){
    static const U64 value=[] {
        const char* s=std::getenv("SOTC_BINDING_ROUTE_TRIGGER");
        if(!s||!*s)return U64(0);
        const int saved=errno;errno=0;char* end=nullptr;
        U64 n=std::strtoull(s,&end,0);const bool bad=errno||!n||end==s||*end||*s=='-'||!Selected(n);errno=saved;
        if(bad){std::fprintf(stderr,"STOP_BINDING_ROUTE_CONFIG: trigger must be a selected nonzero shader; exit=24\n");std::fflush(stderr);std::_Exit(24);}
        std::fprintf(stderr,"Binding route: focus trigger=%#llx; startup budget=8, triggered budget=120; metadata only\n",(unsigned long long)n);
        return n;
    }();return value;
}
inline SotcBindingRouteBudget::Budget budget;
inline std::atomic<U64> waiting{};
inline thread_local Packet preparing;
inline void Begin(U64 shader){
    if(!Enabled())return;
    preparing={};
    if(Selected(shader)){preparing.id=++serial;preparing.shader=shader;preparing.thread=Thread();preparing.prepared_ns=Now();}
}
inline void Expect(U64 unified,U64 logical){
    if(!preparing.id)return;
    if(preparing.count==preparing.bindings.size()){preparing.truncated=true;return;}
    auto& b=preparing.bindings[preparing.count++];
    b.record.unified=unified;b.record.logical_slot=logical;b.record.shader=preparing.shader;
}
inline Binding* Pending(U64 unified){
    for(unsigned i=0;i<preparing.count;++i)if(preparing.bindings[i].record.unified==unified)return &preparing.bindings[i];
    return nullptr;
}
inline void Bound(U64 unified,U64 guest,U64 requested,U64 bytes,bool written,
                  U64 backend,U64 aligned,U64 extent,U64 adjust,const void* sharp){
    if(!preparing.id)return;
    auto* b=Pending(unified);if(!b)return;
    auto& r=b->record;r.sequence=preparing.id;r.thread=preparing.thread;r.ns=Now();
    r.guest=guest;r.requested=requested;r.bytes=bytes;r.adjust=adjust;r.written=written;
    std::memcpy(r.sharp.data(),sharp,r.sharp.size());
    r.native=SotcBufferHistory::Get().Describe(backend,aligned,extent);
    b->backend=backend;b->offset=aligned;b->extent=extent;b->reason="descriptor_not_recorded";
}
// Called at RecordPushDescriptors on the preparation thread, before scheduling.
inline void Descriptor(U64 unified,U64 backend,U64 offset,U64 extent,unsigned array_element,unsigned count){
    if(!preparing.id)return;
    auto* b=Pending(unified);if(!b)return;
    ++b->descriptors;
    if(!b->record.sequence)return;
    if(b->descriptors!=1)b->reason="duplicate_descriptor";
    else if(array_element||count!=1)b->reason="unsupported_descriptor_array";
    else if(b->backend!=backend||b->offset!=offset||b->extent!=extent)b->reason="queued_descriptor_mismatch";
    else b->reason="matched";
}
inline std::shared_ptr<const Packet> Take(){
    if(!Enabled()||!preparing.id)return {};
    Packet p=preparing;preparing={}; // consumed, cannot leak into another dispatch
    bool large=false;for(unsigned i=0;i<p.count;++i)large|=p.bindings[i].record.bytes>=(1u<<20);
    if(!large)return {};
    // Fixed process-wide budget bounds retained metadata even if scheduling stalls.
    const auto admission=budget.Take(p.shader,Trigger());
    if(admission==SotcBindingRouteBudget::Decision::Waiting){
        if(++waiting==1)std::fprintf(stderr,"Binding route: startup budget saved; waiting for selected producer before recording remaining packets\n");return {};
    }
    if(admission==SotcBindingRouteBudget::Decision::Exhausted){
        if(++omitted==1)std::fprintf(stderr,"Binding route: queued packet budget exhausted; later dispatches unobserved\n");return {};
    }
    ++queued;
    p.queued_ns=Now();return std::make_shared<const Packet>(p);
}
inline thread_local const Packet* current{};
inline thread_local U64 current_id{};
inline Record Find(U64 unified,U64 metal,U64 offset,U64 bytes){
    Record out;out.route_sequence=current_id;out.encoder_thread=Thread();out.route_reason="no_queued_packet";
    if(!current)return out;
    out.route_reason="slot_not_observed";
    for(unsigned i=0;i<current->count;++i){const auto& b=current->bindings[i];if(b.record.unified!=unified)continue;
        out.route_reason=b.reason;
        if(current->truncated){out.route_reason="packet_truncated";return out;}
        if(std::strcmp(b.reason,"matched"))return out;
        const auto span=SotcBufferHistory::Get().NativeSpan(metal,offset,bytes);
        const auto& expected=b.record.native;
        if(!span.epoch||!expected.epoch){out.route_reason="native_identity_unavailable";return out;}
        if(span.metal!=expected.metal){out.route_reason="native_buffer_changed";return out;}
        if(span.epoch!=expected.epoch){out.route_reason="native_epoch_changed";return out;}
        if(span.offset!=expected.offset||span.bytes!=expected.bytes){out.route_reason="native_extent_changed";return out;}
        out=b.record;out.route_sequence=current_id;out.encoder_thread=Thread();out.route_reason="matched";return out;
    }return out;
}
struct Scope {
    std::shared_ptr<const Packet> held;
    const Packet* previous;U64 previous_id;SotcBindingAudit::RoutedFind previous_find;
    explicit Scope(const std::shared_ptr<const Packet>& p):held(p),previous(current),previous_id(current_id),previous_find(SotcBindingAudit::routed_find){
        if(Enabled()){current=p.get();current_id=p?p->id:0;SotcBindingAudit::routed_find=Find;}
    }
    ~Scope(){current=previous;current_id=previous_id;SotcBindingAudit::routed_find=previous_find;}
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
};
inline void Write(U64 id,const char* phase,const std::string& data){
    const int saved=errno;const char* dir=std::getenv("SOTC_SCATTER_GUARD_DIR");
    if(!dir||dir[0]!='/'){errno=saved;return;}
    const std::string path=std::string(dir)+"/route-"+std::to_string(id)+"-"+phase+".json";
    int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
    bool ok=fd>=0;size_t at=0;
    while(ok&&at<data.size()){ssize_t n=write(fd,data.data()+at,data.size()-at);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}at+=size_t(n);}
    if(fd>=0&&close(fd))ok=false;
    if(!ok)std::fprintf(stderr,"Binding route: evidence write failed phase=%s id=%llu\n",phase,(unsigned long long)id);
    errno=saved;
}
} // namespace SotcBindingRoute
