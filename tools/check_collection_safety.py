"""Exercise the real collector predicate and SafeToDownload with CPU flag combinations."""
from pathlib import Path
import subprocess
import tempfile
from build_gpu import source_path
from check_buffer_copy_chunks import function

ROOT=Path(__file__).resolve().parents[1]
REL='gpu/shadps4/video_core/texture_cache/'


def fixture(cache,header):
    start=cache.index('        if ((tiled && download) || (download && !pressured)')
    end=cache.index(' {',start)
    condition=cache[start:end]
    declaration=''
    if 'gpu_contents_unresolved' in condition:
        begin=cache.rfind('        const bool gpu_contents_unresolved =',0,start)
        declaration=cache[begin:cache.index(';',begin)+1]
    method=function(header,'    bool SafeToDownload() const')
    return r'''
#include <cassert>
#include <cstdio>
enum ImageFlagBits:unsigned {MaybeCpuDirty=1,CpuDirty=2,GpuDirty=4,Dirty=7,GpuModified=8};
bool True(unsigned flags){return flags!=0;}bool False(unsigned flags){return flags==0;}
struct Image {unsigned flags;@METHOD@};
bool Keep(Image image,bool tiled,bool pressured){
    const bool download=image.SafeToDownload();
    @DECLARATION@
    @CONDITION@ return true;
    return false;
}
int main(){
    constexpr bool corrected=@CORRECTED@;
    assert(!Image{GpuModified|MaybeCpuDirty}.SafeToDownload());
    assert(Keep({GpuModified|MaybeCpuDirty},false,true)==corrected);
    assert(!Keep({GpuModified|CpuDirty},false,true)); // Real CPU overwrite can be evicted.
    assert(Keep({GpuModified},true,true)); // Unchanged tiled-download protection.
    assert(!Keep({GpuModified},false,true)); // Clean linear data can still download/evict under pressure.
    assert(Keep({GpuModified},false,false));
    unsigned retained=0;
    for(unsigned flags=0;flags<16;++flags)for(bool tiled:{false,true})for(bool pressure:{false,true}){
        const auto image=Image{flags};const bool download=image.SafeToDownload();
        const bool existing=(tiled&&download)||(download&&!pressure);
        const bool unresolved=(flags&GpuModified)&&!download&&!(flags&CpuDirty);
        assert(Keep(image,tiled,pressure)==(existing||(corrected&&unresolved)));
        retained+=Keep(image,tiled,pressure);
    }
    std::printf("retained combinations=%u; neighboring dirty page preserved=%d; real CPU overwrite remains evictable\n",retained,corrected);
    return corrected?0:2;
}
'''.replace('@METHOD@',method).replace('@DECLARATION@',declaration).replace('@CONDITION@',condition) \
       .replace('@CORRECTED@','true' if declaration else 'false').replace('#include <cassert>','#include <cassert>\n#include <initializer_list>')


def main():
    source=source_path(ROOT)
    old=subprocess.check_output(['git','-C',str(source),'show',
        '22923f08022909b7e22f7ae8c02ad4c3f6554bfd:'+REL+'texture_cache.cpp'],text=True)
    header=(source/REL/'image.h').read_text()
    with tempfile.TemporaryDirectory(prefix='collector-predicate-',dir=ROOT/'work') as directory:
        root=Path(directory)
        for name,text,expected in [('before',old,2),
            ('current-upstream',(ROOT/'runtime/upstream'/REL/'texture_cache.cpp').read_text(),0),
            ('adapted',(source/REL/'texture_cache.cpp').read_text(),0)]:
            cpp=root/(name+'.cpp');binary=root/name;cpp.write_text(fixture(text,header))
            subprocess.run(['clang++','-std=c++20','-O1','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
            result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=10)
            if result.returncode!=expected:
                raise RuntimeError(f'{name}: expected {expected}, got {result.returncode}: '+result.stderr)
            print(name+': '+result.stdout.strip())
    print('PASS: current upstream collector protects unresolved GPU contents; CPU-only, no image readback or eviction')


if __name__=='__main__':
    main()
