"""Original fragmented-memory propagation fixtures; actual host and Metal scope.

CPU protocol doubles verify deferred copies, refresh, publication, stale mappings,
ABA rejection and no partial writes. --native additionally uses a tiny real Metal
compute dispatch, with synthetic bytes only; it does not validate game behavior.
"""
import ast
import os
from pathlib import Path
import subprocess
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[1]
tree = ast.parse((ROOT/'tools/check_scatter_guard_trial.py').read_text())
base = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
            and any(isinstance(t, ast.Name) and t.id == 'SOURCE' for t in n.targets))
SOURCE = base[:base.index('int main(')].replace('gpu_scatter_guard_metal.h', 'gpu_fragment_writeback_metal.h') + r'''
#include <pthread.h>
static constexpr uint64_t POOL_SIZE=1024;
static unsigned char storage[POOL_SIZE];
static unsigned char* backing_base=storage;
static uint64_t table_generation=42;
enum {KIND_RESERVED=1,KIND_DIRECT=2,KIND_FLEXIBLE=3};
struct Vma {uintptr_t start,end;int kind;uint64_t phys;};
static Vma vmas[4]={{0x10000,0x10030,KIND_DIRECT,64},{0x10030,0x10080,KIND_FLEXIBLE,256}};
static size_t vma_count=2;
static pthread_rwlock_t gate=PTHREAD_RWLOCK_INITIALIZER;
static unsigned writes=0;
static bool assert_no_writes=false;
static void read_lock(){assert(pthread_rwlock_rdlock(&gate)==0);}
static void read_unlock(){if(assert_no_writes)assert(writes==0);assert(pthread_rwlock_unlock(&gate)==0);}
static size_t vma_index(uintptr_t at){size_t i=0;while(i<vma_count&&vmas[i].end<=at)++i;return i;}
static void store_once(unsigned char* dst,const unsigned char* src,uint64_t n){++writes;std::memcpy(dst,src,n);}
extern "C" {
#include "fragment_writeback_runtime.h"
}
static void setup(){
    std::memset(storage,0xa5,sizeof(storage));
    for(unsigned i=0;i<128;++i)storage[i<48?64+i:256+i-48]=static_cast<unsigned char>(i);
}
static void verify(const unsigned char* output){
    assert(std::memcmp(storage+64,output,48)==0&&std::memcmp(storage+256,output+48,80)==0);
    assert(storage[63]==0xa5&&storage[112]==0xa5&&storage[255]==0xa5&&storage[336]==0xa5);
}
static void associate(const MetalFrame::Bound& bound,const std::string& mode){
    const uint64_t native=reinterpret_cast<uintptr_t>((__bridge void*)bound.buffer);
    SotcFragment::Get().Register(native,native,0);
    SotcFragment::Copy c;c.guest=0x10000;c.bytes=128;c.backend=native;c.offset=16;
    c.written=mode!="readonly";c.generation_before=42;c.generation_after=42;
    c.mapping.complete=true;c.mapping.count=2;
    c.mapping.segments[0]={0x10000,64,0x10030,true};c.mapping.segments[1]={0x10030,256,0x10080,true};
    if(mode=="unstable")c.generation_before=40;
    SotcFragment::Get().Add(c);
}
int main(int argc,char** argv) {@autoreleasepool {
    assert(argc==2);const std::string mode=argv[1];setup();
    if(mode=="host"){
        unsigned char data[128];std::memset(data,0xee,sizeof(data));
        assert(sotc_fragment_transfer_v1(0x10000,data,128,42,0));
        for(unsigned i=0;i<128;++i)assert(data[i]==i);
        data[0]=255;data[127]=254;assert(sotc_fragment_transfer_v1(0x10000,data,128,42,1));verify(data);
        assert(writes==2);writes=0;assert_no_writes=true;
        assert(!sotc_fragment_transfer_v1(0x10000,data,128,40,1));
        table_generation=44;assert(!sotc_fragment_transfer_v1(0x10000,data,128,42,1)); // ABA: same addresses, newer epoch.
        table_generation=42;vmas[1].kind=KIND_RESERVED;
        assert(!sotc_fragment_transfer_v1(0x10000,data,128,42,1)); // No write to valid first fragment.
        vmas[1].kind=KIND_DIRECT;vmas[1].phys=POOL_SIZE-1;
        assert(!sotc_fragment_transfer_v1(0x10000,data,128,42,1));
        assert(!sotc_fragment_transfer_v1(UINTPTR_MAX-8,data,128,42,1));
        assert(!sotc_fragment_transfer_v1(0x10000,data,0,42,1));
        assert(!sotc_fragment_transfer_v1(0x10000,nullptr,128,42,1));
        assert(!sotc_fragment_transfer_v1(0x10000,data,128,43,1));
        assert(!sotc_fragment_transfer_v1(0x10000,data,128,42,2));
        verify(data);return 0;
    }
    if(mode=="host_group"){
        vma_count=4;vmas[2]={0x20000,0x20040,KIND_DIRECT,512};vmas[3]={0x20040,0x20080,KIND_DIRECT,768};
        unsigned char a[128],b[128];std::memset(a,0x11,128);std::memset(b,0x22,128);
        SotcFragmentTransfer records[2]={{0x10000,a,128,42},{0x20000,b,128,42}};
        assert(sotc_fragment_transfer_batch_v2(records,2,1));assert(writes==4);
        assert(storage[64]==0x11&&storage[256]==0x11&&storage[512]==0x22&&storage[768]==0x22);
        writes=0;assert_no_writes=true;std::memset(a,0xee,128);std::memset(b,0xdd,128);
        vmas[3].kind=KIND_RESERVED;
        assert(!sotc_fragment_transfer_batch_v2(records,2,0));assert(a[0]==0xee&&b[0]==0xdd);
        assert(!sotc_fragment_transfer_batch_v2(records,2,1));assert(storage[64]==0x11&&storage[512]==0x22);
        vmas[3].kind=KIND_DIRECT;records[1].generation=44;
        assert(!sotc_fragment_transfer_batch_v2(records,2,1));records[1].generation=42;
        vmas[2].phys=64;assert(!sotc_fragment_transfer_batch_v2(records,2,1)); // Different guests, aliased physical output.
        vmas[2].phys=512;records[1].address=0x10000;assert(!sotc_fragment_transfer_batch_v2(records,2,1));
        assert(!sotc_fragment_transfer_batch_v2(records,3,1));assert(!sotc_fragment_transfer_batch_v2(records,0,1));
        assert(!sotc_fragment_transfer_batch_v2(nullptr,2,1));return 0;
    }
    if(mode=="native"){
        id<MTLDevice> device=MTLCreateSystemDefaultDevice();if(!device){std::puts("SKIP: no Metal device exposed to this process");return 77;}
        id<MTLCommandQueue> queue=[device newCommandQueue];assert(queue);
        NSError* error=nil;
        id<MTLLibrary> library=[device newLibraryWithSource:@"#include <metal_stdlib>\nusing namespace metal; kernel void propagate(device uint* p [[buffer(0)]], uint i [[thread_position_in_grid]]) { if(i<32) p[i]=i*7+11; }" options:nil error:&error];
        assert(library&&!error);
        id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"propagate"] error:&error];assert(pipeline&&!error);
        Shader shader;shader.bindings.push_back({Metal::ShaderBinding::Kind::Buffer,3,1,0,2});Pipeline metadata{&shader};
        Owned o;o.compute_pipeline=&metadata;o.cmd=[queue commandBuffer];
        auto buffer=[device newBufferWithLength:160 options:MTLResourceStorageModeShared];
        o.compute_bindings[2].elements.push_back({buffer,16,128});associate(o.compute_bindings[2].elements[0],mode);
        id<MTLBlitCommandEncoder> blit=nil;
        auto begin=[&]{if(!blit)blit=[o.cmd blitCommandEncoder];return blit;};
        auto commit=[&]{if(blit){[blit endEncoding];blit=nil;}[o.cmd commit];o.cmd=[queue commandBuffer];};
        SotcFragmentWriteback::Scope propagation(o,device,commit);
        const auto output=o.compute_bindings[2].elements[0];assert(output.buffer!=buffer);
        id<MTLComputeCommandEncoder> encoder=[o.cmd computeCommandEncoder];[encoder setComputePipelineState:pipeline];
        [encoder setBuffer:output.buffer offset:output.offset atIndex:0];
        [encoder dispatchThreads:MTLSizeMake(32,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];[encoder endEncoding];
        propagation.Finish(o,begin,commit);
        uint32_t expected[32];for(unsigned i=0;i<32;++i)expected[i]=i*7+11;
        verify(reinterpret_cast<unsigned char*>(expected));assert(std::memcmp((char*)buffer.contents+16,expected,128)==0);
        // Real ordered consumer gathers from the two shared backing fragments.
        id<MTLBuffer> pool=[device newBufferWithLength:POOL_SIZE options:MTLResourceStorageModeShared];
        std::memcpy(pool.contents,storage,POOL_SIZE);
        id<MTLBuffer> consumer=[device newBufferWithLength:128 options:MTLResourceStorageModeShared];
        auto gather=begin();[gather copyFromBuffer:pool sourceOffset:64 toBuffer:consumer destinationOffset:0 size:48];
        [gather copyFromBuffer:pool sourceOffset:256 toBuffer:consumer destinationOffset:48 size:80];
        SotcFragmentWriteback::Complete(o,commit);assert(std::memcmp(consumer.contents,expected,128)==0);
        std::puts("PASS: real Metal producer and fragmented consumer; synthetic only");return 0;
    }
    TestDevice* device=[TestDevice new];Shader shader;Pipeline pipeline{&shader};Owned o;o.compute_pipeline=&pipeline;
    if(mode=="unselected")shader.entry="fixture_suffix";
    if(mode=="third")shader.entry="third";
    shader.bindings.push_back({Metal::ShaderBinding::Kind::Buffer,3,1,0,2});
    auto original=[device newBufferWithLength:160 options:MTLResourceStorageModeShared];
    std::memset(original.contents,0xee,160);o.compute_bindings[2].elements.push_back({original,16,128});
    auto& output=o.compute_bindings[2].elements[0];
    if(mode!="direct")associate(output,mode);
    if(mode=="partial")output.size=132;
    if(mode=="extent")output.size=200;
    if(mode=="alias")o.compute_bindings[5].elements.push_back(output);
    if(mode=="duplicate")shader.bindings.push_back(shader.bindings[0]);
    TestCommand* command=[TestCommand new];TestBlit* blit=[TestBlit new];
    blit.command=command;o.cmd=(id<MTLCommandBuffer>)command;unsigned commits=0;
    auto commit=[&]{
        ++commits;[command complete];
        if(mode=="failed")command.status=MTLCommandBufferStatusError;
        if((mode=="before"&&commits==1)||(mode=="aba"&&commits==2)) {table_generation+=2;assert_no_writes=true;}
        command=[TestCommand new];blit.command=command;o.cmd=(id<MTLCommandBuffer>)command;
    };
    if(mode=="group"||mode=="group_bad_second"||mode=="group_reuse"||mode=="group_alias"||mode=="mixed"){
        vma_count=4;vmas[2]={0x20000,0x20040,KIND_DIRECT,512};vmas[3]={0x20040,0x20080,KIND_FLEXIBLE,768};
        for(unsigned i=0;i<128;++i)storage[i<64?512+i:768+i-64]=128+i;
        shader.bindings.push_back({Metal::ShaderBinding::Kind::Buffer,5,1,0,4});
        auto other=[device newBufferWithLength:160 options:MTLResourceStorageModeShared];
        std::memset(other.contents,0xee,160);o.compute_bindings[4].elements.push_back({other,16,128});
        auto& second=o.compute_bindings[4].elements[0];
        if(mode!="mixed"){
            const uint64_t native=reinterpret_cast<uintptr_t>((__bridge void*)other);
            SotcFragment::Get().Register(native,native,0);
            SotcFragment::Copy c;c.guest=0x20000;c.bytes=128;c.backend=native;c.offset=16;c.written=true;
            c.generation_before=c.generation_after=42;c.mapping.complete=true;
            SotcFragment::Get().Add(c);
        }
        if(mode=="group_bad_second")vmas[3].kind=KIND_RESERVED;
        if(mode=="group_alias")vmas[2].phys=64;
        SotcFragmentWriteback::Scope propagation(o,(id<MTLDevice>)device,commit);
        assert(commits==1&&output.buffer!=original);
        assert((mode=="mixed")==(second.buffer==other));
        auto* a=(unsigned char*)output.buffer.contents+output.offset;
        auto* b=(unsigned char*)second.buffer.contents+second.offset;
        assert(a[0]==0&&a[127]==127);
        if(mode!="mixed")assert(b[0]==128&&b[127]==255);
        [command.copies addObject:[^{a[0]=31;a[127]=32;b[0]=33;b[127]=34;} copy]];
        if(mode=="group_reuse"){table_generation=44;assert_no_writes=true;}
        propagation.Finish(o,[&]{return (id<MTLBlitCommandEncoder>)blit;},commit);
        assert(commits==2&&output.buffer==original&&second.buffer==other);
        assert(storage[64]==31&&storage[335]==32);
        if(mode!="mixed")assert(storage[512]==33&&storage[831]==34);
        else assert(storage[512]==128&&storage[831]==255);
        assert(((unsigned char*)original.contents)[16]==31&&((unsigned char*)other.contents)[16]==33);
        assert(((unsigned char*)original.contents)[15]==0xee&&((unsigned char*)other.contents)[144]==0xee);
        assert(writes==(mode=="mixed"?2:4));std::puts("GROUP_PROPAGATION_VERIFIED");return 0;
    }
    if(mode=="append"||mode=="counter_fragment"||mode=="counter_missing"){
        shader.bindings.push_back({Metal::ShaderBinding::Kind::Buffer,4,1,0,3});
        auto counter=[device newBufferWithLength:32 options:MTLResourceStorageModeShared];
        std::memset(counter.contents,0xa7,32);
        o.compute_bindings[3].elements.push_back({counter,8,16});
        auto* count=(uint32_t*)((char*)counter.contents+16);*count=0;
        if(mode=="counter_missing")o.compute_bindings.erase(3);
        if(mode=="counter_fragment")associate(o.compute_bindings[3].elements[0],mode);
        unsigned char expected[128];for(unsigned i=0;i<128;++i)expected[i]=i;
        // All destinations are prepared before dispatch, with intentionally stale bytes.
        std::vector<id<MTLBuffer>> prepared;
        for(unsigned step=0;step<9;++step){auto buffer=[device newBufferWithLength:160 options:MTLResourceStorageModeShared];std::memset(buffer.contents,0xee,160);prepared.push_back(buffer);}
        for(unsigned step=0;step<9;++step){
            shader.entry=step%2?"alternate":"fixture";
            output={prepared[step],16,128};associate(output,mode);
            SotcFragmentWriteback::Scope propagation(o,(id<MTLDevice>)device,commit);
            assert(output.buffer!=prepared[step]);
            auto* data=(unsigned char*)output.buffer.contents;
            assert(std::memcmp(data,expected,128)==0); // All prior append results survive refresh.
            assert(o.compute_bindings[3].elements[0].buffer==counter&&*count==step*3);
            [command.copies addObject:[^{
                const unsigned first=*count;*count+=3;
                for(unsigned j=0;j<3;++j){const uint32_t value=100+first+j;std::memcpy(data+4*(first+j),&value,4);}
            } copy]];
            propagation.Finish(o,[&]{return (id<MTLBlitCommandEncoder>)blit;},commit);
            for(unsigned j=0;j<3;++j){const uint32_t value=100+step*3+j;std::memcpy(expected+4*(step*3+j),&value,4);}
            assert(*count==(step+1)*3&&output.buffer==prepared[step]&&output.offset==16);
            verify(expected);assert(std::memcmp((char*)output.buffer.contents+16,expected,128)==0);
            assert(((unsigned char*)counter.contents)[15]==0xa7&&((unsigned char*)counter.contents)[20]==0xa7);
        }
        assert(commits==18&&writes==18&&*count==27);
        std::puts("APPEND_CHAIN_VERIFIED: nine preprepared destinations, two entries, live counter, preserved tail");return 0;
    }
    // Simulate a preceding GPU writer: refresh must happen only after this block.
    [command.copies addObject:[^{storage[64]=200;} copy]];
    if(mode=="lost_control"){
        commit();auto* stale=(unsigned char*)original.contents+16;
        stale[0]=255;stale[127]=254; // The old writable fallback never publishes these writes.
        assert(storage[64]==200&&storage[335]==127);
        assert(std::memcmp(storage+64,stale,48)!=0);std::puts("LOST_WRITE_REPRODUCED");return 0;
    }
    {
        SotcFragmentWriteback::Scope propagation(o,(id<MTLDevice>)device,commit);
        if(mode=="disabled"||mode=="unselected"||mode=="direct"){
            assert(!commits&&output.buffer==original);return 0;
        }
        assert(commits==1&&output.buffer!=original&&output.offset==0);
        auto* data=(unsigned char*)output.buffer.contents;
        assert(data[0]==200&&data[127]==127); // Fresh input, including untouched tail.
        [command.copies addObject:[^{data[0]=255;data[127]=254;} copy]];
        assert(storage[64]==200&&storage[335]==127);
        propagation.Finish(o,[&]{return (id<MTLBlitCommandEncoder>)blit;},commit);
        assert(commits==2&&output.buffer==original&&output.offset==16);
        unsigned char expected[128];for(unsigned i=0;i<128;++i)expected[i]=i;expected[0]=255;expected[127]=254;
        verify(expected);assert(std::memcmp((char*)original.contents+16,expected,128)==0);
        assert(((unsigned char*)original.contents)[15]==0xee&&((unsigned char*)original.contents)[144]==0xee);
        assert(writes==2);std::puts("PROPAGATION_VERIFIED");
    }
    return 0;
}}
'''

