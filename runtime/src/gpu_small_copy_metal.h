// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Compute kernel and encoding approach adapted from bloodborne-runtime a03929b5.
// Opt-in; preserves the existing serial encoder, command ownership and fallback.
#include "gpu_small_copy.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace SotcSmallCopy {
inline bool Enabled() {
    static const bool on = [] { const char* p = std::getenv("SOTC_SMALL_COPIES");
        return p && !std::strcmp(p, "1"); }();
    return on;
}
inline constexpr const char* Kernel = R"msl(
#include <metal_stdlib>
using namespace metal;
struct Region { ulong src, dst; uint end, width; };
struct Args { uint count, fill, copying; };
kernel void sotc_buffer_vectors(device const uchar* src [[buffer(0)]],
                                device uchar* dst [[buffer(1)]],
                                constant Region* rows [[buffer(2)]],
                                constant Args& a [[buffer(3)]], uint i [[thread_position_in_grid]]) {
    uint lo = 0, hi = a.count;
    while (lo < hi) {
        uint mid = lo + (hi - lo) / 2;
        if (i < rows[mid].end) hi = mid; else lo = mid + 1;
    }
    if (lo == a.count) return;
    Region row = rows[lo];
    uint start = lo ? rows[lo - 1].end : 0;
    ulong off = ulong(i - start) * row.width;
    uint pattern = a.fill * 0x01010101u;
    if (row.width == 16)
        *((device uint4*)(dst + row.dst + off)) = a.copying ?
            *((device const uint4*)(src + row.src + off)) : uint4(pattern);
    else if (row.width == 4)
        *((device uint*)(dst + row.dst + off)) = a.copying ?
            *((device const uint*)(src + row.src + off)) : pattern;
    else dst[row.dst + off] = a.copying ? src[row.src + off] : uchar(a.fill);
}
)msl";

struct Counters {
    std::atomic<U64> calls{}, encoded{}, bytes{}, state{}, alias{}, rejected{}, unavailable{};
};
inline Counters counters;
// Bounded-frequency aggregate logs show actual coverage, not an estimated FPS improvement.
struct Observation {
    U64 call = counters.calls.fetch_add(1, std::memory_order_relaxed) + 1;
    ~Observation() {
        if (call <= 8 || !(call % 4096))
            std::fprintf(stderr, "SOTC_SMALL_COPIES calls=%llu encoded=%llu bytes=%llu state=%llu alias=%llu rejected=%llu unavailable=%llu\n",
                (unsigned long long)call, (unsigned long long)counters.encoded.load(),
                (unsigned long long)counters.bytes.load(), (unsigned long long)counters.state.load(),
                (unsigned long long)counters.alias.load(), (unsigned long long)counters.rejected.load(),
                (unsigned long long)counters.unavailable.load());
    }
};

inline id<MTLComputePipelineState> Pipeline(id<MTLDevice> device) {
    static id<MTLComputePipelineState> pipeline;
    static std::once_flag once;
    std::call_once(once, [device] {
        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithSource:[NSString stringWithUTF8String:Kernel]
                                                    options:nil error:&error];
        id<MTLFunction> function = [library newFunctionWithName:@"sotc_buffer_vectors"];
        if (function) pipeline = [device newComputePipelineStateWithFunction:function error:&error];
        if (!pipeline) std::fprintf(stderr, "SOTC_SMALL_COPIES kernel unavailable: %s; keeping blits\n",
            error ? error.localizedDescription.UTF8String : "no pipeline");
    });
    // This renderer has one device; never bind a cached pipeline to another one.
    return pipeline.device == device ? pipeline : nil;
}

template<class Owned, class Region, class Trace>
bool Encode(Owned& o, id<MTLDevice> device, id<MTLBuffer> src, U64 src_base,
            id<MTLBuffer> dst, U64 dst_base, U32 count, const Region* regions,
            std::uint8_t fill, bool fresh_source, Trace trace) {
    if (!Enabled()) return false;
    Observation observation;
    if (!o.compute || o.render || o.blit || !o.pending.empty() || !dst) {
        ++counters.state; return false;
    }
    // Only the caller allocating staging immediately above may assert fresh_source.
    const Source source = !src ? Source::Fill : src == dst ? Source::SameBuffer :
                          fresh_source ? Source::FreshAllocation : Source::UnknownAlias;
    const Plan p = Prepare(source, src_base, src.length, dst_base, dst.length, count, regions);
    if (p.reason != Reason::Ready) {
        if (p.reason == Reason::Alias) ++counters.alias; else ++counters.rejected;
        return false;
    }
    id<MTLComputePipelineState> pipeline = Pipeline(device);
    if (!pipeline || !pipeline.maxTotalThreadsPerThreadgroup) { ++counters.unavailable; return false; }
    const auto bytes = p.rows.size() * sizeof(Row);
    id<MTLBuffer> table = nil;
    if (bytes > 4096) {
        table = [device newBufferWithBytes:p.rows.data() length:bytes options:MTLResourceStorageModeShared];
        if (!table) { ++counters.unavailable; return false; }
        o.keep.push_back(table);
    }
    // No encoder state changes before all fallible preparation has succeeded.
    if (src) [o.compute useResource:src usage:MTLResourceUsageRead];
    [o.compute useResource:dst usage:src == dst ? MTLResourceUsageRead | MTLResourceUsageWrite : MTLResourceUsageWrite];
    [o.compute setComputePipelineState:pipeline];
    [o.compute setBuffer:src ? src : dst offset:0 atIndex:0];
    [o.compute setBuffer:dst offset:0 atIndex:1];
    if (table) {
        [o.compute useResource:table usage:MTLResourceUsageRead];
        [o.compute setBuffer:table offset:0 atIndex:2];
    } else [o.compute setBytes:p.rows.data() length:bytes atIndex:2];
    struct { U32 count, fill, copying; } args{U32(p.rows.size()), fill, src != nil};
    [o.compute setBytes:&args length:sizeof(args) atIndex:3];
    // Record original transfer intent once, on the same command and before dispatch.
    for (U32 i = 0; i < count; ++i) if (regions[i].size) trace(regions[i]);
    [o.compute dispatchThreads:MTLSizeMake(p.threads, 1, 1)
         threadsPerThreadgroup:MTLSizeMake(std::min<NSUInteger>(256, pipeline.maxTotalThreadsPerThreadgroup), 1, 1)];
    ++counters.encoded;
    counters.bytes.fetch_add(p.bytes, std::memory_order_relaxed);
    return true;
}
} // namespace SotcSmallCopy
