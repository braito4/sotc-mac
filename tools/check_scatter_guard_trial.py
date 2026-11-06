"""Exercise the actual guard with deferred Metal-protocol doubles, without a GPU.

The doubles implement buffer storage and completion ordering only. They do not
validate native Metal scheduling, barriers, driver behavior or game correctness.
"""
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
SOURCE=r'''
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <array>
#include <cassert>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
@interface TestBuffer : NSObject
@property(nonatomic,strong) NSMutableData* storage;
- (NSUInteger)length;
- (void*)contents;
@end
@implementation TestBuffer
- (NSUInteger)length {return self.storage.length;}
- (void*)contents {return self.storage.mutableBytes;}
@end
@interface TestDevice : NSObject
- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)n options:(MTLResourceOptions)options;
@end
@implementation TestDevice
- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)n options:(MTLResourceOptions)options {
    (void)options;TestBuffer* b=[TestBuffer new];b.storage=[NSMutableData dataWithLength:n];return (id<MTLBuffer>)b;
}
@end
@interface TestCommand : NSObject
@property(nonatomic) MTLCommandBufferStatus status;
@property(nonatomic,strong) NSMutableArray* handlers;
@property(nonatomic,strong) NSMutableArray* copies;
- (void)addCompletedHandler:(MTLCommandBufferHandler)handler;
- (void)complete;
@end
@implementation TestCommand
- (instancetype)init {self=[super init];if(self){_handlers=[NSMutableArray new];_copies=[NSMutableArray new];}return self;}
- (void)addCompletedHandler:(MTLCommandBufferHandler)handler {[self.handlers addObject:[handler copy]];}
- (void)complete {
    for(void (^copy)(void) in self.copies)copy();
    self.status=MTLCommandBufferStatusCompleted;
    for(MTLCommandBufferHandler h in self.handlers)h((id<MTLCommandBuffer>)self);
    [self.copies removeAllObjects];[self.handlers removeAllObjects];
}
@end
@interface TestBlit : NSObject
@property(nonatomic,strong) TestCommand* command;
- (void)copyFromBuffer:(id<MTLBuffer>)src sourceOffset:(NSUInteger)from toBuffer:(id<MTLBuffer>)dst destinationOffset:(NSUInteger)to size:(NSUInteger)size;
@end
@implementation TestBlit
- (void)copyFromBuffer:(id<MTLBuffer>)src sourceOffset:(NSUInteger)from toBuffer:(id<MTLBuffer>)dst destinationOffset:(NSUInteger)to size:(NSUInteger)size {
    assert(from<=src.length && size<=src.length-from && to<=dst.length && size<=dst.length-to);
    [self.command.copies addObject:[^{std::memcpy((char*)dst.contents+to,(char*)src.contents+from,size);} copy]];
}
@end
namespace Metal {
struct ShaderBinding {enum class Kind {Buffer};Kind kind=Kind::Buffer;unsigned msl_index{},count=1,set{},binding{};};
}
namespace MetalFrame {
struct Bound {id<MTLBuffer> buffer;std::uint64_t offset{},size{};};
}
struct Shader {std::string entry="fixture";std::vector<Metal::ShaderBinding> bindings;};
struct Pipeline {Shader* shader;std::array<unsigned,3> threads_per_group{64,1,1};};
struct Binding {std::vector<MetalFrame::Bound> elements;};
struct Owned {id<MTLCommandBuffer> cmd;Pipeline* compute_pipeline;std::array<unsigned char,256> push{};
    std::unordered_map<unsigned,Binding> compute_bindings;std::vector<id<MTLBuffer>> keep;};
#include "gpu_scatter_guard_metal.h"
int main(int argc,char** argv) {@autoreleasepool {
    assert(argc==2);const std::string mode=argv[1];
    const bool indirect=mode=="indirect" || mode=="truncated" || mode=="dimensions";
    TestDevice* device=[TestDevice new];
    Shader shader;Pipeline pipeline{&shader};Owned o;o.compute_pipeline=&pipeline;
    if (mode=="unselected")shader.entry="other";
    TestCommand* command=[TestCommand new];o.cmd=(id<MTLCommandBuffer>)command;
    TestBlit* blit=[TestBlit new];blit.command=command;
    std::array<unsigned,4> slots{1,2,10,13},adjust_at{80,81,89,92};
    std::array<std::uint64_t,4> sizes{32,32,68,100};
    if(mode=="truncated")sizes[1]=(8ull<<20)+16;
    for(unsigned i=0;i<4;++i){
        shader.bindings.push_back({Metal::ShaderBinding::Kind::Buffer,slots[i],1,0,slots[i]});
        auto buffer=[device newBufferWithLength:sizes[i]+16 options:MTLResourceStorageModeShared];
        o.compute_bindings[slots[i]].elements.push_back({buffer,16,sizes[i]});
        o.push[adjust_at[i]]=i==0?8:i==1?12:4;
    }
    auto& count=o.compute_bindings[1].elements[0];auto& index=o.compute_bindings[2].elements[0];
    id<MTLBuffer> count_original=count.buffer,index_original=index.buffer;
    auto* count_word=(std::uint32_t*)((char*)count.buffer.contents+count.offset+8+8);
    auto* index_words=(std::uint32_t*)((char*)index.buffer.contents+index.offset+12);
    *count_word=mode=="invalid"?3:mode=="truncated"?(8u<<20)/4+1:2;
    index_words[0]=0;index_words[1]=7;index_words[2]=8;
    if(mode=="truncated")std::memset(index_words,0,sizes[1]-12);
    auto args=[device newBufferWithLength:32 options:MTLResourceStorageModeShared];
    auto* dims=(std::uint32_t*)((char*)args.contents+4);dims[0]=mode=="truncated"?40000:1;dims[1]=mode=="dimensions"?2:1;dims[2]=1;
    bool committed=false;
    {
        SotcScatterGuard::Scope guard(o,(id<MTLDevice>)device,[&]{return (id<MTLBlitCommandEncoder>)blit;},[&]{
            committed=true;[command complete];
            // Concurrent guest reuse after the copy: dispatch must consume only its snapshot.
            *count_word=99;index_words[0]=UINT32_MAX;dims[0]=0;
            o.cmd=(id<MTLCommandBuffer>)[TestCommand new];
        },1,1,1,indirect?args:nil,4);
        if(mode=="unselected" || mode=="disabled"){
            assert(!committed && count.buffer==count_original && index.buffer==index_original);return 0;
        }
        assert(committed && count.buffer!=count_original && index.buffer!=index_original);
        assert(*(std::uint32_t*)((char*)count.buffer.contents+count.offset+16)==2);
        const auto* checked=(std::uint32_t*)((char*)index.buffer.contents+index.offset+12);
        assert(checked[0]==0 && checked[1]==7);
        if(indirect){assert(guard.args!=args && guard.args_offset==0);assert(*(std::uint32_t*)guard.args.contents==1);}
        assert(o.keep.size()==1 && o.keep[0]==index.buffer);
        std::puts("SIMULATED_DISPATCH_REACHED");
    }
    assert(count.buffer==count_original && index.buffer==index_original);
    assert(count.offset==16 && index.offset==16 && count.size==32 && index.size==32);
    assert(*count_word==99 && index_words[0]==UINT32_MAX); // No fabricated guest-memory repair.
    return 0;
}}
'''

