#pragma once
#include "gpu_scatter_guard_metal.h"
#include <chrono>
#include <mutex>

namespace SotcScatterCache {
using SotcScatterProbe::U64;
inline U64 Now() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Idle bytes only. No buffers are allocated at startup. In-flight snapshots remain
// owned by their commands and cannot be checked out from this cache.
class Pool {
    struct Slot {id<MTLDevice> device;id<MTLBuffer> buffer;};
    std::mutex mutex;
    std::vector<Slot> idle;
    U64 budget,held{},hits{},misses{},returned{},discarded{},samples{},preflight_ns{};
public:
    explicit Pool(U64 bytes):budget(bytes) {}
    struct Stats {U64 held,hits,misses,returned,discarded,samples,preflight_ns;};
    Stats Read() {
        std::lock_guard lock(mutex);
        return {held,hits,misses,returned,discarded,samples,preflight_ns};
    }
    id<MTLBuffer> Acquire(id<MTLDevice> device,U64 bytes) {
        {
            std::lock_guard lock(mutex);
            auto best=idle.end();
            for (auto it=idle.begin();it!=idle.end();++it)
                if (it->device==device && it->buffer.length>=bytes &&
                    (best==idle.end() || it->buffer.length<best->buffer.length)) best=it;
            if (best!=idle.end()) {
                id<MTLBuffer> buffer=best->buffer;
                held-=buffer.length;idle.erase(best);++hits;
                return buffer;
            }
            ++misses;
        }
        // Exact lengths retain the guard's original allocation/copy size. A later
        // smaller capture can reuse the smallest adequate completed buffer.
        return [device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
    }
    void Return(id<MTLDevice> device,id<MTLBuffer> buffer,bool completed) {
        std::lock_guard lock(mutex);
        if (!completed || !buffer || buffer.length>budget || held>budget-buffer.length) {++discarded;return;}
        held+=buffer.length;idle.push_back({device,buffer});++returned;
    }
    void RetainUntil(id<MTLCommandBuffer> command,id<MTLDevice> device,id<MTLBuffer> buffer) {
        // Pool outlives all callbacks. Failed commands never make data available for reuse.
        Pool* pool=this;
        [command addCompletedHandler:^(id<MTLCommandBuffer> completed) {
            pool->Return(device,buffer,completed.status==MTLCommandBufferStatusCompleted);
        }];
    }
    void NotePreflight(U64 elapsed) {
        Stats s;
        {
            std::lock_guard lock(mutex);
            ++samples;preflight_ns+=elapsed;
            s={held,hits,misses,returned,discarded,samples,preflight_ns};
        }
        if (s.samples<=4 || s.samples%256==0)
            std::fprintf(stderr,"GPU scatter cache: validated=%llu hits=%llu allocations=%llu returned=%llu discarded=%llu idle_bytes=%llu budget_bytes=%llu preflight_avg_ms=%.3f; preflight includes copy completion wait and CPU validation, not GPU frame time\n",
                (unsigned long long)s.samples,(unsigned long long)s.hits,(unsigned long long)s.misses,
                (unsigned long long)s.returned,(unsigned long long)s.discarded,(unsigned long long)s.held,
                (unsigned long long)budget,double(s.preflight_ns)/double(s.samples)/1e6);
    }
};

inline Pool& Cache() {
    // Deliberately process-lifetime: completion handlers can run during host shutdown.
    static Pool* pool=[] {
        U64 mb=16;
        if (const char* value=std::getenv("SOTC_SCATTER_CACHE_MB")) {
            if (std::strcmp(value,"0")==0) mb=0;
            else if (std::strcmp(value,"16")==0) mb=16;
            else if (std::strcmp(value,"32")==0) mb=32;
            else if (std::strcmp(value,"64")==0) mb=64;
            else SotcScatterGuard::Stop("invalid_cache_budget","none");
        }
        std::fprintf(stderr,"GPU scatter cache: idle budget=%llu MiB; reuse only after consumer completion; strict guard waits unchanged\n",(unsigned long long)mb);
        return new Pool(mb<<20);
    }();
    return *pool;
}
struct Allocator : SotcScatterGuard::FreshSnapshot {
    U64 start{};
    id<MTLBuffer> Acquire(id<MTLDevice> device,U64 bytes) {
        start=Now();return Cache().Acquire(device,bytes);
    }
    void RetainUntil(id<MTLCommandBuffer> command,id<MTLDevice> device,id<MTLBuffer> buffer) {
        Cache().RetainUntil(command,device,buffer);
        Cache().NotePreflight(Now()-start);
    }
};
class Scope : public SotcScatterGuard::Scope {
public:
    template<class Owned,class BeginBlit,class CommitChunk>
    Scope(Owned& o,id<MTLDevice> device,BeginBlit begin_blit,CommitChunk commit_chunk,
          unsigned x,unsigned y,unsigned z,id<MTLBuffer> indirect=nil,U64 indirect_offset=0)
        :SotcScatterGuard::Scope(o,device,begin_blit,commit_chunk,x,y,z,indirect,indirect_offset,Allocator{}) {}
};
} // namespace SotcScatterCache
