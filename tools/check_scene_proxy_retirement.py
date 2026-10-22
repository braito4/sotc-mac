"""Production proxy creation/retirement and scheduler deferral against CPU resource doubles."""
from pathlib import Path
import json
import os
import subprocess
import tempfile
from check_buffer_copy_chunks import function

ROOT = Path(__file__).resolve().parents[1]


def fixture(tree):
    base = tree/'gpu/shadps4/video_core/renderer_vulkan'
    source = (base/'vk_scene_resolution.cpp').read_text()
    header = (base/'vk_scene_resolution.h').read_text()
    adapted = 'void SceneTargets::CleanupStaleEntries()' in source
    get = function(source, 'SceneTargets::Entry& SceneTargets::Get(')
    cleanup = function(source, 'void SceneTargets::CleanupStaleEntries()') if adapted else \
        'void SceneTargets::CleanupStaleEntries() {}'
    entry = function(header, '    struct Entry {')+';'
    scheduler = (base/'vk_scheduler.h').read_text()
    defer = function(scheduler, '    void DeferOperationPastNext(')
    destruction = function((ROOT/'work/gpu-source/gpu/shadps4/video_core/texture_cache/image.cpp').read_text(),
                           'UniqueImage::~UniqueImage()')
    on_submit = function((base/'vk_rasterizer.cpp').read_text(), 'void Rasterizer::OnSubmit()')
    return r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "common/unique_function.h"
