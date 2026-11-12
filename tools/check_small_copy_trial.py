"""Exercise production transfer planning, kernel arithmetic and Metal encoding doubles.

Synthetic CPU-only fixtures: no device creation, shader compilation or game input.
The kernel body is translated to C++ only to compare byte results; that does not
validate MSL compilation or actual GPU ordering. Native validation remains pending.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CORE = r'''
#include "gpu_small_copy.h"
#include <cassert>
#include <random>
#include <cstring>
using namespace SotcSmallCopy;
struct Region { U64 srcOffset, dstOffset, size; };
int main() {
    Region r{0, 64, 16};
    auto run = [&](Source s, U64 base=0) {return Prepare(s,base,base+4096,base,base+4096,1,&r);};
    assert(run(Source::SameBuffer).reason==Reason::Ready);
    assert(run(Source::UnknownAlias).reason==Reason::Alias);
    assert(run(Source::FreshAllocation).reason==Reason::Ready);
    auto high=run(Source::SameBuffer, (1ull<<32)+4096);
    assert(high.rows[0].src==(1ull<<32)+4096 && high.rows[0].dst==(1ull<<32)+4160);
    assert(high.threads==1 && high.rows[0].width==16);
    r={4,68,4};assert(run(Source::SameBuffer).rows[0].width==4);
    r={1,65,3};assert(run(Source::SameBuffer).rows[0].width==1);
    r={0,8,16};assert(run(Source::SameBuffer).reason==Reason::Overlap);
    r={0,16,16};assert(run(Source::SameBuffer).reason==Reason::Ready); // touching, disjoint
    r={UINT64_MAX,0,1};assert(run(Source::SameBuffer).reason==Reason::Bounds);
    r={0,UINT64_MAX,1};assert(run(Source::Fill).reason==Reason::Bounds);
    r={0,0,16};assert(Prepare(Source::Fill,0,0,UINT64_MAX-7,UINT64_MAX,1,&r).reason==Reason::Bounds);
    assert(Prepare(Source::Fill,0,0,4097,4096,1,&r).reason==Reason::Bounds);
    r={0,0,65537};assert(Prepare(Source::Fill,0,0,0,131072,1,&r).reason==Reason::TooLarge);
    r.size=65536;assert(Prepare(Source::Fill,0,0,0,131072,1,&r).reason==Reason::Ready);
    assert(Prepare(Source::Fill,0,0,0,131072,1,&r,32).reason==Reason::TooLarge);
    assert(Prepare(Source::Fill,0,0,0,131072,1025,&r).reason==Reason::TooMany); // no OOB read
    r.size=0;assert(run(Source::Fill).reason==Reason::Empty);
    Region zero[]={{UINT64_MAX,UINT64_MAX,0},{4,68,4}};
    auto z=Prepare(Source::SameBuffer,0,128,0,128,2,zero);
    assert(z.reason==Reason::Ready && z.rows.size()==1 && z.threads==1);
    Region chained[]={{0,128,16},{128,256,16}};
    assert(Prepare(Source::SameBuffer,0,512,0,512,2,chained).reason==Reason::Overlap);
    Region two_writes[]={{0,128,16},{32,136,16}};
    assert(Prepare(Source::FreshAllocation,0,512,0,512,2,two_writes).reason==Reason::Overlap);
    std::vector<Region> many(1024);
    for (unsigned i=0;i<1024;++i) many[i]={i,2048+i,1};
    assert(Prepare(Source::SameBuffer,0,4096,0,4096,1024,many.data()).reason==Reason::Ready);
    // Independent pairwise oracle against production sorting/binary search.
    std::mt19937 random(20261109);
    for(unsigned trial=0;trial<4000;++trial) {
        std::vector<Region> regions(1+random()%20);
        for(auto& q:regions) q={random()%128,random()%128,1+random()%16};
        bool overlaps=false;
        auto hit=[](U64 a,U64 n,U64 b,U64 m){return a<b+m && b<a+n;};
        for(unsigned i=0;i<regions.size();++i) for(unsigned j=0;j<regions.size();++j) {
            const auto& a=regions[i];const auto& b=regions[j];
            overlaps|=hit(a.srcOffset,a.size,b.dstOffset,b.size);
            if(i!=j) overlaps|=hit(a.dstOffset,a.size,b.dstOffset,b.size);
        }
        auto plan=Prepare(Source::SameBuffer,0,256,0,256,regions.size(),regions.data());
        assert(plan.reason==(overlaps?Reason::Overlap:Reason::Ready));
    }
}
'''

METAL = r'''
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cassert>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <algorithm>
#include "gpu_small_copy_metal.h"
using u64=std::uint64_t;using u32=std::uint32_t;using u8=std::uint8_t;
using VkBuffer=std::uintptr_t;using VkDeviceSize=u64;
constexpr u64 VK_WHOLE_SIZE=UINT64_MAX;
struct VkBufferCopy {u64 srcOffset,dstOffset,size;};
static bool fail_pipeline=false, fail_table=false;
// The exact MSL kernel body with only Metal address-space/attribute syntax removed.
namespace KernelModel {
using uint=std::uint32_t;using ulong=std::uint64_t;using uchar=unsigned char;
struct uint4 {uint x,y,z,w;explicit uint4(uint v):x(v),y(v),z(v),w(v){};};
KERNEL_BODY
}
@interface Buffer : NSObject
@property(nonatomic,strong) NSMutableData* data;
- (NSUInteger)length;
- (void*)contents;
@end
@implementation Buffer
- (NSUInteger)length{return self.data.length;}
- (void*)contents{return self.data.mutableBytes;}
@end
@interface Pipeline : NSObject
@property(nonatomic,strong) id<MTLDevice> device;
- (NSUInteger)maxTotalThreadsPerThreadgroup;
@end
@implementation Pipeline
- (NSUInteger)maxTotalThreadsPerThreadgroup{return 256;}
@end
@interface Library : NSObject
- (id<MTLFunction>)newFunctionWithName:(NSString*)name;
@end
@implementation Library
- (id<MTLFunction>)newFunctionWithName:(NSString*)name {assert([name isEqualToString:@"sotc_buffer_vectors"]);return (id<MTLFunction>)[NSObject new];}
@end
@interface Device : NSObject
- (id<MTLBuffer>)newBufferWithBytes:(const void*)bytes length:(NSUInteger)n options:(MTLResourceOptions)options;
- (id<MTLLibrary>)newLibraryWithSource:(NSString*)source options:(MTLCompileOptions*)options error:(NSError**)error;
- (id<MTLComputePipelineState>)newComputePipelineStateWithFunction:(id<MTLFunction>)function error:(NSError**)error;
@end
@implementation Device
- (id<MTLBuffer>)newBufferWithBytes:(const void*)bytes length:(NSUInteger)n options:(MTLResourceOptions)options {
    (void)options;if(fail_table)return nil;
    Buffer* b=[Buffer new];b.data=[NSMutableData dataWithBytes:bytes length:n];return (id<MTLBuffer>)b;
}
- (id<MTLLibrary>)newLibraryWithSource:(NSString*)source options:(MTLCompileOptions*)options error:(NSError**)error {
    (void)options;(void)error;assert([source isEqualToString:[NSString stringWithUTF8String:SotcSmallCopy::Kernel]]);
    return fail_pipeline ? nil : (id<MTLLibrary>)[Library new];
}
- (id<MTLComputePipelineState>)newComputePipelineStateWithFunction:(id<MTLFunction>)function error:(NSError**)error {
    (void)function;(void)error;Pipeline* p=[Pipeline new];p.device=(id<MTLDevice>)self;return (id<MTLComputePipelineState>)p;
}
@end
@interface Encoder : NSObject {
@public id<MTLBuffer> buffers[3];std::vector<SotcSmallCopy::Row> rows;KernelModel::Args args;
}
@property(nonatomic) unsigned mutations;
@property(nonatomic) unsigned dispatches;
- (void)useResource:(id<MTLResource>)resource usage:(MTLResourceUsage)usage;
- (void)setComputePipelineState:(id<MTLComputePipelineState>)pipeline;
- (void)setBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;
- (void)setBytes:(const void*)bytes length:(NSUInteger)n atIndex:(NSUInteger)index;
- (void)dispatchThreads:(MTLSize)threads threadsPerThreadgroup:(MTLSize)group;
@end
@implementation Encoder
- (void)useResource:(id<MTLResource>)r usage:(MTLResourceUsage)u {(void)u;assert(r);++self.mutations;}
- (void)setComputePipelineState:(id<MTLComputePipelineState>)p {assert(p);++self.mutations;}
- (void)setBuffer:(id<MTLBuffer>)b offset:(NSUInteger)o atIndex:(NSUInteger)i {assert(b&&o==0&&i<3);buffers[i]=b;++self.mutations;}
- (void)setBytes:(const void*)b length:(NSUInteger)n atIndex:(NSUInteger)i {
    if(i==2){assert(n<=4096);rows.resize(n/sizeof(SotcSmallCopy::Row));std::memcpy(rows.data(),b,n);buffers[2]=nil;}
    else {assert(i==3&&n==sizeof(args));std::memcpy(&args,b,n);}++self.mutations;
}
- (void)dispatchThreads:(MTLSize)t threadsPerThreadgroup:(MTLSize)g {
    assert(g.width==256 && t.height==1 && t.depth==1);++self.dispatches;
    const auto* table=buffers[2] ? (const KernelModel::Region*)buffers[2].contents : (const KernelModel::Region*)rows.data();
    for(unsigned i=0;i<t.width;++i) KernelModel::sotc_buffer_vectors((const unsigned char*)buffers[0].contents,
        (unsigned char*)buffers[1].contents,table,args,i);
}
@end
@interface Blit : NSObject
@property(nonatomic) unsigned copies;
- (void)copyFromBuffer:(id<MTLBuffer>)src sourceOffset:(NSUInteger)from toBuffer:(id<MTLBuffer>)dst destinationOffset:(NSUInteger)to size:(NSUInteger)n;
- (void)fillBuffer:(id<MTLBuffer>)dst range:(NSRange)range value:(uint8_t)value;
@end
@implementation Blit
- (void)copyFromBuffer:(id<MTLBuffer>)s sourceOffset:(NSUInteger)f toBuffer:(id<MTLBuffer>)d destinationOffset:(NSUInteger)t size:(NSUInteger)n {
    assert(s&&d&&f+n<=s.length&&t+n<=d.length);std::memmove((char*)d.contents+t,(char*)s.contents+f,n);++self.copies;
}
- (void)fillBuffer:(id<MTLBuffer>)d range:(NSRange)r value:(uint8_t)v {assert(NSMaxRange(r)<=d.length);std::memset((char*)d.contents+r.location,v,r.length);++self.copies;}
@end
struct Owned {id<MTLComputeCommandEncoder> compute;id render;id<MTLBlitCommandEncoder> blit;
    std::vector<int> pending;std::vector<id<MTLBuffer>> keep;};
struct BufferRef {id<MTLBuffer> buffer;u64 offset;};
static std::unordered_map<VkBuffer,BufferRef> refs;
static id<MTLDevice> device;
static Blit* blit;
static unsigned ended=0;
BufferRef ResolveBuffer(VkBuffer b){return refs.at(b);}
id<MTLBlitCommandEncoder> BlitEncoder(Owned& o){o.compute=nil;o.render=nil;o.pending.clear();o.blit=(id<MTLBlitCommandEncoder>)blit;++ended;return o.blit;}
void Log(const char*){}
struct Event {std::string name;u64 from,to,bytes,value;id encoder;};
static std::vector<Event> events;
void TraceBufferBlit(Owned& o,const char* name,id<MTLBuffer>,u64 from,id<MTLBuffer>,u64 to,u64 bytes,u64 value=0,id encoder=nil){
    events.push_back({name,from,to,bytes,value,encoder?encoder:o.blit});
}
PRODUCTION_TRANSFERS
static id<MTLBuffer> buffer(NSUInteger n){Buffer* b=[Buffer new];b.data=[NSMutableData dataWithLength:n];return (id<MTLBuffer>)b;}
int main(int argc,char** argv){@autoreleasepool {
    assert(argc==2);std::string mode=argv[1];fail_pipeline=mode=="pipeline";
    const bool enabled=mode!="disabled";
    device=(id<MTLDevice>)[Device new];blit=[Blit new];auto b=buffer(16384);
    refs[1]={b,16};refs[2]={b,32};
    Encoder* encoder=[Encoder new];Owned o;o.compute=(id<MTLComputeCommandEncoder>)encoder;
    auto reset=[&]{o.compute=(id<MTLComputeCommandEncoder>)encoder;o.blit=nil;o.render=nil;o.pending.clear();events.clear();};
    auto* data=(unsigned char*)b.contents;for(unsigned i=0;i<b.length;++i)data[i]=i%251;
    VkBufferCopy r{0,128,64};std::array<unsigned char,64> expected;std::memcpy(expected.data(),data+16,64);
    CopyBuffer(o,1,2,1,&r);
    assert(!std::memcmp(data+160,expected.data(),64)&&events.size()==1);
    const bool active=enabled&&!fail_pipeline;
    assert(events[0].name==(active?"compute_copy_buffer":"blit_copy_buffer"));
    assert(encoder.dispatches==unsigned(active));
    assert(active ? ended==0&&events[0].encoder==encoder : ended==1&&encoder.mutations==0);
    if(!active)return 0;
    // Exact kernel body against independent byte oracle, including unaligned 1/4/16-byte lanes.
    for(unsigned shift=0;shift<17;++shift) {
        reset();VkBufferCopy rows[]={{shift,512+shift,31},{64,1024,4},{128,1536,16},{0,0,0}};
        std::vector<unsigned char> oracle(data,data+b.length);
        for(auto q:rows)std::memcpy(oracle.data()+32+q.dstOffset,data+16+q.srcOffset,q.size);
        CopyBuffer(o,1,2,4,rows);assert(!std::memcmp(oracle.data(),data,b.length));
        assert(events.size()==3&&o.compute==encoder&&events[0].encoder==encoder);
    }
    reset();FillBuffer(o,1,3,31,0x12121212);for(unsigned i=19;i<50;++i)assert(data[i]==0x12);
    assert(events.size()==1&&events[0].name=="compute_fill_buffer"&&events[0].value==((u64(0x12121212)<<32)|0x12));
    reset();FillBuffer(o,1,32,16,0x11223344);for(unsigned i=48;i<64;++i)assert(data[i]==0x44); // old low-byte semantics
    reset();FillBuffer(o,2,16000,VK_WHOLE_SIZE,0x77777777);for(unsigned i=16032;i<b.length;++i)assert(data[i]==0x77);
    reset();std::array<unsigned char,17> update;update.fill(0x91);UpdateBuffer(o,1,1,17,update.data());
    assert(!std::memcmp(data+17,update.data(),17)&&events[0].name=="compute_update_buffer"&&o.keep.size()==1);
    // Two Metal objects share the exact same backing: native identity alone must not admit.
    Buffer* alias=[Buffer new];alias.data=((Buffer*)b).data;refs[3]={(id<MTLBuffer>)alias,32};
    reset();unsigned dispatches=encoder.dispatches;CopyBuffer(o,1,3,1,&r);
    assert(encoder.dispatches==dispatches&&events[0].name=="blit_copy_buffer");
    reset();VkBufferCopy overlap[]={{0,256,16},{256,512,16}};CopyBuffer(o,1,1,2,overlap);
    assert(encoder.dispatches==dispatches&&events.size()==2&&events[0].name=="blit_copy_buffer");
    reset();o.pending.push_back(1);CopyBuffer(o,1,2,1,&r);assert(encoder.dispatches==dispatches);
    reset();o.render=[NSObject new];CopyBuffer(o,1,2,1,&r);assert(encoder.dispatches==dispatches);
    reset();o.compute=nil;CopyBuffer(o,1,2,1,&r);assert(encoder.dispatches==dispatches);
    // External row table is retained, and allocation failure must leave encoder state untouched.
    std::vector<VkBufferCopy> many(200);for(unsigned i=0;i<200;++i)many[i]={i,4096+i,1};
    reset();auto keeps=o.keep.size();CopyBuffer(o,1,1,many.size(),many.data());
    assert(o.keep.size()==keeps+1&&encoder.dispatches==dispatches+1&&events.size()==200);
    reset();unsigned mutations=encoder.mutations;fail_table=true;CopyBuffer(o,1,1,many.size(),many.data());fail_table=false;
    assert(encoder.mutations==mutations&&encoder.dispatches==dispatches+1&&events[0].name=="blit_copy_buffer");
    // Repeated calls do not lose retained staging/tables; game dispatch rebinds on its own path.
    reset();FillBuffer(o,1,1024,16,0xaaaaaaaa);assert(encoder.dispatches==dispatches+2);
}}
'''

def main():
    source = ROOT/'work/small-copy-trial-20261109/gpu/shadps4/video_core/renderer_metal/mtl_frame.mm'
    frame = source.read_text()
    transfers = frame[frame.index('void CopyBuffer(Owned&'):frame.index('void CopyImage(Owned&')]
    header = (ROOT/'runtime/src/gpu_small_copy_metal.h').read_text()
    kernel = header.split('R"msl(\n', 1)[1].split(')msl"', 1)[0]
    kernel = kernel.replace('#include <metal_stdlib>\nusing namespace metal;\n', '')
    kernel = re.sub(r'\[\[[^]]+\]\]', '', kernel)
    kernel = re.sub(r'\bconstant\b', 'const', kernel)
    kernel = re.sub(r'\b(device|kernel)\b', '', kernel)
    metal = METAL.replace('KERNEL_BODY', kernel).replace('PRODUCTION_TRANSFERS', transfers)
    with tempfile.TemporaryDirectory(prefix='small-copy-') as tmp:
        p = Path(tmp)
        common = ['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                  '-I'+str(ROOT/'runtime/src')]
        (p/'core.cpp').write_text(CORE)
        subprocess.run(common+[str(p/'core.cpp'),'-o',str(p/'core')],check=True)
        subprocess.run([str(p/'core')],check=True)
        print('PASS: bounds/overflow, >4GiB offsets, alias/overlap rejection, limits, zero rows and 4000 independent overlap-oracle cases')
        (p/'metal.mm').write_text(metal)
        subprocess.run(common+['-fobjc-arc','-fblocks','-framework','Foundation','-framework','Metal',
                              str(p/'metal.mm'),'-o',str(p/'metal')],check=True)
        for mode in ('enabled','disabled','pipeline'):
            subprocess.run([str(p/'metal'),mode],env=dict(os.environ,SOTC_SMALL_COPIES='0' if mode=='disabled' else '1'),check=True)
        print('PASS: actual Copy/Fill/Update and adapter; CPU-translated kernel byte oracle; shared alias fallback, transfer traces, staging/table lifetime, allocation/pipeline failure and opt-out')
        print('CPU doubles only; MSL compilation, GPU ordering, rendered pixels and FPS remain unvalidated.')

if __name__ == '__main__':
    main()
