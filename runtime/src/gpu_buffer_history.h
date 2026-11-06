#pragma once
// Bounded metadata history, not a GPU execution oracle or guest-allocation ledger.
#include <algorithm>
#include <array>
#include <atomic>
#include <time.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include "gpu_page_history_bridge.h"
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unistd.h>
#include <vector>

namespace SotcBufferHistory {
using U64=std::uint64_t;
inline bool Enabled(){static const bool on=[] {const char* p=std::getenv("SOTC_BUFFER_HISTORY");return p&&!std::strcmp(p,"1");}();return on;}
inline bool PhysicalEnabled(){static const bool on=[] {const char* p=std::getenv("SOTC_GPU_PAGE_HISTORY");return p&&!std::strcmp(p,"1");}();return on;}
inline bool SpanValid(U64 at,U64 n){return n&&n<=UINT64_MAX-at;}
inline bool Overlap(U64 a,U64 n,U64 b,U64 m){return SpanValid(a,n)&&SpanValid(b,m)&&(a>=b?a-b<m:b-a<n);}
struct Span {U64 metal{},epoch{},offset{},bytes{},physical{},pool_epoch{};bool pool{};};
enum class Kind {Register,Remove,Pool,Copy,Fill,Update,ImageDownload,ShaderWriteIntent,CpuSnapshot,Map,Unmap};
inline const char* Name(Kind k){switch(k){
    case Kind::Register:return "native_register";case Kind::Remove:return "native_remove";
    case Kind::Pool:return "pool_import";case Kind::Copy:return "copy_encoded";case Kind::Fill:return "fill_encoded";
    case Kind::Update:return "update_encoded";case Kind::ImageDownload:return "image_download_encoded_bounding_span";
    case Kind::ShaderWriteIntent:return "shader_write_binding_intent";case Kind::CpuSnapshot:return "cpu_snapshot";
    case Kind::Map:return "guest_map_notice";case Kind::Unmap:return "guest_unmap_notice";
}return "unknown";}
inline bool Writer(Kind k){return k==Kind::Copy||k==Kind::Fill||k==Kind::Update||k==Kind::ImageDownload||k==Kind::ShaderWriteIntent||k==Kind::CpuSnapshot;}
struct Event {U64 sequence{},ns{},command{},shader{},slot{},tick{},guest{},guest_bytes{},value{};Kind kind{};Span source,destination;};
struct Hit {Event event;unsigned node{};U64 overlap_bytes{};bool physical_match{};};
struct Node {Span span;U64 before{};unsigned depth{};};
struct PhysicalCoverage {unsigned node{};U64 buckets{},missing{},evicted{},omitted{};};
struct Review {U64 total{},oldest{},evicted{},alias_omitted{},invalid_ranges{},matches_omitted{},nodes_omitted{};
    bool physical_enabled{};U64 physical_buckets{},physical_bucket_omitted{},physical_hits_omitted{};
    std::vector<PhysicalCoverage> physical_coverage;std::vector<Hit> physical_hits;
    std::vector<Node> nodes;std::vector<Hit> hits;std::vector<Event> lifecycle;};
class Registry {
    struct Alias {U64 metal{},base{},length{},physical{},pool_bytes{},pool_epoch{};bool pool{};};
    struct Native {U64 epoch{},references{};};
    std::mutex mutex;
    std::unordered_map<U64,Alias> aliases;
    std::unordered_map<U64,Native> natives;
    std::vector<Event> ring;
    U64 serial{},epochs{},pool_epochs{},alias_omitted{},invalid_ranges{};
    size_t alias_limit;
    struct PhysicalBucket {std::array<Event,4> transfers{},intents{};U64 transfer_count{},intent_count{};};
    std::unordered_map<U64,PhysicalBucket> physical_pages;
    bool physical_enabled;
    U64 physical_bucket_omitted{};
    static constexpr U64 page_shift=18,page_limit=32768,event_page_limit=1024;
    void RememberPhysical(const Event& e){
        if(!physical_enabled||!Writer(e.kind)||!e.destination.pool||!e.destination.pool_epoch)return;
        const auto& d=e.destination;
        if(!SpanValid(d.physical,d.bytes)){++physical_bucket_omitted;return;}
        const U64 first=d.physical>>page_shift,last=(d.physical+d.bytes-1)>>page_shift;
        const U64 count=std::min(last-first+1,event_page_limit);
        physical_bucket_omitted+=last-first+1-count;
        for(U64 at=first;at<first+count;++at){
            auto it=physical_pages.find(at);
            if(it==physical_pages.end()){
                if(physical_pages.size()>=page_limit){++physical_bucket_omitted;continue;}
                it=physical_pages.try_emplace(at).first;
            }
            auto& b=it->second;
            // Frequent shader binding intents must not evict actual encoded transfers.
            if(e.kind==Kind::ShaderWriteIntent)b.intents[b.intent_count++%4]=e;
            else b.transfers[b.transfer_count++%4]=e;
        }
    }
    void Add(Event e){
        e.sequence=++serial;
        struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
        e.ns=U64(now.tv_sec)*1000000000u+U64(now.tv_nsec);
        if(!ring.empty())ring[(e.sequence-1)%ring.size()]=e;
        RememberPhysical(e);
    }
    Span Resolve(U64 metal,U64 offset,U64 bytes) const {
        Span s;s.metal=metal;s.offset=offset;s.bytes=bytes;
        if(auto it=natives.find(metal);it!=natives.end())s.epoch=it->second.epoch;
        bool conflicting=false;
        for(const auto& [backend,a]:aliases){(void)backend;
            if(!a.pool||a.metal!=metal||offset<a.base||offset-a.base>a.pool_bytes||bytes>a.pool_bytes-(offset-a.base))continue;
            const U64 delta=offset-a.base;if(a.physical>UINT64_MAX-delta||!SpanValid(a.physical+delta,bytes))continue;
            if(s.pool&&(s.physical!=a.physical+delta||s.pool_epoch!=a.pool_epoch))conflicting=true;
            s.pool=true;s.physical=a.physical+delta;s.pool_epoch=a.pool_epoch;
        }
        if(conflicting)s.pool=false;
        return s;
    }
    void RemoveLocked(U64 backend){
        const auto it=aliases.find(backend);if(it==aliases.end())return;
        Event e;e.kind=Kind::Remove;e.value=backend;e.destination=Resolve(it->second.metal,it->second.base,it->second.length);Add(e);
        const U64 metal=it->second.metal;aliases.erase(it);
        auto n=natives.find(metal);if(n!=natives.end()&&!--n->second.references)natives.erase(n);
    }
public:
    explicit Registry(size_t capacity=65536,size_t live=8192,bool physical=false):ring(capacity),alias_limit(live),physical_enabled(physical){}
    void Register(U64 backend,U64 metal,U64 base,U64 length){
        std::lock_guard lock(mutex);RemoveLocked(backend);
        if(!metal||aliases.size()>=alias_limit){++alias_omitted;return;}
        auto [it,fresh]=natives.try_emplace(metal);if(fresh)it->second.epoch=++epochs;++it->second.references;
        aliases[backend]={metal,base,length,0,0,0,false};
        Event e;e.kind=Kind::Register;e.value=backend;e.destination=Resolve(metal,base,length);Add(e);
    }
    Span NativeSpan(U64 metal,U64 offset,U64 bytes){std::lock_guard lock(mutex);return Resolve(metal,offset,bytes);}
    Span Describe(U64 backend,U64 offset,U64 bytes){
        std::lock_guard lock(mutex);auto it=aliases.find(backend);
        if(it==aliases.end()||offset>it->second.length||bytes>it->second.length-offset||offset>UINT64_MAX-it->second.base)return {};
        return Resolve(it->second.metal,it->second.base+offset,bytes);
    }
    void Remove(U64 backend){std::lock_guard lock(mutex);RemoveLocked(backend);}
    void Pool(U64 backend,U64 physical,U64 bytes){
        std::lock_guard lock(mutex);auto it=aliases.find(backend);
        if(it==aliases.end()||!SpanValid(physical,bytes)||bytes>it->second.length){++alias_omitted;return;}
        U64 epoch=0;for(const auto& [key,a]:aliases){(void)key;if(a.pool){epoch=a.pool_epoch;break;}}
        if(!epoch)epoch=++pool_epochs;
        it->second.pool=true;it->second.physical=physical;it->second.pool_bytes=bytes;it->second.pool_epoch=epoch;
        Event e;e.kind=Kind::Pool;e.value=backend;e.destination=Resolve(it->second.metal,it->second.base,bytes);Add(e);
    }
    void NativeWrite(Kind kind,U64 command,U64 source,U64 from,U64 destination,U64 to,U64 bytes,U64 value=0){
        std::lock_guard lock(mutex);
        if(!SpanValid(to,bytes)||(source&&!SpanValid(from,bytes))){++invalid_ranges;return;}
        Event e;e.kind=kind;e.command=command;e.value=value;e.destination=Resolve(destination,to,bytes);
        if(source)e.source=Resolve(source,from,bytes);
        Add(e);
    }
    void Binding(U64 backend,U64 offset,U64 bytes,U64 guest,U64 shader,U64 slot,U64 tick){
        std::lock_guard lock(mutex);auto a=aliases.find(backend);
        if(a==aliases.end()||offset>UINT64_MAX-a->second.base||!SpanValid(a->second.base+offset,bytes)){++alias_omitted;return;}
        Event e;e.kind=Kind::ShaderWriteIntent;e.shader=shader;e.slot=slot;e.tick=tick;e.guest=guest;e.guest_bytes=bytes;
        e.destination=Resolve(a->second.metal,a->second.base+offset,bytes);Add(e);
    }
    void CpuCopy(U64 backend,U64 offset,U64 bytes,U64 physical,U64 guest,U64 shader,U64 slot,U64 tick){
        std::lock_guard lock(mutex);auto a=aliases.find(backend);
        if(a==aliases.end()||offset>UINT64_MAX-a->second.base||!SpanValid(a->second.base+offset,bytes)||!SpanValid(physical,bytes)){++alias_omitted;return;}
        Event e;e.kind=Kind::CpuSnapshot;e.shader=shader;e.slot=slot;e.tick=tick;e.guest=guest;e.guest_bytes=bytes;
        U64 epoch=0;for(const auto& [key,p]:aliases){(void)key;if(p.pool){epoch=p.pool_epoch;break;}}
        e.source={0,0,0,bytes,physical,epoch,epoch!=0};e.destination=Resolve(a->second.metal,a->second.base+offset,bytes);Add(e);
    }
    void Mapping(U64 guest,U64 bytes,bool mapped){
        std::lock_guard lock(mutex);Event e;e.kind=mapped?Kind::Map:Kind::Unmap;e.guest=guest;e.guest_bytes=bytes;Add(e);
    }
    static bool Match(const Span& a,const Span& b,U64& start,U64& bytes,bool& physical){
        physical=a.pool&&b.pool&&a.pool_epoch&&a.pool_epoch==b.pool_epoch;
        if(!physical&&(!a.epoch||a.epoch!=b.epoch||a.metal!=b.metal))return false;
        const U64 x=physical?a.physical:a.offset,y=physical?b.physical:b.offset;
        if(!Overlap(x,a.bytes,y,b.bytes))return false;
        start=std::max(x,y);bytes=std::min(x+a.bytes,y+b.bytes)-start;return true;
    }
    Review Inspect(U64 metal,U64 offset,U64 bytes,U64 guest=0,U64 guest_bytes=0){
        std::lock_guard lock(mutex);Review out;out.total=serial;out.evicted=serial>ring.size()?serial-ring.size():0;
        out.oldest=ring.empty()?serial+1:serial>ring.size()?serial-ring.size()+1:1;
        out.alias_omitted=alias_omitted;out.invalid_ranges=invalid_ranges;
        out.nodes.push_back({Resolve(metal,offset,bytes),serial+1,0});
        for(unsigned n=0;n<out.nodes.size();++n){const auto node=out.nodes[n];unsigned kept=0;
            for(U64 seq=std::min(serial,node.before-1);seq>=out.oldest&&seq;--seq){
                const auto& e=ring[(seq-1)%ring.size()];U64 at=0,count=0;bool physical=false;
                if(!Writer(e.kind)||!Match(node.span,e.destination,at,count,physical))continue;
                if(kept++>=8){++out.matches_omitted;continue;}
                out.hits.push_back({e,n,count,physical});
                if(!e.source.bytes||node.depth>=3)continue;
                const U64 delta=at-(physical?e.destination.physical:e.destination.offset);
                if(delta>=e.source.bytes||count>e.source.bytes-delta)continue;
                if(out.nodes.size()>=16){++out.nodes_omitted;continue;}
                Span child=e.source;child.offset+=delta;if(child.pool)child.physical+=delta;child.bytes=count;
                bool duplicate=false;for(const auto& prior:out.nodes)if(prior.before==seq&&prior.span.metal==child.metal&&prior.span.epoch==child.epoch&&prior.span.offset==child.offset&&prior.span.bytes==child.bytes&&prior.span.pool==child.pool&&prior.span.physical==child.physical&&prior.span.pool_epoch==child.pool_epoch)duplicate=true;
                if(!duplicate)out.nodes.push_back({child,seq,node.depth+1});
            }
        }
        for(U64 seq=serial;seq>=out.oldest&&seq;--seq){const auto& e=ring[(seq-1)%ring.size()];
            const bool native=(e.kind==Kind::Register||e.kind==Kind::Remove||e.kind==Kind::Pool)&&e.destination.metal==metal;
            const bool mapping=(e.kind==Kind::Map||e.kind==Kind::Unmap)&&Overlap(guest,guest_bytes,e.guest,e.guest_bytes);
            if(native||mapping){if(out.lifecycle.size()<32)out.lifecycle.push_back(e);else ++out.matches_omitted;}
        }
        out.physical_enabled=physical_enabled;out.physical_buckets=physical_pages.size();
        out.physical_bucket_omitted=physical_bucket_omitted;
        if(physical_enabled)for(unsigned n=0;n<out.nodes.size();++n){
            const auto node=out.nodes[n];const auto& span=node.span;
            if(!span.pool||!SpanValid(span.physical,span.bytes))continue;
            const U64 first=span.physical>>page_shift,last=(span.physical+span.bytes-1)>>page_shift;
            PhysicalCoverage coverage;coverage.node=n;coverage.buckets=std::min(last-first+1,U64(256));
            coverage.omitted=last-first+1-coverage.buckets;
            std::vector<Hit> found;
            for(U64 at=first;at<first+coverage.buckets;++at){
                auto it=physical_pages.find(at);if(it==physical_pages.end()){++coverage.missing;continue;}
                const auto& b=it->second;
                coverage.evicted+=(b.transfer_count>4?b.transfer_count-4:0)+(b.intent_count>4?b.intent_count-4:0);
                for(unsigned j=0;j<8;++j){const Event& e=j<4?b.transfers[j]:b.intents[j-4];
                    U64 start=0,bytes=0;bool physical=false;
                    if(!e.sequence||e.sequence>=node.before||!Match(span,e.destination,start,bytes,physical)||!physical)continue;
                    if(std::none_of(found.begin(),found.end(),[&](const Hit& h){return h.event.sequence==e.sequence;}))found.push_back({e,n,bytes,true});
                }
            }
            std::sort(found.begin(),found.end(),[](const Hit& a,const Hit& b){return a.event.sequence>b.event.sequence;});
            unsigned transfers=0,intents=0;
            for(const auto& hit:found){auto& count=hit.event.kind==Kind::ShaderWriteIntent?intents:transfers;
                if(count++<16)out.physical_hits.push_back(hit);else ++out.physical_hits_omitted;}
            out.physical_coverage.push_back(coverage);
        }
        return out;
    }
};
inline Registry& Get(){static auto* p=new Registry(65536,8192,PhysicalEnabled());return *p;}
inline void Register(U64 backend,U64 metal,U64 base,U64 length){if(Enabled())Get().Register(backend,metal,base,length);}
inline void Remove(U64 backend){if(Enabled())Get().Remove(backend);}
inline void Pool(U64 backend,U64 physical,U64 bytes){if(Enabled())Get().Pool(backend,physical,bytes);}
inline void Mapping(U64 guest,U64 bytes,bool mapped){if(Enabled())Get().Mapping(guest,bytes,mapped);}
inline void Binding(U64 backend,U64 offset,U64 bytes,U64 guest,U64 shader,U64 slot,U64 tick){if(Enabled())Get().Binding(backend,offset,bytes,guest,shader,slot,tick);}
inline void NativeWrite(Kind kind,U64 command,U64 source,U64 from,U64 destination,U64 to,U64 bytes,U64 value=0){if(Enabled())Get().NativeWrite(kind,command,source,from,destination,to,bytes,value);}
template<class Query> void CpuCopy(U64 backend,U64 offset,U64 bytes,U64 guest,U64 shader,U64 slot,U64 tick,Query query){
    if(!Enabled()||!SpanValid(guest,bytes)||!SpanValid(offset,bytes))return;
    U64 done=0;unsigned parts=0;
    while(done<bytes&&parts++<256){U64 physical=0;uintptr_t end=0;
        if(!query(guest+done,&physical,&end)||end<=guest+done)break;
        const U64 n=std::min<U64>(bytes-done,end-guest-done);
        Get().CpuCopy(backend,offset+done,n,physical,guest+done,shader,slot,tick);done+=n;
    }
    if(done!=bytes)std::fprintf(stderr,"Buffer history: CPU copy mapping incomplete copied=%llu requested=%llu; source attribution partial\n",(unsigned long long)done,(unsigned long long)bytes);
}
inline void JsonSpan(std::ostream& s,const Span& p){s<<"{\"metal\":"<<p.metal<<",\"epoch\":"<<p.epoch<<",\"offset\":"<<p.offset<<",\"bytes\":"<<p.bytes<<",\"pool_known\":"<<(p.pool?"true":"false")<<",\"physical\":"<<p.physical<<",\"pool_epoch\":"<<p.pool_epoch<<'}';}
inline void JsonEvent(std::ostream& s,const Event& e){s<<"{\"sequence\":"<<e.sequence<<",\"ns\":"<<e.ns<<",\"kind\":\""<<Name(e.kind)<<"\",\"command\":"<<e.command<<",\"shader\":"<<e.shader<<",\"slot\":"<<e.slot<<",\"tick\":"<<e.tick<<",\"guest\":"<<e.guest<<",\"guest_bytes\":"<<e.guest_bytes<<",\"value\":"<<e.value<<",\"source\":";JsonSpan(s,e.source);s<<",\"destination\":";JsonSpan(s,e.destination);s<<'}';}
inline void Dump(unsigned slot,U64 metal,U64 offset,U64 bytes,U64 guest=0,U64 guest_bytes=0){
    if(!Enabled())return;
    static std::atomic<U64> reports{};const U64 report=++reports;
    if(report>32){if(report==33)std::fprintf(stderr,"Buffer history: report budget exhausted; later reports omitted\n");return;}
    const auto r=Get().Inspect(metal,offset,bytes,guest,guest_bytes);
    for(unsigned i=0;i<r.nodes.size();++i) {
        const auto& span=r.nodes[i].span;
        if(span.pool)SotcPageHistoryBridge::Dump(static_cast<unsigned>(report),slot,i,span.physical,span.bytes);
    }
    std::ostringstream s;s<<"{\"schema\":3,\"clock\":\"CLOCK_MONOTONIC\",\"report\":"<<report<<",\"slot\":"<<slot<<",\"total_events\":"<<r.total<<",\"oldest_retained\":"<<r.oldest<<",\"evicted\":"<<r.evicted<<",\"alias_omitted\":"<<r.alias_omitted<<",\"invalid_ranges\":"<<r.invalid_ranges<<",\"matches_omitted\":"<<r.matches_omitted<<",\"nodes_omitted\":"<<r.nodes_omitted<<",\"nodes\":[";
    for(size_t i=0;i<r.nodes.size();++i){if(i)s<<',';const auto& n=r.nodes[i];s<<"{\"before_sequence\":"<<n.before<<",\"depth\":"<<n.depth<<",\"span\":";JsonSpan(s,n.span);s<<'}';}
    s<<"],\"candidates\":[";
    for(size_t i=0;i<r.hits.size();++i){if(i)s<<',';const auto& h=r.hits[i];s<<"{\"node\":"<<h.node<<",\"overlap_bytes\":"<<h.overlap_bytes<<",\"physical_match\":"<<(h.physical_match?"true":"false")<<",\"event\":";JsonEvent(s,h.event);s<<'}';}
    s<<"],\"lifecycle\":[";for(size_t i=0;i<r.lifecycle.size();++i){if(i)s<<',';JsonEvent(s,r.lifecycle[i]);}
    s<<"],\"physical_history\":{\"enabled\":"<<(r.physical_enabled?"true":"false")<<",\"bucket_bytes\":262144,\"bucket_limit\":32768,\"allocated_buckets\":"<<r.physical_buckets<<",\"event_buckets_omitted\":"<<r.physical_bucket_omitted<<",\"hits_omitted\":"<<r.physical_hits_omitted<<",\"coverage\":[";
    for(size_t i=0;i<r.physical_coverage.size();++i){if(i)s<<',';const auto& c=r.physical_coverage[i];s<<"{\"node\":"<<c.node<<",\"buckets\":"<<c.buckets<<",\"missing\":"<<c.missing<<",\"evicted_event_bucket_records\":"<<c.evicted<<",\"omitted\":"<<c.omitted<<'}';}
    s<<"],\"candidates\":[";
    for(size_t i=0;i<r.physical_hits.size();++i){if(i)s<<',';const auto& h=r.physical_hits[i];s<<"{\"node\":"<<h.node<<",\"overlap_bytes\":"<<h.overlap_bytes<<",\"event\":";JsonEvent(s,h.event);s<<'}';}
    s<<"]},\"limits\":\"Physical archive: four transfers and four intents per bucket, 1024 buckets per event, 256 buckets and sixteen candidates per class per node; explicit omissions. Physical candidates respect pool epochs and node copy cutoffs but are not recursively expanded. Host observation order, not GPU completion order. Encoded copies and binding write intents are candidates, not completed/exclusive writers. Physical overlap is not guest allocation identity. Image download spans include padding. CPU mappings are later observations, not pinned lifetimes. CPU stores, image shader writes and evicted history may be absent.\"}\n";
    const char* dir=std::getenv("SOTC_SCATTER_GUARD_DIR");std::string path;bool saved=false;
    if(dir&&*dir=='/'){path=std::string(dir)+"/history-"+std::to_string(report)+"-slot-"+std::to_string(slot)+".json";const auto data=s.str();int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
        if(fd>=0){size_t done=0;while(done<data.size()){auto n=write(fd,data.data()+done,data.size()-done);if(n<=0)break;done+=size_t(n);}saved=done==data.size()&&fsync(fd)==0;if(close(fd)!=0)saved=false;}}
    std::fprintf(stderr,"Buffer history report: slot=%u events=%llu candidates=%zu nodes=%zu evicted=%llu omitted=%llu saved=%u path=%s; metadata candidates, not proven writers\n",slot,(unsigned long long)r.total,r.hits.size(),r.nodes.size(),(unsigned long long)r.evicted,(unsigned long long)(r.matches_omitted+r.nodes_omitted),unsigned(saved),path.c_str());
}
} // namespace SotcBufferHistory
