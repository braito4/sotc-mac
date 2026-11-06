#pragma once
// Ordered GPU copies of selected dispatch inputs; no guest-memory CPU reads or repairs.
#include "gpu_scatter_probe_policy.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace SotcScatterProbe {
inline const std::vector<Config>& Configs() {
    static const auto configs=[] {
        std::vector<Config> v;const char* p=std::getenv("SOTC_SCATTER_PROBE");
        if(p&&*p&&!Parse(p,v)){std::fprintf(stderr,"STOP_GPU_SCATTER_CONFIG: invalid layout\n");std::exit(24);}
        if(!v.empty())std::fprintf(stderr,"GPU scatter probe: enabled entries=%zu; ordered pre-dispatch snapshots; 8 MiB per capture, 32 MiB pending; original dispatch unchanged\n",v.size());
        return v;
    }();return configs;
}
inline std::atomic<U64> pending_bytes{},encoded{},completed{},skipped{},violations{};
constexpr U64 CopyLimit=8ull<<20, PendingLimit=32ull<<20;
struct Lease { U64 size;~Lease(){pending_bytes.fetch_sub(size);} };
inline std::shared_ptr<Lease> Reserve(U64 bytes) {
    U64 old=pending_bytes.load();
    do {if(bytes>PendingLimit||old>PendingLimit-bytes)return {};}
    while(!pending_bytes.compare_exchange_weak(old,old+bytes));
    return std::shared_ptr<Lease>(new Lease{bytes});
}
inline bool Periodic() {
    static std::atomic<std::int64_t> last{};
    auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto prev=last.load();return now-prev>=5&&last.compare_exchange_strong(prev,now);
}
inline void Omitted(const char* why,const std::string& entry) {
    const U64 n=++skipped;
    if(n<=4||Periodic())std::fprintf(stderr,"GPU scatter unavailable: reason=%s entry=%s skipped=%llu; coverage unknown\n",why,entry.c_str(),(unsigned long long)n);
}
// Must be included after mtl_frame.h. Does not change Owned or shared ABI layouts.
template<class Owned,class BeginBlit>
inline void Capture(Owned& o,id<MTLDevice> device,BeginBlit begin_blit,unsigned x,unsigned y,unsigned z,
                    id<MTLBuffer> indirect=nil,U64 indirect_offset=0) {
    if(Configs().empty()||!o.compute_pipeline||!o.compute_pipeline->shader)return;
    const auto* shader=o.compute_pipeline->shader;const Config* found=nullptr;
    for(const auto& c:Configs())if(c.entry==shader->entry){found=&c;break;}
    if(!found)return;
    const Config c=*found;
    if(o.compute_pipeline->threads_per_group[0]!=c.threads_x||
       o.compute_pipeline->threads_per_group[1]!=1||o.compute_pipeline->threads_per_group[2]!=1){Omitted("thread_layout",c.entry);return;}
    if(!indirect&&(y!=1||z!=1)){Omitted("multidimensional_dispatch",c.entry);return;}
    const auto get=[&](unsigned slot)->const MetalFrame::Bound* {
        for(const auto& b:shader->bindings)if(b.kind==Metal::ShaderBinding::Kind::Buffer&&b.msl_index==slot&&b.count==1){
            auto it=o.compute_bindings.find((b.set<<16)|b.binding);
            if(it!=o.compute_bindings.end()&&it->second.elements.size()==1)return &it->second.elements[0];
        }return nullptr;
    };
    std::array<MetalFrame::Bound,4> bound{};
    const std::array<unsigned,4> slots{c.count_slot,c.index_slot,c.output_slot[0],c.output_slot[1]};
    const std::array<unsigned,4> adjust_at{c.count_adjust,c.index_adjust,c.output_adjust[0],c.output_adjust[1]};
    for(unsigned i=0;i<4;++i){
        const auto* b=get(slots[i]);const U64 adjust=o.push[adjust_at[i]];
        if(!b||!b->buffer||adjust%4||adjust>b->size||!ValidSpan(b->offset,b->size,b->buffer.length)){
            Omitted("binding_extent_or_alignment",c.entry);return;
        }
        bound[i]=*b;bound[i].offset+=adjust;bound[i].size-=adjust;
    }
    if(!ValidSpan(c.count_field,4,bound[0].size)||
       (indirect&&!ValidSpan(indirect_offset,12,indirect.length))){Omitted("count_or_indirect_extent",c.entry);return;}
    const U64 input_words=bound[1].size/4;
    const U64 dispatched=U64(x)*c.threads_x;
    const U64 copy_words=std::min<U64>({input_words,CopyLimit/4,indirect?UINT64_MAX:dispatched});
    const U64 bytes=16+copy_words*4;
    auto lease=Reserve(bytes);if(!lease){Omitted("pending_budget",c.entry);return;}
    id<MTLBuffer> snapshot=[device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    if(!snapshot){Omitted("snapshot_allocation",c.entry);return;}
    const U64 seq=++encoded;
    // BlitEncoder ends the preceding encoder. Capture is ordered before the original dispatch.
    auto blit=begin_blit();
    [blit copyFromBuffer:bound[0].buffer sourceOffset:bound[0].offset+c.count_field toBuffer:snapshot destinationOffset:0 size:4];
    if(indirect)[blit copyFromBuffer:indirect sourceOffset:indirect_offset toBuffer:snapshot destinationOffset:4 size:12];
    if(copy_words)[blit copyFromBuffer:bound[1].buffer sourceOffset:bound[1].offset toBuffer:snapshot destinationOffset:16 size:copy_words*4];
    std::array<U64,4> ids{};for(unsigned i=0;i<4;++i)ids[i]=reinterpret_cast<std::uintptr_t>((__bridge void*)bound[i].buffer);
    const bool is_indirect=indirect!=nil;
    if(seq<=4)std::fprintf(stderr,"GPU scatter encoded: sequence=%llu entry=%s words=%llu indirect=%u; awaiting Metal completion\n",(unsigned long long)seq,c.entry.c_str(),(unsigned long long)copy_words,unsigned(is_indirect));
    [o.cmd addCompletedHandler:^(id<MTLCommandBuffer> command) {
        (void)lease; // Retains accounting and snapshot until completion or command disposal.
        if(command.status!=MTLCommandBufferStatusCompleted){Omitted("command_failed",c.entry);return;}
        const U64 done=++completed;
        const auto* data=static_cast<const unsigned char*>(snapshot.contents);
        std::uint32_t count;std::memcpy(&count,data,4);
        U64 threads=dispatched;
        if(is_indirect){std::uint32_t dims[3];std::memcpy(dims,data+4,12);
            if(dims[1]!=1||dims[2]!=1){Omitted("indirect_dimensions",c.entry);return;}threads=U64(dims[0])*c.threads_x;}
        auto r=Inspect(data+16,copy_words,count,threads,input_words,{bound[2].size,bound[3].size},c.stride);
        const bool bad=r.invalid||r.input_short;if(bad)++violations;
        if(bad||seq<=4||Periodic()){
            std::fprintf(stderr,"GPU scatter result: sequence=%llu entry=%s count=%u active=%llu checked=%llu input_capacity=%llu invalid=%llu input_short=%u truncated=%u first_thread=%llu first_index=%llu max_index=%llu encoded=%llu completed=%llu skipped=%llu violations=%llu; ordered GPU input snapshot, not writer attribution or lifetime proof\n",(unsigned long long)seq,c.entry.c_str(),count,(unsigned long long)r.active,(unsigned long long)r.checked,(unsigned long long)input_words,(unsigned long long)r.invalid,unsigned(r.input_short),unsigned(r.truncated),(unsigned long long)r.first_thread,(unsigned long long)r.first_index,(unsigned long long)r.maximum,(unsigned long long)encoded.load(),(unsigned long long)done,(unsigned long long)skipped.load(),(unsigned long long)violations.load());
            for(unsigned i=0;i<4;++i)std::fprintf(stderr,"  scatter binding: sequence=%llu slot=%u metal=%#llx offset=%llu bytes=%llu stride=%u\n",(unsigned long long)seq,slots[i],(unsigned long long)ids[i],(unsigned long long)bound[i].offset,(unsigned long long)bound[i].size,i<2?4:c.stride[i-2]);
            std::fflush(stderr);
        }
    }];
}
} // namespace SotcScatterProbe
