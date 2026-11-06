#pragma once
// Opt-in diagnostic containment, not a repair of index producers or page lifetimes.
// Included after mtl_frame.h; no shared renderer ABI changes.
#include "gpu_scatter_probe_policy.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

namespace SotcScatterGuard {
using namespace SotcScatterProbe;
inline std::atomic<U64> sequence{};
[[noreturn]] inline void Stop(const char* reason, const char* entry) {
    std::fprintf(stderr, "STOP_GPU_SCATTER_GUARD: reason=%s entry=%s; selected dispatch NOT encoded; exit=23\n", reason, entry);
    std::fflush(stderr);
    std::_Exit(23);
}
inline const std::vector<Config>& Configs() {
    static const auto configs=[] {
        std::vector<Config> result;
        const char* value=std::getenv("SOTC_SCATTER_GUARD");
        if (value && *value) {
            if (!Parse(value,result)) Stop("invalid_layout","none");
            const char* dir=std::getenv("SOTC_SCATTER_GUARD_DIR");
            if (!dir || dir[0]!='/') Stop("absolute_evidence_directory_required","none");
            for (const auto& c:result) {
                std::array<unsigned,4> slots{c.count_slot,c.index_slot,c.output_slot[0],c.output_slot[1]};
                std::sort(slots.begin(),slots.end());
                if (std::adjacent_find(slots.begin(),slots.end())!=slots.end()) Stop("aliased_roles",c.entry.c_str());
            }
            std::fprintf(stderr,"GPU scatter guard: enabled entries=%zu; GPU completion before validation; immutable count/index/indirect inputs; stop on invalid or incomplete coverage; diagnostic, not FPS comparison\n",result.size());
        }
        return result;
    }();
    return configs;
}
inline bool Save(const std::string& path, const void* bytes, U64 size) {
    const int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
    if (fd<0) return false;
    const auto* data=static_cast<const unsigned char*>(bytes);
    U64 done=0;
    while (done<size) {
        const auto written=write(fd,data+done,size-done);
        if (written<=0) { close(fd);return false; }
        done+=written;
    }
    const bool synced=fsync(fd)==0;
    return close(fd)==0 && synced;
}

// The default preserves the original trial's fresh-allocation behavior.
struct FreshSnapshot {
    id<MTLBuffer> Acquire(id<MTLDevice> device,U64 bytes) {
        return [device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    }
    void RetainUntil(id<MTLCommandBuffer>,id<MTLDevice>,id<MTLBuffer>) {}
    void OnFailure(U64,U64,U64,U64,const void*,U64) {}
};

class Scope {
    std::array<MetalFrame::Bound*,2> changed{};
    std::array<MetalFrame::Bound,2> original{};
public:
    id<MTLBuffer> args=nil;
    U64 args_offset{};
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
    ~Scope() {
        for (unsigned i=0;i<changed.size();++i) if (changed[i]) *changed[i]=original[i];
    }
    template<class Owned,class BeginBlit,class CommitChunk,class SnapshotStorage=FreshSnapshot>
    Scope(Owned& o,id<MTLDevice> device,BeginBlit begin_blit,CommitChunk commit_chunk,
          unsigned x,unsigned y,unsigned z,id<MTLBuffer> indirect=nil,U64 indirect_offset=0,
          SnapshotStorage storage={})
        :args(indirect),args_offset(indirect_offset) {
        if (Configs().empty() || !o.compute_pipeline || !o.compute_pipeline->shader) return;
        const auto* shader=o.compute_pipeline->shader;
        const Config* found=nullptr;
        for (const auto& c:Configs()) if (c.entry==shader->entry) {found=&c;break;}
        if (!found) return;
        const Config& c=*found;
        const auto stop=[&](const char* reason) {Stop(reason,c.entry.c_str());};
        if (o.compute_pipeline->threads_per_group[0]!=c.threads_x ||
            o.compute_pipeline->threads_per_group[1]!=1 || o.compute_pipeline->threads_per_group[2]!=1 ||
            (!indirect && (y!=1 || z!=1))) stop("thread_layout");
        const auto get=[&](unsigned slot)->MetalFrame::Bound* {
            for (const auto& b:shader->bindings)
                if (b.kind==Metal::ShaderBinding::Kind::Buffer && b.msl_index==slot && b.count==1) {
                    auto it=o.compute_bindings.find((b.set<<16)|b.binding);
                    if (it!=o.compute_bindings.end() && it->second.elements.size()==1) return &it->second.elements[0];
                }
            return nullptr;
        };
        std::array<MetalFrame::Bound*,4> refs{};
        std::array<MetalFrame::Bound,4> bound{};
        const std::array<unsigned,4> slots{c.count_slot,c.index_slot,c.output_slot[0],c.output_slot[1]};
        const std::array<unsigned,4> adjust_at{c.count_adjust,c.index_adjust,c.output_adjust[0],c.output_adjust[1]};
        std::array<U64,4> adjust{};
        for (unsigned i=0;i<4;++i) {
            refs[i]=get(slots[i]);adjust[i]=o.push[adjust_at[i]];
            if (!refs[i] || !refs[i]->buffer || adjust[i]%4 || adjust[i]>refs[i]->size ||
                refs[i]->offset%4 || !ValidSpan(refs[i]->offset,refs[i]->size,refs[i]->buffer.length)) stop("binding_extent_or_alignment");
            for (unsigned j=0;j<i;++j) if (refs[i]==refs[j]) stop("aliased_binding_roles");
            bound[i]=*refs[i];
        }
        if (!ValidSpan(c.count_field,4,bound[0].size-adjust[0]) || bound[0].size>65536 || bound[0].size%4 ||
            (indirect && (indirect_offset%4 || !ValidSpan(indirect_offset,12,indirect.length)))) stop("count_or_indirect_extent");
        constexpr U64 copy_limit=8ull<<20;
        const U64 capacity=(bound[1].size-adjust[1])/4;
        const U64 direct_threads=U64(x)*c.threads_x;
        const U64 copied=std::min<U64>({capacity,copy_limit/4,indirect?UINT64_MAX:direct_threads});
        // Preserve prefixes and push offsets. The shader sees the same logical count/index data.
        const U64 count_at=256, index_at=(count_at+bound[0].size+255)&~U64(255);
        const U64 index_bytes=adjust[1]+copied*4, bytes=index_at+std::max<U64>(index_bytes,4);
        id<MTLBuffer> snapshot=storage.Acquire(device,bytes);
        if (!snapshot) stop("snapshot_allocation");
        const U64 seq=++sequence;
        auto blit=begin_blit();
        [blit copyFromBuffer:bound[0].buffer sourceOffset:bound[0].offset toBuffer:snapshot destinationOffset:count_at size:bound[0].size];
        if (index_bytes) [blit copyFromBuffer:bound[1].buffer sourceOffset:bound[1].offset toBuffer:snapshot destinationOffset:index_at size:index_bytes];
        if (indirect) [blit copyFromBuffer:indirect sourceOffset:indirect_offset toBuffer:snapshot destinationOffset:0 size:12];
        id<MTLCommandBuffer> copied_command=o.cmd;
        dispatch_semaphore_t finished=dispatch_semaphore_create(0);
        [copied_command addCompletedHandler:^(id<MTLCommandBuffer>) {dispatch_semaphore_signal(finished);}];
        commit_chunk(); // Uses the renderer's normal admission, error reporting and keep-alive path.
        if (dispatch_semaphore_wait(finished,dispatch_time(DISPATCH_TIME_NOW,120ll*NSEC_PER_SEC))) stop("snapshot_timeout");
        if (copied_command.status!=MTLCommandBufferStatusCompleted) stop("snapshot_command_failed");
        const auto* data=static_cast<const unsigned char*>(snapshot.contents);
        std::uint32_t count{};std::memcpy(&count,data+count_at+adjust[0]+c.count_field,4);
        U64 threads=direct_threads;
        if (indirect) {
            std::uint32_t dims[3];std::memcpy(dims,data,12);
            if (dims[1]!=1 || dims[2]!=1) stop("indirect_dimensions");
            threads=U64(dims[0])*c.threads_x;
        }
        const auto r=Inspect(data+index_at+adjust[1],copied,count,threads,capacity,
                             {bound[2].size-adjust[2],bound[3].size-adjust[3]},c.stride);
        const bool bad=r.invalid || r.input_short || r.truncated;
        if (bad || seq<=4 || seq%256==0) {
            std::fprintf(stderr,"GPU scatter guard result: sequence=%llu entry=%s count=%u active=%llu checked=%llu invalid=%llu input_short=%u truncated=%u first_thread=%llu first_index=%llu; dispatch=%s\n",
                (unsigned long long)seq,c.entry.c_str(),count,(unsigned long long)r.active,(unsigned long long)r.checked,
                (unsigned long long)r.invalid,unsigned(r.input_short),unsigned(r.truncated),
                (unsigned long long)r.first_thread,(unsigned long long)r.first_index,bad?"BLOCKED":"validated_snapshot");
        }
        if (bad) {
            std::string meta="{\"schema\":1,\"entry\":\""+c.entry+"\",\"sequence\":"+std::to_string(seq)+
                ",\"count_offset\":"+std::to_string(count_at+adjust[0]+c.count_field)+
                ",\"index_offset\":"+std::to_string(index_at+adjust[1])+",\"copied_words\":"+std::to_string(copied)+
                ",\"threads\":"+std::to_string(threads)+",\"bindings\":[";
            for (unsigned i=0;i<4;++i) {
                const U64 identity=reinterpret_cast<std::uintptr_t>((__bridge void*)bound[i].buffer);
                storage.OnFailure(slots[i],identity,bound[i].offset+adjust[i],bound[i].size-adjust[i],
                    i==0?data+count_at+adjust[0]:i==1?data+index_at+adjust[1]:nullptr,
                    i==0?bound[0].size-adjust[0]:i==1?copied*4:0);
                std::fprintf(stderr,"  scatter guard binding: slot=%u metal=%#llx offset=%llu bytes=%llu adjust=%llu\n",
                    slots[i],(unsigned long long)identity,(unsigned long long)bound[i].offset,
                    (unsigned long long)bound[i].size,(unsigned long long)adjust[i]);
                if (i) meta+=",";
                meta+="{\"slot\":"+std::to_string(slots[i])+",\"metal\":"+std::to_string(identity)+
                    ",\"offset\":"+std::to_string(bound[i].offset)+",\"bytes\":"+std::to_string(bound[i].size)+
                    ",\"adjust\":"+std::to_string(adjust[i])+",\"stride\":"+std::to_string(i<2?4:c.stride[i-2])+"}";
            }
            meta+="]}\n";
            const std::string base=std::string(std::getenv("SOTC_SCATTER_GUARD_DIR"))+"/capture-"+std::to_string(seq);
            const bool bin_ok=Save(base+".bin",data,bytes), meta_ok=Save(base+".json",meta.data(),meta.size());
            std::fprintf(stderr,"GPU scatter evidence: path=%s binary_saved=%u metadata_saved=%u; snapshot only, original producer and allocation ownership unknown\n",base.c_str(),unsigned(bin_ok),unsigned(meta_ok));
            stop("invalid_or_incomplete_indices");
        }
        // Bind exactly the validated data. No second read of mutable guest input, no clamping.
        for (unsigned i=0;i<2;++i) {
            changed[i]=refs[i];original[i]=*refs[i];refs[i]->buffer=snapshot;
            refs[i]->offset=i?index_at:count_at;
            refs[i]->size=i?index_bytes:bound[0].size;
        }
        if (indirect) {args=snapshot;args_offset=0;}
        o.keep.push_back(snapshot);
        storage.RetainUntil(o.cmd,device,snapshot);
    }
};
} // namespace SotcScatterGuard
