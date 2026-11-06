#pragma once
// Opt-in ordered before/after snapshots of a selected fragmented writable binding.
// No guest-memory writeback, binding substitution, or claim of logical allocation ownership.
#include "gpu_scatter_provenance_guard.h"
#include <sstream>

namespace SotcFragmentProducer {
using SotcFragment::U64;
struct Selection { bool enabled{}; U64 shader{}; unsigned binding{}; };
[[noreturn]] inline void Stop(const char* reason) {
    std::fprintf(stderr,"STOP_GPU_FRAGMENT_PRODUCER: reason=%s; exit=23\n",reason);
    std::fflush(stderr);std::_Exit(23);
}
inline const Selection& Selected() {
    static const Selection selected=[] {
        Selection s;
        const char* value=std::getenv("SOTC_FRAGMENT_PRODUCER");
        if (!value || !*value) return s;
        // Exact hexadecimal request-shader:decimal descriptor binding. No game identities here.
        std::string v(value);const auto split=v.find(':');
        if (split==std::string::npos || !split || split>16 || split+1==v.size()) Stop("invalid_selection");
        for (size_t i=0;i<split;++i) {
            const char c=v[i];unsigned digit;
            if(c>='0'&&c<='9')digit=c-'0';else if(c>='a'&&c<='f')digit=c-'a'+10;
            else if(c>='A'&&c<='F')digit=c-'A'+10;else Stop("invalid_shader");
            s.shader=(s.shader<<4)|digit;
        }
        for(size_t i=split+1;i<v.size();++i) {
            if(v[i]<'0'||v[i]>'9'||s.binding>255)Stop("invalid_binding");
            s.binding=s.binding*10+unsigned(v[i]-'0');
        }
        if(!s.shader||s.binding>255||!SotcFragment::Enabled())Stop("invalid_selection_or_provenance_disabled");
        const char* dir=std::getenv("SOTC_SCATTER_GUARD_DIR");
        if(!dir||dir[0]!='/')Stop("absolute_evidence_directory_required");
        s.enabled=true;
        std::fprintf(stderr,"Fragment producer: enabled; at most 16 dispatch snapshots, 8 MiB each; normal GPU completion wait; diagnostic timing, no writeback\n");
        return s;
    }();return selected;
}
inline std::atomic<U64> admitted{},omitted{};
inline std::string Hex(const void* data,size_t bytes) {
    const auto* p=static_cast<const unsigned char*>(data);std::string out;out.reserve(bytes*2);
    constexpr char digits[]="0123456789abcdef";
    for(size_t i=0;i<bytes;++i){out+=digits[p[i]>>4];out+=digits[p[i]&15];}return out;
}
inline std::string Quote(const std::string& value) {
    std::string out="\"";
    for(unsigned char c:value) {
        if(c=='"'||c=='\\'){out+='\\';out+=char(c);}
        else if(c<32){char buf[7];std::snprintf(buf,sizeof(buf),"\\u%04x",c);out+=buf;}
        else out+=char(c);
    }return out+'"';
}
class Capture {
    id<MTLBuffer> source=nil,snapshot=nil;
    U64 offset{},bytes{},prefix{},seq{};
    std::string metadata;
public:
    Capture(const Capture&)=delete;
    Capture& operator=(const Capture&)=delete;
    template<class Owned,class BeginBlit>
    Capture(Owned& o,id<MTLDevice> device,BeginBlit begin_blit,
            unsigned x,unsigned y,unsigned z,id<MTLBuffer> indirect=nil,U64 indirect_offset=0) {
        const auto& selected=Selected();
        if(!selected.enabled||!o.compute_pipeline||!o.compute_pipeline->shader)return;
        const auto* shader=o.compute_pipeline->shader;
        for(const auto& b:shader->bindings) {
            if(b.kind!=Metal::ShaderBinding::Kind::Buffer||b.count!=1||b.binding!=selected.binding||b.set!=0)continue;
            auto it=o.compute_bindings.find((b.set<<16)|b.binding);
            if(it==o.compute_bindings.end()||it->second.elements.size()!=1)continue;
            const auto& bound=it->second.elements[0];if(!bound.buffer)continue;
            const U64 metal=reinterpret_cast<std::uintptr_t>((__bridge void*)bound.buffer);
            const auto match=SotcFragment::Get().Find(metal,bound.offset,bound.size);
            if(!match.found||!match.copy.written||match.copy.shader!=selected.shader||match.copy.slot!=selected.binding)continue;
            const auto& c=match.copy;
            // Copy the exact fallback span inside the bound range, preserving any alignment prefix.
            if(c.offset>UINT64_MAX-c.metal_base)Stop("copy_offset_overflow");
            offset=c.offset+c.metal_base;bytes=c.bytes;
            if(!bytes||bytes>(8ull<<20)||offset%4||bytes%4||offset<bound.offset||
               !SotcScatterProbe::ValidSpan(offset-bound.offset,bytes,bound.size)||
               !SotcScatterProbe::ValidSpan(offset,bytes,bound.buffer.length))Stop("copy_extent");
            if(indirect&&(indirect_offset%4||!SotcScatterProbe::ValidSpan(indirect_offset,12,indirect.length)))Stop("indirect_extent");
            seq=admitted.fetch_add(1)+1;
            if(seq>16){const auto n=++omitted;if(n==1||n%256==0)std::fprintf(stderr,"Fragment producer: snapshot budget exhausted omitted=%llu\n",(unsigned long long)n);return;}
            source=bound.buffer;prefix=std::min<U64>(64,bytes);
            snapshot=[device newBufferWithLength:512+bytes options:MTLResourceStorageModeShared];
            if(!snapshot)Stop("snapshot_allocation");
            // All saved padding is initialized; no uninitialized Metal bytes leave the process.
            std::memset(snapshot.contents,0,512);
            if(!indirect){const std::uint32_t dims[3]={x,y,z};std::memcpy(snapshot.contents,dims,12);}
            std::memcpy(static_cast<unsigned char*>(snapshot.contents)+16,o.push.data(),o.push.size());
            std::ostringstream m;
            m<<"{\"schema\":1,\"sequence\":"<<seq<<",\"copy_sequence\":"<<c.sequence
             <<",\"entry\":"<<Quote(shader->entry)<<",\"msl_slot\":"<<b.msl_index
             <<",\"request_shader\":"<<c.shader<<",\"binding\":"<<c.slot
             <<",\"guest\":"<<c.guest<<",\"bytes\":"<<bytes<<",\"metal\":"<<metal
             <<",\"offset\":"<<offset<<",\"bound_offset\":"<<bound.offset<<",\"bound_bytes\":"<<bound.size
             <<",\"registration\":"<<c.registration<<",\"tick\":"<<c.tick
             <<",\"generation_before_copy\":"<<c.generation_before<<",\"generation_after_copy\":"<<c.generation_after
             <<",\"mapping_complete\":"<<(c.mapping.complete?"true":"false")<<",\"mapping\":[";
            for(unsigned i=0;i<c.mapping.count;++i){const auto& s=c.mapping.segments[i];if(i)m<<',';
                m<<"{\"start\":"<<s.start<<",\"physical\":"<<s.physical<<",\"end\":"<<s.end<<",\"mapped\":"<<(s.mapped?"true":"false")<<'}';}
            m<<"],\"cpu_copy_head\":"<<Quote(Hex(c.head.data(),c.head_bytes))
             <<",\"indirect\":"<<(indirect?"true":"false")<<",\"groups_offset\":0,\"push_offset\":16,\"push_bytes\":"<<o.push.size()
             <<",\"before_offset\":320,\"prefix_bytes\":"<<prefix<<",\"after_offset\":512,\"threads_per_group\":["
             <<o.compute_pipeline->threads_per_group[0]<<','<<o.compute_pipeline->threads_per_group[1]<<','<<o.compute_pipeline->threads_per_group[2]
             <<"],\"scope\":\"dispatch using selected fallback copy; no allocation ownership or expected-value proof\"";
            metadata=m.str();
            auto blit=begin_blit();
            [blit copyFromBuffer:source sourceOffset:offset toBuffer:snapshot destinationOffset:320 size:prefix];
            if(indirect)[blit copyFromBuffer:indirect sourceOffset:indirect_offset toBuffer:snapshot destinationOffset:0 size:12];
            return;
        }
    }
    template<class Owned,class BeginBlit,class CommitChunk>
    void Finish(Owned& o,BeginBlit begin_blit,CommitChunk commit_chunk) {
        if(!snapshot)return;
        auto blit=begin_blit(); // Ends compute encoder, orders the snapshot after the selected dispatch.
        [blit copyFromBuffer:source sourceOffset:offset toBuffer:snapshot destinationOffset:512 size:bytes];
        id<MTLCommandBuffer> command=o.cmd;
        dispatch_semaphore_t done=dispatch_semaphore_create(0);
        [command addCompletedHandler:^(id<MTLCommandBuffer>){dispatch_semaphore_signal(done);}];
        commit_chunk(); // Existing submission admission, retention and error path.
        if(dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,120ll*NSEC_PER_SEC)))Stop("snapshot_timeout");
        if(command.status!=MTLCommandBufferStatusCompleted)Stop("snapshot_command_failed");
        const auto* data=static_cast<const unsigned char*>(snapshot.contents);
        const bool equal=std::memcmp(data+320,data+512,prefix)==0;
        metadata+=",\"completed\":true,\"before_after_prefix_equal\":"+std::string(equal?"true":"false")+"}\n";
        const std::string base=std::string(std::getenv("SOTC_SCATTER_GUARD_DIR"))+"/producer-"+std::to_string(seq);
        const bool bin=SotcScatterGuard::Save(base+".bin",data,512+bytes);
        const bool meta=SotcScatterGuard::Save(base+".json",metadata.data(),metadata.size());
        std::fprintf(stderr,"Fragment producer completed: sequence=%llu bytes=%llu prefix_equal=%u binary_saved=%u metadata_saved=%u path=%s; completed snapshot, not expected output validation\n",
            (unsigned long long)seq,(unsigned long long)bytes,unsigned(equal),unsigned(bin),unsigned(meta),base.c_str());
        if(!bin||!meta)Stop("evidence_save_failed");
        snapshot=nil;source=nil;
    }
};
} // namespace SotcFragmentProducer