def main():
    with tempfile.TemporaryDirectory(dir=ROOT/'work') as tmp:
        p=Path(tmp);(p/'test.mm').write_text(SOURCE)
        subprocess.run(['clang++','-std=c++20','-fobjc-arc','-fblocks','-Wall','-Wextra','-Werror',
                        '-fsanitize=address,undefined','-framework','Foundation','-framework','Metal',
                        '-Wl,-export_dynamic','-I'+str(ROOT/'runtime/src'),str(p/'test.mm'),'-o',str(p/'test')],check=True)
        modes=('host','lost_control','propagate','disabled','unselected','direct','partial','extent','readonly','unstable','alias','duplicate','before','aba','failed','bad_config','append','counter_fragment','counter_missing','third','duplicate_profile','too_many','trailing_separator','host_group','group','group_bad_second','group_reuse','group_alias','mixed','duplicate_output','too_many_outputs')
        if '--native' in sys.argv:modes+=('native',)
        rejected={'partial','extent','readonly','unstable','alias','duplicate','before','aba','failed','bad_config','counter_fragment','counter_missing','duplicate_profile','too_many','trailing_separator','group_bad_second','group_reuse','group_alias','duplicate_output','too_many_outputs'}
        for mode in modes:
            env=dict(os.environ,SOTC_FRAGMENT_PROVENANCE='1',SOTC_FRAGMENT_WRITEBACK='fixture,3,1')
            if mode=='disabled':env.pop('SOTC_FRAGMENT_WRITEBACK')
            if mode=='bad_config':env['SOTC_FRAGMENT_WRITEBACK']='fixture,3,1,2,4'
            if mode in ('append','counter_fragment','counter_missing'):
                env['SOTC_FRAGMENT_WRITEBACK']='initializer,3,1;fixture,3,1,4;alternate,3,1,4'
            if mode=='third':env['SOTC_FRAGMENT_WRITEBACK']='initializer,3,1;alternate,3,1;third,3,1'
            if mode=='duplicate_profile':env['SOTC_FRAGMENT_WRITEBACK']='fixture,3,1;fixture,5,1'
            if mode=='too_many':env['SOTC_FRAGMENT_WRITEBACK']='a,3,1;b,3,1;c,3,1;d,3,1;e,3,1;fixture,3,1'
            if mode=='trailing_separator':env['SOTC_FRAGMENT_WRITEBACK']='fixture,3,1;'
            if mode in ('group','group_bad_second','group_reuse','group_alias','mixed'):
                env['SOTC_FRAGMENT_WRITEBACK']='fixture,3+5,1'
            if mode=='duplicate_output':env['SOTC_FRAGMENT_WRITEBACK']='fixture,3+3,1'
            if mode=='too_many_outputs':env['SOTC_FRAGMENT_WRITEBACK']='fixture,3+5+6,1'
            r=subprocess.run([str(p/'test'),mode],env=env,text=True,capture_output=True,timeout=45)
            if mode=='native' and r.returncode==77:
                print(r.stdout.strip(),flush=True);continue
            assert r.returncode==(23 if mode in rejected else 0),(mode,r.returncode,r.stdout,r.stderr)
            if mode in rejected:assert 'STOP_GPU_FRAGMENT_WRITEBACK' in r.stderr and 'PROPAGATION_VERIFIED' not in r.stdout
            print('PASS:',mode,flush=True)
    print('PASS: actual transfer and propagation scope, ASan/UBSan; native only when explicitly requested')

if __name__=='__main__':main()
