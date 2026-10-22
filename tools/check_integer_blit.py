"""CPU contracts for actual Metal blit selection, guards and integer shader bodies.

Uses SDK enum definitions, but never creates a Metal device. MSL attributes and
vector constructors are translated mechanically for CPU texture doubles; this
is not a Metal shader compilation or pixel-rendering test.
"""
import json
from pathlib import Path
import re
import subprocess

from build_gpu import source_path
from check_buffer_copy_chunks import function

ROOT = Path(__file__).resolve().parents[1]
BASE = Path('gpu/shadps4/video_core/renderer_metal')


def fixture(tree):
    source = (tree / BASE / 'mtl_frame.mm').read_text()
    depth = function((tree / BASE / 'vk_to_mtl.mm').read_text(), 'bool IsDepthPixelFormat(')
    adapted = 'const char* BlitFragmentName(' in source
    if adapted:
        helpers = '\n'.join(function(source, signature) for signature in (
            'u32 BlitIntegerKind(', 'const char* BlitFragmentName(',
            'bool IntegerBlitSupported(', 'bool IntegerBlitRegionSupported('))
        body = function(source, 'void BlitImage(')
        # Wiring matters: passing helpers in isolation is insufficient.
        assert 'stringWithUTF8String:BlitFragmentName(format)' in source
        assert body.index('IntegerBlitSupported(') < body.index('BlitPipeline(')
        assert body.index('IntegerBlitRegionSupported(') < body.index('EndEncoders(o)')
        assert 'const u32 src_level = b.srcSubresource.mipLevel;' in body
        assert '[enc setFragmentBytes:&src_level length:sizeof src_level atIndex:0];' in body
        shader_literals = source[source.index('NSString* source =', source.index('BlitPipeline(')):]
        shader_literals = shader_literals[:shader_literals.index('library = [device')]
        assert 'BlitIntegerKind(format) ? blit_integer_library : blit_library' in source
        assert 'if (BlitIntegerKind(format)) {\n            source = [source stringByAppendingString:' in source
        shader = ''.join(json.loads(token) for token in re.findall(r'"(?:[^"\\]|\\.)*"', shader_literals))
        shaders = '\n'.join(function(shader, signature) for signature in (
            'fragment uint4 blit_uint_fs(', 'fragment int4 blit_sint_fs('))
        shaders = re.sub(r'\[\[.*?\]\]', '', shaders).replace('fragment ', '').replace('constant ', 'const ')
        shaders = re.sub(r'\b(uint2|int2|float2)\(', r'make_\1(', shaders)
        shaders = shaders.replace('access::read', 'TextureAccess::Read')
    else:
        # Evaluate the pinned selector itself, not a replacement model of it.
        pipeline = function(source, 'id<MTLRenderPipelineState> BlitPipeline(')
        expr = re.search(r'newFunctionWithName:(is_depth .*?)\];', pipeline).group(1).replace('@"', '"')
        helpers = ('const char* BlitFragmentName(MTLPixelFormat format) {\n'
                   'const bool is_depth = Metal::VkToMtl::IsDepthPixelFormat(u32(format));\n'
                   'return ' + expr + ';\n}')
        shaders = ''
    return r'''
#import <Metal/Metal.h>
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
using u32 = std::uint32_t;
namespace Metal::VkToMtl { @DEPTH@ }
@HELPERS@
#if @ADAPTED@
using uint = unsigned int;
using uint2 = uint __attribute__((ext_vector_type(2)));
using int2 = int __attribute__((ext_vector_type(2)));
using float2 = float __attribute__((ext_vector_type(2)));
using uint4 = uint __attribute__((ext_vector_type(4)));
using int4 = int __attribute__((ext_vector_type(4)));
uint2 make_uint2(uint a, uint b) { return {a,b}; }
uint2 make_uint2(int2 v) { return {uint(v.x),uint(v.y)}; }
int2 make_int2(int v) { return {v,v}; }
int2 make_int2(uint2 v) { return {int(v.x),int(v.y)}; }
int2 make_int2(float2 v) { return {int(v.x),int(v.y)}; }
float2 make_float2(uint2 v) { return {float(v.x),float(v.y)}; }
float2 floor(float2 v) { return {std::floor(v.x),std::floor(v.y)}; }
int2 clamp(int2 v, int2 lo, int2 hi) {
    return {std::max(lo.x,std::min(v.x,hi.x)),std::max(lo.y,std::min(v.y,hi.y))};
}
struct V { float2 uv; };
namespace TextureAccess { enum mode {Read}; }
template<class T> struct Vector;
template<> struct Vector<uint> {using type=uint4;};
template<> struct Vector<int> {using type=int4;};
template<class T, TextureAccess::mode> struct texture2d {
    using Texel=typename Vector<T>::type;
    Texel value;
    uint get_width(uint level)const {assert(level<3);return 4U>>level;}
    uint get_height(uint level)const {assert(level<3);return 4U>>level;}
    Texel read(uint2 p, uint level)const {
        assert(p.x<get_width(level)&&p.y<get_height(level));
        // Distinct levels and positions; exact first channels exercise integer bits.
        auto v=value;v[2]=T(level);v[3]=T(p.x+4*p.y);return v;
    }
};
@SHADERS@
void CheckGuards() {
    auto supported=[](MTLPixelFormat s,MTLPixelFormat d,MTLTextureType st=MTLTextureType2D,
                      MTLTextureType dt=MTLTextureType2D,NSUInteger ss=1,NSUInteger ds=1,
                      VkFilter f=VK_FILTER_NEAREST){return IntegerBlitSupported(s,d,st,dt,ss,ds,f);};
    assert(supported(MTLPixelFormatR32Uint,MTLPixelFormatR32Uint));
    assert(supported(MTLPixelFormatRG32Uint,MTLPixelFormatRG32Uint));
    assert(supported(MTLPixelFormatRGBA32Sint,MTLPixelFormatRGBA32Sint));
    assert(!supported(MTLPixelFormatR32Uint,MTLPixelFormatR32Sint));
    assert(!supported(MTLPixelFormatR32Uint,MTLPixelFormatR32Float));
    assert(!supported(MTLPixelFormatR32Float,MTLPixelFormatR32Uint));
    assert(!supported(MTLPixelFormatR32Uint,MTLPixelFormatR16Uint));
    assert(!supported(MTLPixelFormatR8Unorm,MTLPixelFormatR8Unorm));
    for(auto type:{MTLTextureType2DArray,MTLTextureType3D,MTLTextureTypeCube,MTLTextureType2DMultisample}) {
        assert(!supported(MTLPixelFormatR32Uint,MTLPixelFormatR32Uint,type));
        assert(!supported(MTLPixelFormatR32Uint,MTLPixelFormatR32Uint,MTLTextureType2D,type));
    }
    assert(!supported(MTLPixelFormatR32Uint,MTLPixelFormatR32Uint,MTLTextureType2D,MTLTextureType2D,2));
    assert(!supported(MTLPixelFormatR32Uint,MTLPixelFormatR32Uint,MTLTextureType2D,MTLTextureType2D,1,2));
    assert(!supported(MTLPixelFormatR32Uint,MTLPixelFormatR32Uint,MTLTextureType2D,MTLTextureType2D,1,1,VK_FILTER_LINEAR));
    VkImageBlit b{};
    b.srcSubresource=b.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,1,0,1};
    b.srcOffsets[1]=b.dstOffsets[1]={2,2,1};
    assert(IntegerBlitRegionSupported(b,2,2));
    assert(!IntegerBlitRegionSupported(b,1,2));
    assert(!IntegerBlitRegionSupported(b,2,1));
    for(int i=0;i<8;++i) {
        auto bad=b;
        switch(i) {
        case 0:bad.srcSubresource.layerCount=2;break;
        case 1:bad.dstSubresource.layerCount=2;break;
        case 2:bad.srcSubresource.baseArrayLayer=1;break;
        case 3:bad.dstSubresource.baseArrayLayer=1;break;
        case 4:bad.srcSubresource.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;break;
        case 5:bad.dstSubresource.aspectMask=VK_IMAGE_ASPECT_STENCIL_BIT;break;
        case 6:bad.srcOffsets[1].z=2;break;
        case 7:bad.dstOffsets[0].z=1;break;
        }
        assert(!IntegerBlitRegionSupported(bad,2,2));
    }
}
void CheckValues() {
    texture2d<uint,TextureAccess::Read> u{{0x01000001U,0xffffffffU,0,0}};
    texture2d<int,TextureAccess::Read> s{{std::numeric_limits<int>::min(),-16777217,0,0}};
    // Independent nearest-coordinate oracle, including minified mip and edge clamping.
    const float coords[]={-0.125f,0,0.125f,0.375f,0.625f,0.875f,1,1.125f};
    for(uint level=0;level<3;++level)for(float x:coords)for(float y:coords) {
        auto a=blit_uint_fs({{x,y}},u,level);auto b=blit_sint_fs({{x,y}},s,level);
        const int size=4>>level;
        const int ix=std::max(0,std::min(size-1,int(std::floor(x*size))));
        const int iy=std::max(0,std::min(size-1,int(std::floor(y*size))));
        assert(a[0]==0x01000001U&&a[1]==0xffffffffU&&a[2]==level&&a[3]==uint(ix+4*iy));
        assert(b[0]==std::numeric_limits<int>::min()&&b[1]==-16777217&&b[2]==int(level)&&b[3]==ix+4*iy);
    }
    // A float-valued path cannot satisfy the exact integer data contract.
    assert(double(float(u.value[0]))!=double(u.value[0]));
    assert(double(float(s.value[1]))!=double(s.value[1]));
}
#endif
int main() {
    bool types=std::strcmp(BlitFragmentName(MTLPixelFormatR32Uint),"blit_uint_fs")==0 &&
               std::strcmp(BlitFragmentName(MTLPixelFormatRG32Uint),"blit_uint_fs")==0 &&
               std::strcmp(BlitFragmentName(MTLPixelFormatR32Sint),"blit_sint_fs")==0;
    assert(std::strcmp(BlitFragmentName(MTLPixelFormatRGBA8Unorm),"blit_fs")==0);
    assert(std::strcmp(BlitFragmentName(MTLPixelFormatDepth32Float),"blit_depth_fs")==0);
#if @ADAPTED@
    CheckGuards();CheckValues();
    for(auto f:{MTLPixelFormatR8Uint,MTLPixelFormatRG16Uint,MTLPixelFormatRGBA32Uint,MTLPixelFormatRGB10A2Uint})
        assert(std::strcmp(BlitFragmentName(f),"blit_uint_fs")==0);
    for(auto f:{MTLPixelFormatR8Sint,MTLPixelFormatRG16Sint,MTLPixelFormatRGBA32Sint})
        assert(std::strcmp(BlitFragmentName(f),"blit_sint_fs")==0);
#endif
    std::printf("integer fragment selection=%d; float/depth preserved\n",types);
    return types?0:2;
}
'''.replace('@DEPTH@', depth).replace('@HELPERS@', helpers).replace('@SHADERS@', shaders).replace('@ADAPTED@', str(int(adapted)))


def check_sources(pinned, adapted, output):
    output.mkdir(parents=True, exist_ok=True)
    includes = adapted / 'deps/macos-arm64/include'
    for label, tree, expected in (('pinned', pinned, 2), ('adapted', adapted, 0)):
        mm = output / (label + '.mm'); binary = output / label
        mm.write_text(fixture(tree))
        subprocess.run(['clang++','-std=c++20','-O1','-Wall','-Wextra','-Werror',
                        '-mmacosx-version-min=15.0',
                        '-I'+str(includes),str(mm),'-o',str(binary)],check=True)
        result = subprocess.run([str(binary)],capture_output=True,text=True,timeout=10)
        if result.returncode != expected:
            raise RuntimeError(f'{label}: expected {expected}, got {result.returncode}: '
                               + result.stdout + result.stderr)
        print(label + ': ' + result.stdout.strip())
    print('PASS: actual integer blit selection/guards and 384 CPU shader-body cases; no Metal device')


if __name__ == '__main__':
    check_sources(ROOT/'runtime/upstream',source_path(ROOT),ROOT/'work/integer-blit-fixture')