#include "video_core/renderer_vulkan/scene_resolution.h"
#include "gpu_memory_trace.h"
using u64=std::uint64_t;using u32=std::uint32_t;using VkImage=void*;
u64 next_image=1;unsigned destroyed=0,finishes=0;
std::unordered_map<u64,unsigned> views;
namespace vk {
struct Image {VkImage handle{};operator VkImage()const{return handle;}
    explicit operator bool()const{return handle!=nullptr;}};
struct Extent3D {u32 width=1920,height=1080,depth=1;};
struct ImageCreateInfo {void* pNext{};Extent3D extent{};u32 mipLevels=1;};
enum class ImageLayout {eUndefined};
struct View {u64 image;explicit View(u64 id):image{id}{++views[image];}
    ~View(){assert(views.at(image));--views[image];}};
using UniqueImageView=std::unique_ptr<View>;
}
namespace BbStats {std::atomic<u64> vk_image_bytes{0};}
namespace BbMetalResidency {void RemoveImage(vk::Image){}void ReleaseRetired(){}}
void vmaDestroyImage(void*,vk::Image image,void*){
    assert(views[reinterpret_cast<u64>(image.handle)]==0);++destroyed;
}
namespace VideoCore {
using ImageId=u64;struct ImageViewInfo {};
struct UniqueImage {
    void* allocator{};void* allocation{};vk::Image image{};vk::ImageCreateInfo image_ci{};u64 size_bytes{};
    UniqueImage()=default;UniqueImage(int,void*){}
    UniqueImage(const UniqueImage&)=delete;
    UniqueImage& operator=(UniqueImage&& other){assert(!image);image=std::exchange(other.image,{});return *this;}
    ~UniqueImage();
    void Create(vk::ImageCreateInfo ci){
        image_ci=ci;image={reinterpret_cast<VkImage>(next_image++)};
        size_bytes=u64(ci.extent.width)*ci.extent.height*4;BbStats::vk_image_bytes+=size_bytes;
        SotcMemory::ImageAllocated(reinterpret_cast<u64>(image.handle),size_bytes,
            {ci.extent.width,ci.extent.height,1,1,1,1,37,31,0});
    }
};
@DESTRUCTION@
struct Backing {struct {vk::ImageCreateInfo image_ci;} image;};
struct Image {u64 image_uid{};bool scene_proxy=false;Backing storage;Backing* backing=&storage;};
}
namespace Vulkan {
struct Instance {int GetDevice()const{return 0;}void* GetAllocator()const{return nullptr;}};
struct Scheduler {
    struct PendingOp {Common::UniqueFunction<void> callback;u64 gpu_tick;};
    std::queue<PendingOp> pending_ops;std::recursive_mutex pending_ops_mutex;
    std::atomic<u32> num_pending_ops{0};u64 current=10,completed=0;
    u64 CurrentTick()const{return current;}
    void Finish(){++finishes;}
    @DEFER@
    void Complete(u64 tick){completed=tick;
        while(!pending_ops.empty() && pending_ops.front().gpu_tick<=completed){
            pending_ops.front().callback();pending_ops.pop();--num_pending_ops;
        }
    }
};
struct SceneTargets {
    @ENTRY@
    Instance instance;Scheduler& scheduler;
    std::function<VideoCore::Image*(VideoCore::ImageId,u64)> lookup;
    std::unordered_map<u64,std::unique_ptr<Entry>> entries;std::unordered_set<u64> tracked;
    std::array<std::pair<u64,Entry*>,8> recent{};u32 recent_next=0;u64 generation=0;
    static u64 Key(u64 uid,u32 level){return uid<<4|level;}
    SceneResolution::Size ProxySize(const VideoCore::Image&,u32 level)const{
        return {640u>>level,360u>>level};
    }
    void Copy(Entry& e,VideoCore::Image&,bool native){assert(!native);e.state.CopiedToProxy();}
    Entry& Get(VideoCore::ImageId,u32=0);void CleanupStaleEntries();
};
@GET@
@CLEANUP@
struct Rasterizer {
    bool drained=false,fault_process_pending=false;SceneTargets* scene_targets;
    struct Cache {void ProcessFaultBuffer(){}void ProcessDownloadImages(){}void RunGarbageCollector(){}};
    struct Runtime {void TickFrame(){}} runtime;Cache buffer_cache,texture_cache;
    void DrainDrawPipe(){drained=true;}void OnSubmit();
};
@ON_SUBMIT@
}
int main(){
    constexpr bool adapted=@ADAPTED@;
    Vulkan::Scheduler scheduler;
    std::unordered_map<u64,VideoCore::Image> native;
    auto lookup=[&](u64 id,u64 uid)->VideoCore::Image*{
        const auto it=native.find(id);return it!=native.end() && (!uid || it->second.image_uid==uid)?&it->second:nullptr;
    };
    Vulkan::SceneTargets targets{{},scheduler,lookup,{},{},{},0,0};
    auto make=[&](u64 id,u64 uid,u32 level=0){
        native[id].image_uid=uid;auto& e=targets.Get(id,level);
        e.views.emplace_back(VideoCore::ImageViewInfo{},std::make_unique<vk::View>(reinterpret_cast<u64>(e.image.image.handle)));
        return &e;
    };
    auto* first=make(1,10);make(1,10,1);auto* live=make(2,20);
    live->state.ProxyWrite();const auto live_handle=live->image.image.handle;
    assert(&targets.Get(1)==first); // Exercise recent-entry hit before retiring it.
    targets.CleanupStaleEntries();assert(targets.generation==0 && destroyed==0);
    native[1].image_uid=11; // ImageId reused; old UID's two levels must retire.
    Vulkan::Rasterizer renderer{false,false,&targets,{},{},{}};
    renderer.OnSubmit();assert(renderer.drained);
    assert(targets.entries.size()==(adapted?1:3));
    assert(targets.generation==(adapted?1:0));
    if(adapted){
        assert(!targets.tracked.contains(10) && targets.tracked.contains(20));
        for(const auto& e:targets.recent)assert(!e.second);
        assert(scheduler.pending_ops.size()==1 && scheduler.pending_ops.front().gpu_tick==11);
        SotcMemory::Failure(0,0,0); // Retired images still count as live and pending.
    }
    make(1,11);assert(targets.Get(1).uid==11); // Old recent pointer cannot be reused.
    assert(targets.Get(2).image.image.handle==live_handle && live->state.dirty);
    targets.CleanupStaleEntries(); // Idempotent; does not queue the same owners twice.
    scheduler.Complete(10);assert(destroyed==0);
    scheduler.Complete(11);assert(destroyed==(adapted?2:0));scheduler.current=12;
    for(u64 i=0;i<200;++i){
        make(3,100+i);native.erase(3);targets.CleanupStaleEntries();
        scheduler.Complete(scheduler.current);assert(destroyed==(adapted?2+i:0));
        scheduler.Complete(scheduler.current+1);scheduler.current+=2;
    }
    assert(targets.entries.size()==(adapted?2:204));
    assert(destroyed==(adapted?202:0) && finishes==0);
    assert(targets.Get(2).image.image.handle==live_handle && live->state.dirty);
    SotcMemory::Failure(0,0,0);
    std::printf("active proxies=%zu, actually destroyed=%u; UID/mip/recent/view-order/past-next checks passed\n",
                targets.entries.size(),destroyed);
    return adapted?0:2;
}
'''.replace('@GET@',get).replace('@CLEANUP@',cleanup).replace('@ENTRY@',entry) \
       .replace('@DEFER@',defer).replace('@DESTRUCTION@',destruction) \
       .replace('@ON_SUBMIT@',on_submit).replace('@ADAPTED@','true' if adapted else 'false')


def main():
    with tempfile.TemporaryDirectory(prefix='scene-proxy-fixture-',dir=ROOT/'work') as temp:
        for name,tree,expected in (('pinned',ROOT/'runtime/upstream',2),('adapted',ROOT/'work/gpu-source',0)):
            root=Path(temp)/name;root.mkdir();cpp=root/'fixture.cpp';binary=root/'fixture'
            cpp.write_text(fixture(tree))
            subprocess.run(['clang++','-std=c++20','-O1','-Wall','-Wextra','-Werror','-pthread',
                            '-I'+str(ROOT/'runtime/src'),'-I'+str(ROOT/'work/gpu-source/gpu/shadps4'),
                            str(cpp),'-o',str(binary)],check=True)
            result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=15,
                env=dict(os.environ,SOTC_MEMORY_TRACE='1',BB_GPU_USER_DIR=str(root)))
            if result.returncode!=expected:
                raise RuntimeError(f'{name}: expected {expected}, got {result.returncode}: {result.stderr}')
            print(name+': '+result.stdout.strip())
            if name=='adapted':
                rows=[json.loads(line) for line in (root/'memory/summary.jsonl').read_text().splitlines()]
                first,last=rows
                assert first['role_count']==[0,3] and first['scene_proxy_pending_count']==2
                assert first['scene_proxy_destroyed_count']==0 and first['scene_proxy_pending_bytes']>0
                assert last['role_count']==[0,2] and last['scene_proxy_pending_count']==0
                assert last['scene_proxy_created_count']==204 and last['scene_proxy_destroyed_count']==202
                assert last['scene_proxy_created_bytes']-last['scene_proxy_destroyed_bytes']==last['live_bytes']
                assert last['observation_errors']==last['tracking_overflow']==0
    print('PASS: production proxy map stays bounded; deferred owners survive current tick; no GPU or guest input')


if __name__=='__main__':
    main()
