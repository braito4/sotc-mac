"""Original CPU controls exercising extracted production alias methods; no game/GPU."""
from pathlib import Path
import subprocess
import tempfile
from build_texture_alias_trial import BASE, OUT, prepare
from check_buffer_copy_chunks import function

PRELUDE = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
using u32=uint32_t;using u64=uint64_t;using s32=int32_t;using VAddr=uint64_t;
namespace AmdGpu {
enum class ArrayMode {Linear, Macro, Array1DTiledThin1, Array1DTiledThick};
enum class ImageType {Color2D,Color3D};
unsigned GetMicroTileThickness(ArrayMode a){return a==ArrayMode::Array1DTiledThick?4:1;}
}
struct ImageInfo {
 struct {bool is_block=false;} props;
 struct {unsigned width=16,height=16,depth=1;} size;
 struct {int levels=1;unsigned layers=1;} resources;
 struct Mip {u32 size=0,pitch=0,height=0,offset=0;};
 std::array<Mip,4> mips_layout{};
 AmdGpu::ArrayMode array_mode=AmdGpu::ArrayMode::Macro;
 AmdGpu::ImageType type=AmdGpu::ImageType::Color2D;
 u32 micro_tiled_mips=0,pitch=16;u64 guest_address=0x10000,guest_size=0x1000;
 bool IsCompatible(const ImageInfo&)const{return true;}
 s32 MipOf(const ImageInfo&)const;s32 SliceOf(const ImageInfo&,s32)const;
};
'''
TEST = r'''
int main(){
 ImageInfo parent;parent.resources.layers=4;parent.mips_layout[0]={0x4000,16,16,0};
 ImageInfo sub=parent;sub.resources.layers=1;sub.guest_size=0x4000;sub.guest_address+=0x1000;
 assert(sub.SliceOf(parent,0)==1); // full backing stride, odd layer
 sub.resources.layers=2;sub.guest_size=0x2000;assert(sub.SliceOf(parent,0)==1);
 sub.guest_address=parent.guest_address+0x3000;assert(sub.SliceOf(parent,0)==-1);
 sub.resources.layers=1;sub.guest_size=0x1000;assert(sub.SliceOf(parent,0)==3);
 sub.guest_address++;assert(sub.SliceOf(parent,0)==-1);
 parent.resources.levels=2;parent.micro_tiled_mips=2;
 parent.mips_layout[1]={0x1000,8,8,0x4000};
 sub=parent;sub.resources.levels=1;sub.resources.layers=1;
 sub.size.width=sub.size.height=8;sub.pitch=8;sub.guest_address+=0x4400;sub.guest_size=0x1000;
 sub.array_mode=AmdGpu::ArrayMode::Array1DTiledThin1;
 assert(sub.MipOf(parent)==1);assert(sub.SliceOf(parent,1)==1);
 assert(sub.SliceOf(parent,-1)==-1);assert(sub.SliceOf(parent,2)==-1);
 parent.resources.layers=0;assert(sub.MipOf(parent)==-1);assert(sub.SliceOf(parent,1)==-1);
 parent.resources.layers=4;parent.mips_layout[1].size=0;
 assert(sub.MipOf(parent)==-1);assert(sub.SliceOf(parent,1)==-1);
 return 0;
}
'''
def main():
    prepare()
    with tempfile.TemporaryDirectory(prefix='alias-fixture-') as t:
        t=Path(t)
        for name,root in [('old',BASE),('new',OUT)]:
            source=(root/'gpu/shadps4/video_core/texture_cache/image_info.cpp').read_text()
            cpp=PRELUDE+'\n'+function(source,'s32 ImageInfo::MipOf(')+'\n'+function(source,'s32 ImageInfo::SliceOf(')+TEST
            p=t/(name+'.cpp');p.write_text(cpp);exe=t/name
            subprocess.run(['clang++','-std=c++23','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
            r=subprocess.run([str(exe)],capture_output=True,text=True)
            if name=='old':
                assert r.returncode!=0 and 'sub.SliceOf(parent,0)==1' in r.stderr,r.stderr
                print('PASS: old source reproduces wrong odd-layer rejection')
            else:
                assert r.returncode==0,r.stderr
                print('PASS: production mip/slice methods, odd layers, stride, bounds and invalid inputs')
        source=(OUT/'gpu/shadps4/video_core/texture_cache/texture_cache.cpp').read_text()
        start=source.index('    // Resolve against the final backing')
        end=source.index('    cached = FindImageCacheEntry{',start)
        block=source[start:end]
        hitstart=source.index('        if (cached.view_mip > 0)')
        hitend=source.index('        return cached.image_id;',hitstart)
        # Execute the real slow-path assignments and real cache-hit replay.
        extra=r'''
struct Desc {struct {struct {struct {int level=7,layer=9;} base;} range;} view_info;};
struct Cache {int view_mip=-1,view_slice=-1;};
void slow(const ImageInfo& info,const ImageInfo& final,Desc& desc,Cache& cached){
 struct {ImageInfo info;} image{final};int view_mip=3,view_slice=3;
'''+block+'cached.view_mip=view_mip;cached.view_slice=view_slice;}\nvoid hit(Desc& desc,Cache& cached){'+source[hitstart:hitend]+r'''}
int main(){
 ImageInfo parent;parent.resources.levels=2;parent.resources.layers=4;
 parent.mips_layout[0]={0x4000,16,16,0};parent.mips_layout[1]={0x1000,8,8,0x4000};
 ImageInfo sub=parent;sub.resources.levels=1;sub.resources.layers=1;
 sub.size.width=sub.size.height=8;sub.pitch=8;sub.guest_address+=0x4400;sub.guest_size=0x1000;
 Desc a,b;Cache c;slow(sub,parent,a,c);hit(b,c);
 assert(a.view_info.range.base.level==1&&a.view_info.range.base.layer==1);
 assert(b.view_info.range.base.level==1&&b.view_info.range.base.layer==1);
 sub=parent;sub.resources.levels=1;sub.resources.layers=1;sub.guest_size=0x1000;
 Desc d,e;slow(sub,parent,d,c);hit(e,c);
 assert(d.view_info.range.base.level==7&&d.view_info.range.base.layer==9);
 assert(e.view_info.range.base.level==7&&e.view_info.range.base.layer==9);
}
'''
        source=(OUT/'gpu/shadps4/video_core/texture_cache/image_info.cpp').read_text()
        p=t/'cache.cpp';p.write_text(PRELUDE+function(source,'s32 ImageInfo::MipOf(')+function(source,'s32 ImageInfo::SliceOf(')+extra)
        subprocess.run(['clang++','-std=c++23','-fsanitize=address,undefined',str(p),'-o',str(t/'cache')],check=True)
        subprocess.run([str(t/'cache')],check=True)
        print('PASS: final-backing offsets, cache-hit parity and descriptor base preservation')
if __name__=='__main__':main()
