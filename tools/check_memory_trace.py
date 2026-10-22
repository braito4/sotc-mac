"""Run production range/copy/staging/destruction methods against CPU-only resource doubles."""
from pathlib import Path
from build_gpu import source_path, build_path
import json
import os
import subprocess
import tempfile
from check_buffer_copy_chunks import function
from check_staging_reuse import fixture

ROOT = Path(__file__).resolve().parents[1]


def main():
    tree=source_path(ROOT)
    buffer=(tree/'gpu/shadps4/video_core/buffer_cache/buffer_cache.cpp').read_text()
    image=(tree/'gpu/shadps4/video_core/texture_cache/image.cpp').read_text()
    ranges='\n'.join(function(buffer,s) for s in (
        'std::optional<std::pair<const Buffer*, u64>> BufferCache::UnifiedRange(',
        'std::pair<const Buffer*, u64> BufferCache::UnifiedCopy('))
    destruction='\n'.join(function(image,s) for s in ('UniqueImage::~UniqueImage()', 'void UniqueImage::Destroy()'))
    prefix=fixture(tree).replace('int main(){','int staging_fixture_main(){')
    prefix=prefix.replace('struct Instance {','struct Instance {u64 StorageMinAlignment()const{return 256;}')
    prefix=prefix.replace('MemoryType type{};};','MemoryType type{};void Flush()const{}};')
    prefix=prefix.replace('    void FreeDeferred(',
        '    StagingBufferRef Request(u64 size,MemoryType type,u64 alignment){return RequestLarge(size,type,false,alignment);}\n'
        '    void FreeDeferred(')
    code=prefix+r'''
#include <optional>
using VAddr=u64;
constexpr u64 POOL_WINDOW_STEP=3072*MB;
bool mapping_valid=true;u64 mapping_physical=1024*MB,mapping_end=0;
bool runtime_memory_pool_phys(u64,u64* physical,std::uintptr_t* end){
    if(!mapping_valid)return false;*physical=mapping_physical;*end=mapping_end;return true;
}
struct Memory {u64 copied{};void CopySparseMemory(u64,unsigned char*,u64 size){copied=size;}};
namespace VideoCore {
struct BufferCache {
    Instance instance;Scheduler scheduler;StagingBufferPool staging_pool;Memory storage;Memory* memory=&storage;
    std::array<std::unique_ptr<Buffer>,2> pool_windows;unsigned page_tables=0;
    BufferCache(){for(auto& p:pool_windows)p=std::make_unique<Buffer>(instance,0,3584*MB,MemoryType::DeviceLocal,"fake");}
    void UnifiedPageTable(u64,u64){++page_tables;}
    std::size_t PoolWindow(u64 physical){return physical/POOL_WINDOW_STEP;}
    std::optional<std::pair<const Buffer*,u64>> UnifiedRange(VAddr,u64);
    std::pair<const Buffer*,u64> UnifiedCopy(VAddr,u64,bool);
};
@RANGE_METHODS@
}
using VkImage=void*;
namespace vk {struct Image {
    VkImage handle{};operator VkImage()const{return handle;}explicit operator bool()const{return handle!=nullptr;}
};}
namespace BbStats {std::atomic<u64> vk_image_bytes{8192};}
namespace BbMetalResidency {void RemoveImage(vk::Image){}}
unsigned destruction_calls=0;
void vmaDestroyImage(void*,vk::Image,void*){++destruction_calls;}
namespace VideoCore {
struct UniqueImage {
    void* allocator{};vk::Image image{};void* allocation{};u64 size_bytes{};
    ~UniqueImage();void Destroy();
};
@DESTRUCTION_METHODS@
}
int main(){
    constexpr u64 address=0x100000,size=3*1024*MB-32768;
    mapping_end=address+size;
    VideoCore::BufferCache cache;
    {
        SotcMemory::BindingScope context{{123,7,address,0xfffffff0,size,false,false}};
        assert(!cache.UnifiedRange(address,size));
        const auto copied=cache.UnifiedCopy(address,size,false);
        assert(copied.first->SizeBytes()==3*1024*MB && copied.second==0);
        assert(cache.storage.copied==size);
        auto direct=cache.UnifiedRange(address,128*MB);
        assert(direct && direct->second==mapping_physical);
        mapping_valid=false;
        assert(!cache.UnifiedRange(address,size));
    }
    SotcMemory::ImageAllocated(1,4096,{});
    SotcMemory::ImageOwner(1,100,address,4096,0,1,3);
    SotcMemory::Failure(0,0,0); // Still allocated despite queued deletion.
    {
        VideoCore::UniqueImage first{nullptr,{reinterpret_cast<VkImage>(1)},nullptr,4096};
        first.Destroy();
        assert(!first.image && destruction_calls==1);
    }
    assert(destruction_calls==1); // Explicit Destroy then destructor must not double free.
    SotcMemory::ImageAllocated(2,4096,{});
    {
        VideoCore::UniqueImage second{nullptr,{reinterpret_cast<VkImage>(2)},nullptr,4096};
    }
    assert(destruction_calls==2 && BbStats::vk_image_bytes==0);
    SotcMemory::Failure(8,111,222);
}
'''
    code=code.replace('@RANGE_METHODS@',ranges).replace('@DESTRUCTION_METHODS@',destruction)
    with tempfile.TemporaryDirectory(prefix='memory-source-fixture-',dir=ROOT/'work') as temp:
        root=Path(temp);cpp=root/'fixture.cpp';binary=root/'fixture';cpp.write_text(code)
        subprocess.run(['clang++','-std=c++20','-O1','-Wall','-Wextra','-Werror','-pthread',
                        '-I'+str(ROOT/'runtime/src'),str(cpp),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True,timeout=15,
                       env=dict(os.environ,SOTC_MEMORY_TRACE='1',BB_GPU_USER_DIR=str(root)))
        rows=lambda name:[json.loads(line) for line in (root/'memory'/name).read_text().splitlines()]
        buffers=rows('buffers.jsonl')
        assert [x['event'] for x in buffers]==['pool_window_cross','copy_begin','staging_allocate_begin',
            'staging_selected','copy_complete','pool_window_hit','pool_mapping_short']
        assert all(x['shader']==123 and x['slot']==7 for x in buffers)
        assert buffers[2]['size']==3*(1<<30)-32768 and buffers[2]['allocation']==3*(1<<30)
        assert buffers[3]['buffer']==buffers[4]['buffer'] and buffers[4]['elapsed_ns']>0
        summaries=rows('summary.jsonl')
        assert summaries[0]['live_bytes']==4096 and summaries[0]['state_bytes'][3]==4096
        assert summaries[-1]['live_bytes']==0 and summaries[-1]['destroyed_count']==2
        assert summaries[-1]['large_copy_requests']==summaries[-1]['large_copy_completions']==1
    print('PASS: actual UnifiedRange/UnifiedCopy/RequestLarge rounding and both VMA destruction paths; CPU metadata only, no GPU initialized')


if __name__=='__main__':
    main()
