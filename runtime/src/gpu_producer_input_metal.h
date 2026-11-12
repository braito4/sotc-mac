#pragma once
// Ordered immutable producer inputs; abort before unsafe normal reads. No writeback.
#include "gpu_producer_input_policy.h"
#include "gpu_producer_bundle_metal.h"

namespace SotcProducerInput {
inline std::atomic<U64> sequence{};
[[noreturn]] inline void Stop(const char* reason,const char* entry) {
    std::fprintf(stderr,"STOP_GPU_PRODUCER_INPUT: reason=%s entry=%s; selected producer NOT encoded; exit=23\n",reason,entry);
    std::fflush(stderr);std::_Exit(23);
}
inline const std::vector<Config>& Configs() {
    static const auto configs=[] {
        std::vector<Config> result;
        const char* text=std::getenv("SOTC_PRODUCER_INPUT_GUARD");
        if(text&&*text) {
            if(!Parse(text,result))Stop("invalid_layout","none");
            const char* dir=std::getenv("SOTC_SCATTER_GUARD_DIR");
            if(!dir||dir[0]!='/')Stop("absolute_evidence_directory_required","none");
            std::fprintf(stderr,"Producer input guard: entries=%zu; immutable parameters, indices and indirect args; ordered validation before normal reads; diagnostic timing\n",result.size());
        }
        return result;
    }();return configs;
}
class Scope {
    std::array<MetalFrame::Bound*,2> changed{};
    std::array<MetalFrame::Bound,2> original{};
public:
    id<MTLBuffer> args=nil;U64 args_offset{};
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
    ~Scope(){for(unsigned i=0;i<2;++i)if(changed[i])*changed[i]=original[i];}
    template<class Owned,class BeginBlit,class CommitChunk>
    Scope(Owned& o,id<MTLDevice> device,BeginBlit begin,CommitChunk commit,
          unsigned x,unsigned y,unsigned z,id<MTLBuffer> indirect=nil,U64 indirect_at=0)
        :args(indirect),args_offset(indirect_at) {
        if(Configs().empty()||!o.compute_pipeline||!o.compute_pipeline->shader)return;
        const auto* shader=o.compute_pipeline->shader;const Config* selected=nullptr;
        for(const auto& c:Configs())if(c.entry==shader->entry){selected=&c;break;}
        if(!selected)return;const auto& c=*selected;
        const auto stop=[&](const char* why){Stop(why,c.entry.c_str());};
        if(o.compute_pipeline->threads_per_group[0]!=c.threads||o.compute_pipeline->threads_per_group[1]!=1||
           o.compute_pipeline->threads_per_group[2]!=1||(!indirect&&(y!=1||z!=1)))stop("thread_layout");
        const std::array<unsigned,3> slots{c.params,c.indices,c.normals};
        const std::array<unsigned,3> push_at{c.params_adjust,c.indices_adjust,c.normals_adjust};
        std::array<MetalFrame::Bound*,3> refs{};std::array<MetalFrame::Bound,3> bound{};
        std::array<U64,3> adjust{};std::array<SotcBindingAudit::Record,3> audits{};
        for(unsigned i=0;i<3;++i) {
            for(const auto& b:shader->bindings)if(b.kind==Metal::ShaderBinding::Kind::Buffer&&b.msl_index==slots[i]) {
                auto it=o.compute_bindings.find((b.set<<16)|b.binding);
                if(refs[i]||b.count!=1||it==o.compute_bindings.end()||it->second.elements.size()!=1)stop("ambiguous_binding");
                refs[i]=&it->second.elements[0];
                if(b.set==0)audits[i]=SotcBindingAudit::Find(b.binding,reinterpret_cast<uintptr_t>((__bridge void*)refs[i]->buffer),refs[i]->offset,refs[i]->size);
            }
            if(push_at[i]>=o.push.size())stop("push_extent");
            adjust[i]=(U64(o.push[push_at[i]])>>2)*4;
            if(!refs[i]||!refs[i]->buffer||refs[i]->offset%4||refs[i]->size%4||adjust[i]>refs[i]->size||
               !ValidSpan(refs[i]->offset,refs[i]->size,refs[i]->buffer.length))stop("binding_extent");
            for(unsigned j=0;j<i;++j)if(refs[i]==refs[j])stop("aliased_roles");
            bound[i]=*refs[i];
        }
        if(bound[0].size>65536||!ValidSpan(adjust[0]+c.first_field,4,bound[0].size)||
           !ValidSpan(adjust[0]+c.count_field,4,bound[0].size)||
           (indirect&&(indirect_at%4||!ValidSpan(indirect_at,12,indirect.length))))stop("params_or_indirect_extent");
        const U64 params_at=512,index_at=(params_at+bound[0].size+255)&~U64(255);
        const U64 index_bytes=std::min<U64>(bound[1].size,8ull<<20);
        if(index_bytes<adjust[1])stop("index_adjust_extent");
        const U64 total=index_at+std::max<U64>(index_bytes,4);
        id<MTLBuffer> snapshot=[device newBufferWithLength:total options:MTLResourceStorageModeShared];
        if(!snapshot)stop("snapshot_allocation");std::memset(snapshot.contents,0,total);
        const std::uint32_t direct_dims[3]={x,y,z};std::memcpy(snapshot.contents,direct_dims,12);
        if(o.push.size()>256)stop("push_extent");
        std::memcpy(static_cast<unsigned char*>(snapshot.contents)+16,o.push.data(),o.push.size());
        auto blit=begin();
        [blit copyFromBuffer:bound[0].buffer sourceOffset:bound[0].offset toBuffer:snapshot destinationOffset:params_at size:bound[0].size];
        if(index_bytes)[blit copyFromBuffer:bound[1].buffer sourceOffset:bound[1].offset toBuffer:snapshot destinationOffset:index_at size:index_bytes];
        if(indirect)[blit copyFromBuffer:indirect sourceOffset:indirect_at toBuffer:snapshot destinationOffset:0 size:12];
        id<MTLCommandBuffer> copied=o.cmd;dispatch_semaphore_t done=dispatch_semaphore_create(0);
        [copied addCompletedHandler:^(id<MTLCommandBuffer>){dispatch_semaphore_signal(done);}];
        commit();
        if(dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,120ll*NSEC_PER_SEC)))stop("snapshot_timeout");
        if(copied.status!=MTLCommandBufferStatusCompleted)stop("snapshot_command_failed");
        const auto* data=static_cast<const unsigned char*>(snapshot.contents);
        std::uint32_t first{},count{},dims[3];
        std::memcpy(&first,data+params_at+adjust[0]+c.first_field,4);
        std::memcpy(&count,data+params_at+adjust[0]+c.count_field,4);std::memcpy(dims,data,12);
        if(dims[1]!=1||dims[2]!=1)stop("indirect_dimensions");
        const auto r=Inspect(data+index_at+adjust[1],(index_bytes-adjust[1])/4,(bound[1].size-adjust[1])/4,
            first,count,U64(dims[0])*c.threads,bound[2].size-adjust[2],c.stride,adjust[1]/4,adjust[2]/4);
        const U64 seq=++sequence;
        if(r.Bad()||seq<=4||seq%256==0)std::fprintf(stderr,"Producer input result: sequence=%llu entry=%s first=%u triangles=%llu checked=%llu invalid=%llu first_bad_word=%llu first_bad_index=%llu capacity=%llu input_short=%u truncated=%u arithmetic_wrap=%u; dispatch=%s\n",
            (unsigned long long)seq,c.entry.c_str(),first,(unsigned long long)r.triangles,(unsigned long long)r.checked,(unsigned long long)r.invalid,
            (unsigned long long)r.first_bad_word,(unsigned long long)r.first_bad_index,(unsigned long long)r.capacity,unsigned(r.input_short),unsigned(r.truncated),unsigned(r.arithmetic_wrap),r.Bad()?"BLOCKED":"validated_snapshot");
        if(r.Bad()) {
            std::ostringstream m;m<<"{\"schema\":1,\"entry\":"<<SotcFragmentProducer::Quote(c.entry)<<",\"sequence\":"<<seq
                <<",\"producer_encoded\":false,\"snapshot_completed\":true,\"groups_offset\":0,\"push_offset\":16,\"push_bytes\":"<<o.push.size()
                <<",\"params_offset\":"<<params_at<<",\"index_offset\":"<<index_at<<",\"index_bytes\":"<<index_bytes
                <<",\"first\":"<<first<<",\"count\":"<<count<<",\"triangles\":"<<r.triangles<<",\"checked\":"<<r.checked
                <<",\"invalid\":"<<r.invalid<<",\"capacity\":"<<r.capacity<<",\"stride\":"<<c.stride
                <<",\"input_short\":"<<(r.input_short?"true":"false")<<",\"truncated\":"<<(r.truncated?"true":"false")
                <<",\"arithmetic_wrap\":"<<(r.arithmetic_wrap?"true":"false")<<",\"bindings\":[";
            for(unsigned i=0;i<3;++i) {
                const U64 metal=reinterpret_cast<uintptr_t>((__bridge void*)bound[i].buffer);
                SotcFragment::Report(slots[i],metal,bound[i].offset,bound[i].size,i==0?data+params_at:i==1?data+index_at:nullptr,i==0?bound[0].size:i==1?index_bytes:0);
                SotcFragment::DumpHistory(slots[i],metal,bound[i].offset,bound[i].size);
                SotcBindingAudit::Sample(unsigned(seq),slots[i],0,audits[i]);
                if(i)m<<',';
                m<<"{\"slot\":"<<slots[i]<<",\"metal\":"<<metal<<",\"offset\":"<<bound[i].offset<<",\"bytes\":"<<bound[i].size<<",\"adjust\":"<<adjust[i]<<",\"binding_audit\":";
                SotcBindingAudit::Json(m,audits[i]);m<<'}';
            }
            m<<"],\"scope\":\"ordered pre-dispatch input snapshot; mapping history is not allocation ownership\"}\n";
            const std::string base=std::string(std::getenv("SOTC_SCATTER_GUARD_DIR"))+"/producer-input-"+std::to_string(seq);
            const auto meta=m.str();const bool bin=SotcScatterGuard::Save(base+".bin",data,total),json=SotcScatterGuard::Save(base+".json",meta.data(),meta.size());
            std::fprintf(stderr,"Producer input evidence: path=%s binary_saved=%u metadata_saved=%u\n",base.c_str(),unsigned(bin),unsigned(json));
            stop(bin&&json?"invalid_or_incomplete_input":"evidence_save_failed");
        }
        for(unsigned i=0;i<2;++i){changed[i]=refs[i];original[i]=*refs[i];refs[i]->buffer=snapshot;refs[i]->offset=i?index_at:params_at;refs[i]->size=i?index_bytes:bound[0].size;}
        if(indirect){args=snapshot;args_offset=0;}o.keep.push_back(snapshot);
    }
};
} // namespace SotcProducerInput
