#pragma once
// Ordered producer inputs/counter/output. Opt-in direct selection removes the
// fragmentation prerequisite for the exact selected large output. No writeback.
#include "gpu_fragment_producer_metal.h"
#include "gpu_binding_audit.h"

namespace SotcProducerBundle {
using SotcFragment::U64;
using SotcFragmentProducer::Quote;
using SotcFragmentProducer::Hex;
[[noreturn]] inline void Stop(const char* reason) {
    std::fprintf(stderr,"STOP_GPU_PRODUCER_BUNDLE: reason=%s; exit=23\n",reason);
    std::fflush(stderr);std::_Exit(23);
}
struct Config {std::vector<std::string> entries;unsigned output{},counter{};U64 minimum{};bool any_large_output{};};
inline const Config& Settings() {
    static const Config c=[] {
        Config c;const char* env=std::getenv("SOTC_PRODUCER_BUNDLE");if(!env||!*env)return c;
        std::string s(env);std::array<std::string,4> fields;
        size_t begin=0;
        for(unsigned i=0;i<4;++i){const auto end=s.find(',',begin);
            if((i<3&&end==std::string::npos)||(i==3&&end!=std::string::npos))Stop("layout");
            fields[i]=s.substr(begin,end==std::string::npos?end:end-begin);begin=end+1;
        }
        // At most two exact variants sharing the reviewed output/counter layout.
        // Each gets half the existing budget so an early variant cannot starve the other.
        size_t at=0;
        do {
            const auto end=fields[0].find('|',at);
            const auto entry=fields[0].substr(at,end==std::string::npos?end:end-at);
            if(entry.empty()||entry.size()>200||c.entries.size()==2)Stop("entry");
            for(char x:entry)if(!((x>='a'&&x<='z')||(x>='A'&&x<='Z')||(x>='0'&&x<='9')||x=='_'))Stop("entry");
            for(const auto& prior:c.entries)if(prior==entry)Stop("duplicate_entry");
            c.entries.push_back(entry);
            if(end==std::string::npos)break;
            at=end+1;
        } while(true);
        std::array<U64,3> values{};
        for(unsigned i=0;i<3;++i){if(fields[i+1].empty())Stop("layout");
            for(char x:fields[i+1]){if(x<'0'||x>'9'||values[i]>(8ull<<20))Stop("layout");values[i]=values[i]*10+unsigned(x-'0');}}
        if(values[0]>255||values[1]>255||values[0]==values[1]||!values[2]||values[2]>(8ull<<20))Stop("layout");
        c.output=unsigned(values[0]);c.counter=unsigned(values[1]);c.minimum=values[2];
        const char* direct=std::getenv("SOTC_PRODUCER_DIRECT");
        c.any_large_output=direct&&!std::strcmp(direct,"1");
        const char* dir=std::getenv("SOTC_SCATTER_GUARD_DIR");
        if(!dir||dir[0]!='/'||!SotcFragment::Enabled())Stop("evidence_or_provenance_missing");
        std::fprintf(stderr,"Producer bundle: selection=%s; max 32 captures, 8 MiB per binding, 32 MiB per capture; diagnostic waits, no writeback\n",c.any_large_output?"selected large output, no fragmentation prerequisite":"arm on selected fragmented output then include direct");
        return c;
    }();return c;
}
inline std::array<std::atomic<bool>,2> armed{};
inline std::array<std::atomic<U64>,2> entry_count{};
inline std::atomic<U64> selected_count{},budget_omitted{};
struct Slice {
    id<MTLBuffer> buffer=nil;
    unsigned slot{};
    U64 offset{},bytes{},copied{},before{},after{},after_bytes{};
    SotcBindingAudit::Record audit;
};
class Capture {
    std::vector<Slice> slices;
    id<MTLBuffer> snapshot=nil;
    U64 seq{},total{};
    std::string metadata;
public:
    Capture(const Capture&)=delete;Capture& operator=(const Capture&)=delete;
    template<class Owned,class BeginBlit>
    Capture(Owned& o,id<MTLDevice> device,BeginBlit begin_blit,unsigned x,unsigned y,unsigned z,
            id<MTLBuffer> indirect=nil,U64 indirect_offset=0) {
        const auto& c=Settings();
        if(c.entries.empty()||!o.compute_pipeline||!o.compute_pipeline->shader)return;
        const auto* shader=o.compute_pipeline->shader;
        size_t selection=0;
        while(selection<c.entries.size()&&shader->entry!=c.entries[selection])++selection;
        if(selection==c.entries.size())return;
        const auto find=[&](unsigned slot)->const MetalFrame::Bound* {
            const MetalFrame::Bound* result=nullptr;
            for(const auto& b:shader->bindings) {
                if(b.kind!=Metal::ShaderBinding::Kind::Buffer||b.msl_index!=slot)continue;
                auto it=o.compute_bindings.find((b.set<<16)|b.binding);
                if(result||b.count!=1||it==o.compute_bindings.end()||it->second.elements.size()!=1)Stop("ambiguous_or_missing_binding");
                result=&it->second.elements[0];
            }
            return result;
        };
        const auto* output=find(c.output);if(!output||!output->buffer||output->size<c.minimum)return;
        const U64 identity=reinterpret_cast<std::uintptr_t>((__bridge void*)output->buffer);
        // Find is a bounded copy observation, not a completed producer or page lifetime claim.
        const auto origin=SotcFragment::Get().Find(identity,output->offset,output->size);
        const bool fragment=origin.found&&origin.copy.written;
        if(fragment||c.any_large_output)armed[selection].store(true);
        if(!armed[selection].load())return;
        if(++entry_count[selection]>32/c.entries.size()){
            const auto n=++budget_omitted;
            if(n==1||n%256==0)std::fprintf(stderr,"Producer bundle: budget exhausted omitted=%llu entry=%s\n",(unsigned long long)n,c.entries[selection].c_str());
            return;
        }
        seq=++selected_count;
        if(!find(c.counter))Stop("missing_counter");
        if(indirect&&(indirect_offset%4||!SotcScatterProbe::ValidSpan(indirect_offset,12,indirect.length)))Stop("indirect_extent");
        std::ostringstream m;
        m<<"{\"schema\":1,\"sequence\":"<<seq<<",\"entry\":"<<Quote(c.entries[selection])
         <<",\"selection_index\":"<<selection<<",\"selection_budget\":"<<32/c.entries.size()
         <<",\"output_slot\":"<<c.output<<",\"counter_slot\":"<<c.counter
         <<",\"output_fallback_observed\":"<<(fragment?"true":"false")
         <<",\"fragment_prerequisite_disabled\":"<<(c.any_large_output?"true":"false")
         <<",\"indirect\":"<<(indirect?"true":"false")<<",\"groups_offset\":0,\"push_offset\":16,\"push_bytes\":"<<o.push.size()
         <<",\"threads_per_group\":["<<o.compute_pipeline->threads_per_group[0]<<','<<o.compute_pipeline->threads_per_group[1]<<','<<o.compute_pipeline->threads_per_group[2]<<"],\"bindings\":[";
        total=512;
        for(const auto& b:shader->bindings){
            if(b.kind!=Metal::ShaderBinding::Kind::Buffer)continue;
            if(slices.size()>=16)Stop("binding_budget");
            const auto* bound=find(b.msl_index);
            if(!bound||!bound->buffer||!bound->size||bound->offset%4||bound->size%4||
               !SotcScatterProbe::ValidSpan(bound->offset,bound->size,bound->buffer.length))Stop("binding_extent");
            Slice v;v.buffer=bound->buffer;v.slot=b.msl_index;v.offset=bound->offset;v.bytes=bound->size;
            v.copied=std::min<U64>(v.bytes,8ull<<20);v.before=total;total=(total+v.copied+255)&~U64(255);
            if(v.slot==c.output||v.slot==c.counter){v.after=total;v.after_bytes=v.copied;total=(total+v.copied+255)&~U64(255);}
            if(total>(32ull<<20))Stop("snapshot_budget");
            const U64 metal=reinterpret_cast<std::uintptr_t>((__bridge void*)bound->buffer);
            const auto match=SotcFragment::Get().Find(metal,v.offset,v.bytes);
            if(b.set==0){
                v.audit=SotcBindingAudit::Find(b.binding,metal,v.offset,v.bytes);
                if(seq==1)SotcBindingAudit::Sample(unsigned(seq),v.slot,0,v.audit);
            }
            if(seq==1)SotcFragment::DumpHistory(v.slot,metal,v.offset,v.bytes);
            if(!slices.empty())m<<',';
            m<<"{\"slot\":"<<v.slot<<",\"set\":"<<b.set<<",\"binding\":"<<b.binding<<",\"metal\":"<<metal
             <<",\"offset\":"<<v.offset<<",\"bytes\":"<<v.bytes<<",\"copied\":"<<v.copied
             <<",\"before\":"<<v.before<<",\"after\":"<<v.after<<",\"after_bytes\":"<<v.after_bytes
             <<",\"truncated\":"<<(v.copied!=v.bytes?"true":"false")<<",\"fallback_observed\":"<<(match.found?"true":"false");
            if(match.found){const auto& a=match.copy;
                m<<",\"copy_sequence\":"<<a.sequence<<",\"copy_guest\":"<<a.guest<<",\"copy_bytes\":"<<a.bytes
                 <<",\"copy_metal_offset\":"<<a.offset+a.metal_base<<",\"written_intent\":"<<(a.written?"true":"false")
                 <<",\"gpu_ordered_copy\":"<<(a.gpu_copy?"true":"false")
                 <<",\"full_span\":"<<(match.complete?"true":"false")<<",\"registration\":"<<a.registration
                 <<",\"tick\":"<<a.tick<<",\"generation_before_copy\":"<<a.generation_before<<",\"generation_after_copy\":"<<a.generation_after
                 <<",\"cpu_copy_head\":"<<Quote(Hex(a.head.data(),a.head_bytes))<<",\"mapping\":[";
                for(unsigned j=0;j<a.mapping.count;++j){const auto& seg=a.mapping.segments[j];if(j)m<<',';
                    m<<"{\"start\":"<<seg.start<<",\"physical\":"<<seg.physical<<",\"end\":"<<seg.end<<",\"mapped\":"<<(seg.mapped?"true":"false")<<'}';}m<<']';
            }
            m<<",\"binding_audit\":";SotcBindingAudit::Json(m,v.audit);
            m<<'}';slices.push_back(v);
        }
        m<<"],\"scope\":\"ordered binding snapshots; fallback mappings are earlier observations, not completion-time ownership\"";metadata=m.str();
        snapshot=[device newBufferWithLength:total options:MTLResourceStorageModeShared];if(!snapshot)Stop("allocation");
        std::memset(snapshot.contents,0,total);
        if(!indirect){const std::uint32_t dims[3]={x,y,z};std::memcpy(snapshot.contents,dims,12);}
        std::memcpy(static_cast<unsigned char*>(snapshot.contents)+16,o.push.data(),o.push.size());
        auto blit=begin_blit();
        for(const auto& v:slices)[blit copyFromBuffer:v.buffer sourceOffset:v.offset toBuffer:snapshot destinationOffset:v.before size:v.copied];
        if(indirect)[blit copyFromBuffer:indirect sourceOffset:indirect_offset toBuffer:snapshot destinationOffset:0 size:12];
    }
    template<class Owned,class BeginBlit,class CommitChunk>
    void Finish(Owned& o,BeginBlit begin_blit,CommitChunk commit_chunk){
        if(!snapshot)return;
        auto blit=begin_blit();
        for(const auto& v:slices)if(v.after_bytes)[blit copyFromBuffer:v.buffer sourceOffset:v.offset toBuffer:snapshot destinationOffset:v.after size:v.after_bytes];
        id<MTLCommandBuffer> command=o.cmd;dispatch_semaphore_t done=dispatch_semaphore_create(0);
        [command addCompletedHandler:^(id<MTLCommandBuffer>){dispatch_semaphore_signal(done);}];
        commit_chunk();
        if(dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,120ll*NSEC_PER_SEC)))Stop("timeout");
        if(command.status!=MTLCommandBufferStatusCompleted)Stop("command_failed");
        if(seq==1)for(const auto& v:slices)SotcBindingAudit::Sample(unsigned(seq),v.slot,1,v.audit);
        const auto* bytes=static_cast<const unsigned char*>(snapshot.contents);metadata+=",\"completed\":true,\"comparisons\":[";
        bool first=true;
        for(const auto& v:slices)if(v.after_bytes){
            U64 changed=0;for(U64 i=0;i<v.after_bytes;++i)changed+=bytes[v.before+i]!=bytes[v.after+i];
            if(!first)metadata+=',';first=false;
            metadata+="{\"slot\":"+std::to_string(v.slot)+",\"compared_bytes\":"+std::to_string(v.after_bytes)+",\"changed_bytes\":"+std::to_string(changed)+'}';
            std::fprintf(stderr,"Producer bundle result: sequence=%llu slot=%u compared=%llu changed=%llu truncated=%u\n",(unsigned long long)seq,v.slot,(unsigned long long)v.after_bytes,(unsigned long long)changed,unsigned(v.copied!=v.bytes));
        }
        metadata+="]}\n";
        const std::string base=std::string(std::getenv("SOTC_SCATTER_GUARD_DIR"))+"/bundle-"+std::to_string(seq);
        const bool bin=SotcScatterGuard::Save(base+".bin",bytes,total),meta=SotcScatterGuard::Save(base+".json",metadata.data(),metadata.size());
        std::fprintf(stderr,"Producer bundle saved: sequence=%llu bytes=%llu binary_saved=%u metadata_saved=%u path=%s\n",(unsigned long long)seq,(unsigned long long)total,unsigned(bin),unsigned(meta),base.c_str());
        if(!bin||!meta)Stop("save_failed");snapshot=nil;slices.clear();
    }
};
} // namespace SotcProducerBundle
