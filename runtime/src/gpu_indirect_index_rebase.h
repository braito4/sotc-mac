#pragma once
// Original bounded GPU normalization of indexed indirect draws. No CPU argument readback.
#import <Metal/Metal.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <atomic>
#include <chrono>
#include "gpu_indirect_index_validation.h"

namespace SotcIndirectIndex {
constexpr std::uint64_t Capacity = 8ull << 20;
struct Parameters {
    std::uint64_t length, base, capacity;
    std::uint32_t width, selective;
};
static_assert(sizeof(Parameters)==32);
struct Workspace {
    id<MTLBuffer> indices, arguments, status;
    id<MTLCommandBuffer> status_command;
};
inline bool Selective() {
    static const bool enabled=[] {
        const char* value=std::getenv("SOTC_INDIRECT_INDEX_SELECTIVE");
        return value && std::strcmp(value,"1")==0;
    }();
    return enabled;
}
inline std::atomic<std::uint64_t> low_draws{}, copied_draws{}, copied_bytes{};
inline void Report() {
    static std::atomic<std::int64_t> last{};
    const auto now=std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    auto previous=last.load();
    if (now-previous<5 || !last.compare_exchange_strong(previous,now)) return;
    std::printf("Indirect index totals (completed GPU commands): bypass=%llu copied=%llu bytes=%llu selective=%u\n",
        (unsigned long long)low_draws.load(),(unsigned long long)copied_draws.load(),
        (unsigned long long)copied_bytes.load(),unsigned(Selective()));
}
[[noreturn]] inline void Fail(const char* reason) {
    std::fprintf(stderr,"STOP_GPU_INDIRECT_INDEX: %s\n",reason);
    std::fflush(stderr);
    std::_Exit(23);
}
inline NSString* Shader() {
    NSString* prefix=[@"#include <metal_stdlib>\nusing namespace metal;\n"
        stringByAppendingString:[NSString stringWithUTF8String:Validation]];
    return [prefix stringByAppendingString:@"\n"
        "struct Args { uint count, instances, first; int base_vertex; uint base_instance; };\n"
        "struct Params { ulong length, base, capacity; uint width, selective; };\n"
        "kernel void normalize_indices(device const uchar* src [[buffer(0)]],"
        " device const Args& in [[buffer(1)]], device uchar* dst [[buffer(2)]],"
        " device Args* out [[buffer(3)]], device atomic_uint* error [[buffer(4)]],"
        " constant Params& p [[buffer(5)]], uint lane [[thread_position_in_grid]]) {\n"
        " Args a=in; ulong advance=ulong(a.first)*p.width;\n"
        " ulong bytes=ulong(a.count)*p.width;\n"
        " uint problem=validate_indices(p.length,p.base,p.width,a.first,a.count,a.instances,~0ul);\n"
        " bool active=a.count!=0 && a.instances!=0;\n"
        " bool low=!problem && bypass_indices(p.base,p.width,a.first,a.count,a.instances,p.selective!=0);\n"
        " if (!problem && active && !low && bytes>p.capacity) problem=2;\n"
        " if (lane==0) {\n"
        "  out[0]=Args{0,0,0,0,0}; out[1]=Args{0,0,0,0,0};\n"
        "  if (problem) atomic_fetch_or_explicit(error,problem,memory_order_relaxed);\n"
        "  else if (low) { out[0]=a; if(active) atomic_fetch_add_explicit(error+1,1u,memory_order_relaxed); }\n"
        "  else { a.first=0; out[p.selective?1:0]=a; if(active) {"
        "   atomic_fetch_add_explicit(error+2,1u,memory_order_relaxed);"
        "   uint previous=atomic_fetch_add_explicit(error+3,uint(bytes),memory_order_relaxed);"
        "   if(previous>0xffffffffu-uint(bytes)) atomic_fetch_add_explicit(error+4,1u,memory_order_relaxed); } }\n"
        " }\n"
        " if (problem || !active || low) return;\n"
        " ulong at=p.base+advance;\n"
        " if (p.width==4) { device const uint* s=(device const uint*)(src+at); device uint* d=(device uint*)dst;"
        "  for(ulong i=lane;i<ulong(a.count);i+=256) d[i]=s[i]; }\n"
        " else { device const ushort* s=(device const ushort*)(src+at); device ushort* d=(device ushort*)dst;"
        "  for(ulong i=lane;i<ulong(a.count);i+=256) d[i]=s[i]; }\n"
        "}\n"];
}
inline id<MTLComputePipelineState> Pipeline(id<MTLDevice> device) {
    static std::mutex mutex;
    static id<MTLDevice> owner;
    static id<MTLComputePipelineState> pipeline;
    std::scoped_lock lock{mutex};
    if (pipeline && owner==device) return pipeline;
    NSError* error=nil;
    auto library=[device newLibraryWithSource:Shader() options:nil error:&error];
    if (!library) Fail(error.localizedDescription.UTF8String);
    pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"normalize_indices"] error:&error];
    if (!pipeline) Fail(error.localizedDescription.UTF8String);
    owner=device;
    return pipeline;
}
// The caller ends prior encoders and creates a serial compute encoder. It must end
// this encoder before drawing from the output. Both outputs are reused only in GPU order.
inline void Encode(id<MTLDevice> device, id<MTLCommandBuffer> command,
                   id<MTLComputeCommandEncoder> encoder, Workspace& work,
                   id<MTLBuffer> source, std::uint64_t base,
                   id<MTLBuffer> arguments, std::uint64_t argument_offset, unsigned width) {
    if (!source || !arguments || !command || !encoder || (width!=2 && width!=4) ||
        base>source.length || argument_offset%4 || argument_offset>arguments.length ||
        20>arguments.length-argument_offset) Fail("invalid buffer or indirect argument extent");
    if (!work.indices) {
        work.indices=[device newBufferWithLength:Capacity options:MTLResourceStorageModePrivate];
        work.arguments=[device newBufferWithLength:40 options:MTLResourceStorageModePrivate];
        if (!work.indices || !work.arguments) Fail("workspace allocation failed");
    }
    if (work.status_command!=command) {
        auto status=[device newBufferWithLength:20 options:MTLResourceStorageModeShared];
        if (!status) Fail("status allocation failed");
        std::memset(status.contents,0,20);
        work.status=status;
        work.status_command=command;
        [command addCompletedHandler:^(id<MTLCommandBuffer> completed) {
            // Existing renderer command-failure handling owns Metal execution errors.
            if (completed.status==MTLCommandBufferStatusCompleted) {
                std::uint32_t problem=0;
                std::memcpy(&problem,status.contents,4);
                if (problem) Fail(problem&1 ? "GPU index extent invalid" : "GPU index extent exceeds 8 MiB workspace");
                std::uint32_t counters[5]; std::memcpy(counters,status.contents,20);
                low_draws.fetch_add(counters[1]); copied_draws.fetch_add(counters[2]);
                copied_bytes.fetch_add((std::uint64_t(counters[4])<<32)|counters[3]);
            }
        }];
    }
    Report();
    const Parameters p{source.length,base,Capacity,width,unsigned(Selective())};
    [encoder setComputePipelineState:Pipeline(device)];
    [encoder setBuffer:source offset:0 atIndex:0];
    [encoder setBuffer:arguments offset:argument_offset atIndex:1];
    [encoder setBuffer:work.indices offset:0 atIndex:2];
    [encoder setBuffer:work.arguments offset:0 atIndex:3];
    [encoder setBuffer:work.status offset:0 atIndex:4];
    [encoder setBytes:&p length:sizeof(p) atIndex:5];
    [encoder dispatchThreads:MTLSizeMake(256,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];
}
inline void Draw(id<MTLRenderCommandEncoder> encoder, MTLPrimitiveType primitive,
                 MTLIndexType type, id<MTLBuffer> original, std::uint64_t base,
                 const Workspace& work) {
    if (Selective()) {
        [encoder drawIndexedPrimitives:primitive indexType:type indexBuffer:original
            indexBufferOffset:base indirectBuffer:work.arguments indirectBufferOffset:0];
    }
    [encoder drawIndexedPrimitives:primitive indexType:type indexBuffer:work.indices
        indexBufferOffset:0 indirectBuffer:work.arguments indirectBufferOffset:Selective()?20:0];
}
}
