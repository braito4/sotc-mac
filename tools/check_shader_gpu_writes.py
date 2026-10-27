"""Exercise the production GPU-write entry with allocator checking disabled; no GPU."""
import os
from pathlib import Path
import subprocess
import tempfile
from build_gpu import ROOT,source_path
from check_buffer_copy_chunks import function


def main():
    tree=source_path(ROOT)
    source=(tree/'gpu/shim/bbport_free_check.cpp').read_text()
    start=source.index('void Check(u64 address, u64 size, const void* data, Source source, u64 seq) {')
    prefix=source[start:source.index('    StartWatcher();',start)]+'}\n'
    assert prefix.count('SotcShaderWriterBridge::Observe(')==1
    assert prefix.index('SotcShaderWriterBridge::Observe(')<prefix.index('if (!Enabled()')
    bind=function((tree/'gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp').read_text(),
                  'void Rasterizer::BindBuffers(')
    paged=bind[bind.index('} else if (Shader::IsPagedBuffer'):bind.index('} else {',bind.index('} else if (Shader::IsPagedBuffer'))]
    assert 'SotcShaderWriterBridge::Binding(vsharp.base_address,vsharp.GetSize(),' in paged
    assert 'stage.pgm_hash,binding.buffer,vsharp.GetSize(),true)' in paged
    assert paged.index('SotcShaderWriterBridge::Binding(')<paged.index('buffer_cache.LayerPagedRecord(')
    textures=function((tree/'gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp').read_text(),
                      'void Rasterizer::BindTextures(')
    assert 'SotcShaderWriterBridge::ImageBinding(desc.info.guest_address,desc.info.guest_size,' in textures
    assert textures.index('SotcShaderWriterBridge::ImageBinding(')<textures.index('if (!image_id)')
    assert 'SotcShaderWriterBridge::ResolvedImageBinding(image.info.guest_address,' in textures
    assert 'image.info.guest_address != desc.info.guest_address' in textures
    assert 'image.info.guest_size != desc.info.guest_size' in textures
    assert textures.index('auto& image = texture_cache.GetImage(image_id);')<textures.index('SotcShaderWriterBridge::ResolvedImageBinding(')
    targets=function((tree/'gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp').read_text(),
                     'void Rasterizer::PrepareRenderState(')
    assert targets.count('SotcShaderWriterBridge::TargetBinding(')==2
    compact=''.join(targets.split())
    assert 'image.info.guest_address,image.info.guest_size,fragment_hash,cb,' in compact
    assert 'image.info.guest_address,image.info.guest_size,fragment_hash,AmdGpu::NUM_COLOR_BUFFERS,' in compact
    fixture=r'''
#include <cassert>
#include <cerrno>
#include "bbport_free_check.h"
#include "gpu_shader_writer_bridge.h"
#include "gpu_memory_trace.h"
using u64=std::uint64_t;
static unsigned calls,expected_source,binding_calls,paged_binding_calls,image_binding_calls,target_binding_calls,resolved_image_binding_calls;
static std::uint64_t word=0x1234;
#if HOST_CALLBACK
extern "C" void sotc_shader_gpu_write(std::uint64_t address,const void* data,std::uint64_t size,
    unsigned source,std::uint64_t sequence,std::uintptr_t caller){
    assert(address==256&&data==&word&&size==8&&source==expected_source&&sequence==11+source&&caller);
    ++calls;errno=ENOENT;
}
extern "C" void sotc_shader_writable_binding(std::uint64_t address,std::uint64_t bytes,
    std::uint64_t shader,std::uint64_t slot,std::uint64_t declared){
    assert(address==256&&bytes==8&&shader==85&&slot==3&&declared==16);++binding_calls;errno=ENOENT;
}
extern "C" void sotc_shader_paged_writable_binding(std::uint64_t address,std::uint64_t bytes,
    std::uint64_t shader,std::uint64_t slot,std::uint64_t declared){
    assert(address==256&&bytes==8&&shader==85&&slot==4&&declared==17);++paged_binding_calls;errno=ENOENT;
}
extern "C" void sotc_shader_image_writable_binding(std::uint64_t address,std::uint64_t bytes,
    std::uint64_t shader,std::uint64_t slot,std::uint64_t declared){
    assert(address==256&&bytes==8&&shader==85&&slot==5&&declared==18);++image_binding_calls;errno=ENOENT;
}
extern "C" void sotc_shader_target_writable_binding(std::uint64_t address,std::uint64_t bytes,
    std::uint64_t shader,std::uint64_t slot,std::uint64_t declared){
    assert(address==256&&bytes==8&&shader==85&&slot==6&&declared==19);++target_binding_calls;errno=ENOENT;
}
extern "C" void sotc_shader_resolved_image_writable_binding(std::uint64_t address,std::uint64_t bytes,
    std::uint64_t shader,std::uint64_t slot,std::uint64_t declared){
    assert(address==256&&bytes==8&&shader==85&&slot==7&&declared==20);++resolved_image_binding_calls;errno=ENOENT;
}
#endif
namespace BbFreeCheck {
bool Enabled(){return false;}
@PREFIX@
}
int main(){
    const unsigned on=HOST_CALLBACK&&!std::strcmp(std::getenv("SOTC_SHADER_COPY_TRACE"),"1");
    for(unsigned source=0;source<11;++source){expected_source=source;errno=E2BIG;
        BbFreeCheck::Check(256,8,&word,static_cast<BbFreeCheck::Source>(source),11+source);
        assert(calls==on*(source+1)&&errno==E2BIG);
    }
    {SotcMemory::BindingScope binding({85,3,256,16,8,true,false});}
    assert(binding_calls==on&&errno==E2BIG&&SotcMemory::binding.address==0);
    SotcShaderWriterBridge::Binding(256,8,85,4,17,true);
    assert(paged_binding_calls==on&&errno==E2BIG);
    SotcShaderWriterBridge::ImageBinding(256,8,85,5,18);
    assert(image_binding_calls==on&&errno==E2BIG);
    SotcShaderWriterBridge::TargetBinding(256,8,85,6,19);
    assert(target_binding_calls==on&&errno==E2BIG);
    SotcShaderWriterBridge::ResolvedImageBinding(256,8,85,7,20);
    assert(resolved_image_binding_calls==on&&errno==E2BIG);
    {SotcMemory::BindingScope binding({85,3,256,16,8,false,false});}
    assert(binding_calls==on);
}
'''.replace('@PREFIX@',prefix)
    with tempfile.TemporaryDirectory(prefix='shader-gpu-write-',dir=ROOT/'work') as folder:
        p=Path(folder);cpp=p/'fixture.cpp';cpp.write_text(fixture)
        for exported in (0,1):
            binary=p/str(exported)
            subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-Wl,-export_dynamic',
                '-DHOST_CALLBACK='+str(exported),'-I'+str(ROOT/'runtime/src'),
                '-I'+str(source_path(ROOT)/'gpu/shim'),str(cpp),'-o',str(binary)],check=True)
            for setting in ('0','1'):
                subprocess.run([str(binary)],check=True,env=dict(os.environ,SOTC_SHADER_COPY_TRACE=setting))
    print('PASS: all 11 GPU-write source IDs reach the optional host bridge with original allocator checking disabled; absent/disabled controls and errno preserved; no GPU')


if __name__=='__main__':main()
