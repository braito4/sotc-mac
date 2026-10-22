"""Exercise production frame retirement while submission queues stay nonempty."""
from pathlib import Path
import os
import subprocess
import tempfile
from build_gpu import source_path
from check_buffer_copy_chunks import function
from check_scene_proxy_retirement import fixture

ROOT = Path(__file__).resolve().parents[1]


def busy_fixture(tree, omit_busy_hook=False):
    source = (tree/'gpu/shadps4/video_core/amdgpu/liverpool.cpp').read_text()
    signal = function(source, 'void Liverpool::SignalDecodedFrames()')
    if omit_busy_hook:
        # Control retains production cleanup and ordinary OnSubmit, removing only
        # the two new busy-frame calls. This isolates scheduling from proxy cleanup.
        assert signal.count('MaintainCaches();')==2
        signal=signal.replace('                self.MaintainCaches();\n','')
        signal=signal.replace('    rasterizer->MaintainCaches();\n','')
    enabled = 'MaintainCaches()' in signal
    prefix = fixture(tree).split('int main(){', 1)[0]
    return prefix + r'''
namespace Libraries::GnmDriver {
u64 retired_frame=0;
void NoteFramesRetired(u64 frame){retired_frame=frame;}
}
namespace BbTimeline {constexpr u32 PipeTask=1;void Note(u32,u32,u64){}}
namespace BbToggle {constexpr u64 PipelinedTasks=1;}
namespace AmdGpu {
struct Liverpool {
    std::mutex submit_mutex;std::deque<std::pair<u64,u64>> frame_ends;
    u64 decoded_total=0;unsigned num_submits=1;Vulkan::Rasterizer* rasterizer;
    explicit Liverpool(Vulkan::Rasterizer* r):rasterizer(r){}
    void SignalDecodedFrames();
};
@SIGNAL@
}
int main(){
    constexpr bool enabled=@ENABLED@;
    for(bool pipelined:{false,true}){
        destroyed=0;finishes=0;Libraries::GnmDriver::retired_frame=0;
        Vulkan::Scheduler scheduler;
        std::unordered_map<u64,VideoCore::Image> native;
        auto lookup=[&](u64 id,u64 uid)->VideoCore::Image*{
            auto it=native.find(id);
            return it!=native.end() && (!uid || it->second.image_uid==uid)?&it->second:nullptr;
        };
        Vulkan::SceneTargets targets{{},scheduler,lookup,{},{},{},0,0};
        Vulkan::Rasterizer renderer{&targets};renderer.pipe_signals=pipelined;
        AmdGpu::Liverpool processor{&renderer};
        native[1].image_uid=1;auto& stable=targets.Get(1);stable.state.ProxyWrite();
        const auto stable_handle=stable.image.image.handle;
        renderer.OnSubmit();
        const unsigned initial_collections=renderer.texture_cache.collections;
        const unsigned initial_frames=renderer.runtime.frames;
        for(u64 frame=1;frame<=200;++frame){
            native[2].image_uid=frame+1;targets.Get(2);native.erase(2);
            // Advance the maintenance deadline without sleeping or changing GPU completion.
            renderer.last_cache_maintenance_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count()-251'000'000;
            processor.decoded_total=frame;processor.frame_ends.push_back({frame,frame});
            const u64 tick=scheduler.current;
            processor.SignalDecodedFrames();
            assert(processor.num_submits==1); // The idle/OnSubmit path is never taken.
            if(pipelined){
                assert(targets.entries.size()==(enabled?2:frame+1));
                if(enabled)assert(renderer.last_reads); // Cache access needs pending-read guards.
                renderer.RunQueued();
            }
            assert(renderer.drained);
            assert(targets.entries.size()==(enabled?1:frame+1));
            assert(destroyed==(enabled?frame-1:0)); // Queued ownership is still live.
            scheduler.Complete(tick);
            assert(Libraries::GnmDriver::retired_frame==frame); // No past-next FIFO deadlock.
            assert(destroyed==(enabled?frame-1:0));
            scheduler.Complete(tick+1);
            assert(destroyed==(enabled?frame:0));
            scheduler.current=tick+2;
            assert(targets.Get(1).image.image.handle==stable_handle && stable.state.dirty);
            // A second frame signal inside 250 ms must not run collection again.
            if(enabled){
                const auto collections=renderer.texture_cache.collections;
                renderer.MaintainCaches();
                assert(renderer.texture_cache.collections==collections);
            }
        }
        assert(renderer.runtime.frames==initial_frames); // Cache work is not a guest-frame tick.
        assert(renderer.texture_cache.collections==initial_collections+(enabled?200:0));
        const auto busy_active=targets.entries.size();
        const auto collections=renderer.texture_cache.collections;
        renderer.OnSubmit();assert(renderer.texture_cache.collections==collections+1);
        assert(finishes==0);
        std::printf("%s busy queue: active=%zu destroyed=%u collections=%u; frame signal precedes deferred release\n",
                    pipelined?"pipelined":"serial",busy_active,destroyed,
                    renderer.texture_cache.collections);
    }
    return enabled?0:2;
}
'''.replace('@SIGNAL@', signal).replace('@ENABLED@', 'true' if enabled else 'false')


def main():
    with tempfile.TemporaryDirectory(prefix='busy-cache-fixture-', dir=ROOT/'work') as temp:
        for name, tree, expected, omit in (('pinned', ROOT/'runtime/upstream', 2, False),
                                          ('cleanup-without-busy-hook', source_path(ROOT), 2, True),
                                          ('adapted', source_path(ROOT), 0, False)):
            root=Path(temp)/name;root.mkdir();cpp=root/'fixture.cpp';binary=root/'fixture'
            cpp.write_text(busy_fixture(tree, omit))
            subprocess.run(['clang++','-std=c++20','-O1','-Wall','-Wextra','-Werror','-pthread',
                            '-I'+str(ROOT/'runtime/src'),'-I'+str(source_path(ROOT)/'gpu/shadps4'),
                            str(cpp),'-o',str(binary)],check=True)
            result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=15,
                                  env=dict(os.environ,SOTC_MEMORY_TRACE='0'))
            if result.returncode!=expected:
                raise RuntimeError(f'{name}: expected {expected}, got {result.returncode}: {result.stderr}')
            print(name+': '+result.stdout.strip())
    print('PASS: busy queues collect on ordered frame boundaries; throttling and real completion preserved')


if __name__=='__main__':
    main()