with tempfile.TemporaryDirectory(dir=ROOT/'work') as tmp:
    directory=Path(tmp);source=directory/'test.mm';binary=directory/'test'
    source.write_text(SOURCE)
    subprocess.run(['clang++','-std=c++20','-fobjc-arc','-fblocks','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-framework','Foundation','-framework','Metal',
                    '-I'+str(ROOT/'runtime/src'),str(source),'-o',str(binary)],check=True)
    for mode in ('direct','indirect','unselected','disabled','invalid','truncated','dimensions'):
        evidence=directory/mode;evidence.mkdir()
        env=dict(os.environ,SOTC_SCATTER_GUARD='fixture,1,8,80,2,81,10,89,8,13,92,12,64',
                 SOTC_SCATTER_GUARD_DIR=str(evidence))
        if mode=='disabled':env.pop('SOTC_SCATTER_GUARD')
        result=subprocess.run([str(binary),mode],env=env,capture_output=True,text=True)
        expected=23 if mode in ('invalid','truncated','dimensions') else 0
        assert result.returncode==expected,(mode,result.returncode,result.stderr)
        if expected:assert 'SIMULATED_DISPATCH_REACHED' not in result.stdout
        if mode in ('invalid','truncated'):
            metadata=json.loads((evidence/'capture-1.json').read_text());data=(evidence/'capture-1.bin').read_bytes()
            count=struct.unpack_from('<I',data,metadata['count_offset'])[0]
            assert count==(3 if mode=='invalid' else (8<<20)//4+1)
            if mode=='invalid':assert struct.unpack_from('<III',data,metadata['index_offset'])==(0,7,8)
            assert 'binary_saved=1 metadata_saved=1' in result.stderr
        print('PASS:',mode)
print('PASS: actual guard, deferred-copy protocol doubles, input mutation, binding restoration and failure evidence (ASan/UBSan); no native GPU execution')
