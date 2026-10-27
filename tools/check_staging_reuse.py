"""Actual staging allocation/ownership methods against CPU buffer and tick doubles."""
from pathlib import Path
from build_gpu import source_path, build_path
import re
import os
import subprocess
from check_buffer_copy_chunks import function

ROOT = Path(__file__).resolve().parents[1]


def fixture(tree):
    source = (tree/'gpu/shadps4/video_core/renderer_vulkan/vk_staging_buffer_pool.cpp').read_text()
    methods = '\n'.join(function(source, signature) for signature in (
        'u64 RoundAllocationSize(', 'StagingBufferRef StagingBufferPool::RequestLarge(',
        'void StagingBufferPool::FreeDeferred('))
    old = 'entry_size > max_size' in methods
    declaration = function(source, 'StagingBufferRef StagingBufferPool::RequestLarge(').split('{',1)[0]
    declaration = declaration.replace('StagingBufferPool::','').replace('u64 alignment)', 'u64 alignment=0)').strip()+';'
    return r'''
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>
#include "gpu_memory_trace.h"
using u64=std::uint64_t;using u32=std::uint32_t;using u8=std::uint8_t;
constexpr u64 operator""_KB(unsigned long long n){return n*1024;}
constexpr u64 MB=1024*1024,LARGE_MIN_GRANULARITY=64_KB;
#define ASSERT_MSG(condition, message) assert(condition)
namespace Common { u64 AlignUp(u64 value,u64 step){return (value+step-1)&~(step-1);} }
namespace SotcVisual { enum Counter {StagingNewBuffers,StagingReuses,StagingOversizeReuses};void Count(Counter){} }
struct Instance {u64 NonCoherentAtomSize()const{return 256;}};
struct BackendDevice { static const Instance& From(const Instance& i){return i;} };
namespace VideoCore {
enum class MemoryType {DeviceLocal,HostUncached,HostCached};
struct Buffer {
    u64 size;std::vector<u8> mapped_data{0};
    Buffer(const Instance&,int,u64 s,MemoryType,const char*):size(s){}
    u64 SizeBytes()const{return size;}
};
}
using MemoryType=VideoCore::MemoryType;
struct Scheduler {u64 current=10,completed=1;u64 CurrentTick()const{return current;}bool IsFree(u64 tick)const{return tick<=completed;}};
struct StagingBufferRef {VideoCore::Buffer* buffer{};u64 offset{},size{};u8* mapped{};MemoryType type{};};
struct StagingBufferPool {
    struct LargeBuffer {std::unique_ptr<VideoCore::Buffer> buffer;u64 tick,last_used_frame;bool held;u64 used_bytes=0;bool tail_open=false;};
    std::array<std::vector<LargeBuffer>,3> large_caches;
    Instance instance;Scheduler scheduler;u64 frame=0;mutable unsigned published=0;
    @REQUEST_DECLARATION@
    void FreeDeferred(const StagingBufferRef&);
    void PublishStats()const{++published;}
    VideoCore::Buffer* Seed(u64 size,MemoryType type,bool held=false,u64 tick=0) {
        auto& entry=large_caches[u32(type)].emplace_back(LargeBuffer{
            std::make_unique<VideoCore::Buffer>(instance,0,size,type,"synthetic"),tick,0,held});
        return entry.buffer.get();
    }
};
@METHODS@
int main(){
    const auto type=MemoryType::HostUncached;
    StagingBufferPool sequence;sequence.Seed(128*MB,type);
    for(u64 size:{64*MB,48*MB,32*MB,16*MB}){
        auto ref=sequence.RequestLarge(size,type,false);
        assert(ref.size==size && ref.offset==0 && ref.buffer->SizeBytes()>=size);
        assert(ref.mapped==ref.buffer->mapped_data.data());
        sequence.scheduler.completed=sequence.scheduler.current++;
    }
    u64 total=0;for(const auto& entry:sequence.large_caches[1])total+=entry.buffer->SizeBytes();
    constexpr bool old=@OLD@;
    assert(sequence.large_caches[1].size()==(old?5:1));
    assert(total==(old?288:128)*MB);
    StagingBufferPool lifetime;
    auto* held=lifetime.Seed(128*MB,type,true);
    auto* busy=lifetime.Seed(96*MB,type,false,100);
    auto* other=lifetime.Seed(64*MB,MemoryType::HostCached);
    auto ref=lifetime.RequestLarge(64*MB,type,false);
    assert(ref.buffer!=held && ref.buffer!=busy && ref.buffer!=other);
    auto again=lifetime.RequestLarge(64*MB,type,false);
    assert(again.buffer!=ref.buffer); // The selected tick has not completed.
    lifetime.FreeDeferred({.buffer=held,.type=type});
    auto after_release=lifetime.RequestLarge(100*MB,type,false);
    assert(after_release.buffer!=held); // Release still requires actual completion.
    lifetime.scheduler.completed=100;lifetime.scheduler.current=101;
    auto exact=lifetime.RequestLarge(96*MB,type,false);
    assert(exact.buffer==busy); // Smallest sufficient free buffer wins.
    StagingBufferPool deferred;
    auto a=deferred.RequestLarge(64*MB,type,true);
    deferred.scheduler.completed=deferred.scheduler.current++;
    auto b=deferred.RequestLarge(64*MB,type,true);
    assert(a.buffer!=b.buffer); // Completed but still held is not reusable.
    StagingBufferPool observations;observations.Seed(128*MB,type);
    for(unsigned i=0;i<768;++i){
        observations.RequestLarge(64*MB,type,false);
        observations.scheduler.completed=observations.scheduler.current++;
    }
    assert(old ? observations.published==0 : observations.published>=3);
    std::printf("sequential retained buffers=%zu bytes=%llu; held/pending/type/best-fit and repeated-reuse observation checks passed\n",
        sequence.large_caches[1].size(),(unsigned long long)total);
    return old?2:0;
}
'''.replace('@METHODS@', methods).replace('@OLD@', 'true' if old else 'false').replace('@REQUEST_DECLARATION@', declaration)


def main():
    work = ROOT/'work/staging-reuse-fixture';work.mkdir(exist_ok=True)
    for name, tree, expected in (('pinned', ROOT/'runtime/upstream', 2), ('adapted', source_path(ROOT), 0)):
        cpp, binary = work/(name+'.cpp'), work/name
        cpp.write_text(fixture(tree))
        subprocess.run(['clang++', '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror',
                        '-I'+str(ROOT/'runtime/src'),
                        str(cpp), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10,
                                env=dict(os.environ,SOTC_MEMORY_TRACE='1',
                                         BB_GPU_USER_DIR=str(work/(name+'-state'))))
        print(name+': '+result.stdout.strip())
        if result.returncode != expected:
            raise RuntimeError(f'{name}: expected {expected}, got {result.returncode}: {result.stderr}')
    print('PASS: retained oversize buffers reused with deferred/tick/type guards; no resource freed, no GPU initialized')


if __name__ == '__main__':
    main()
