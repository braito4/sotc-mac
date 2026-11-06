#pragma once
#include "gpu_scatter_cached_guard.h"
#include "gpu_fragment_provenance.h"
namespace SotcFragment {
struct Allocator : SotcScatterCache::Allocator {
    void OnFailure(U64 slot,U64 metal,U64 offset,U64 bytes,const void* head,U64 head_bytes) {
        Report(slot,metal,offset,bytes,head,head_bytes);
        DumpHistory(static_cast<unsigned>(slot),metal,offset,bytes);
    }
};
class Scope : public SotcScatterGuard::Scope {
public:
    template<class Owned,class BeginBlit,class CommitChunk>
    Scope(Owned& o,id<MTLDevice> device,BeginBlit begin_blit,CommitChunk commit_chunk,
          unsigned x,unsigned y,unsigned z,id<MTLBuffer> indirect=nil,U64 indirect_offset=0)
        :SotcScatterGuard::Scope(o,device,begin_blit,commit_chunk,x,y,z,indirect,indirect_offset,Allocator{}) {}
};
}
