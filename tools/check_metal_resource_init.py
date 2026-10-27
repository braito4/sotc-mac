"""Run the actual resource Init control flow with device-export doubles, without a GPU."""
import subprocess
from build_gpu import ROOT, REVISION, source_path
from check_buffer_copy_chunks import function

REL = 'gpu/shadps4/video_core/renderer_metal/mtl_resources.mm'


def fixture(text):
    method = function(text, 'void Init(void* device)')
    # Only the Objective-C property expression used for logging needs a C++ substitute.
    method = method.replace('mtl_device.name.UTF8String', '"fake Metal device"')
    method = method.replace('(__bridge id<MTLDevice>)', '(void*)')
    return r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#define ASSERT_MSG(condition, ...) assert(condition)
using VkDevice = void*;
struct VkExportMetalDeviceInfoEXT { int type; void* mtlDevice = nullptr; };
struct VkExportMetalObjectsInfoEXT { int type; VkExportMetalDeviceInfoEXT* next; };
constexpr int VK_STRUCTURE_TYPE_EXPORT_METAL_DEVICE_INFO_EXT = 1;
constexpr int VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT = 2;
int exports = 0;
void Export(VkDevice device, VkExportMetalObjectsInfoEXT* info) {
    assert(device); ++exports; info->next->mtlDevice = device;
}
using PFN_vkExportMetalObjectsEXT = decltype(&Export);
struct Dispatcher {
    PFN_vkExportMetalObjectsEXT vkGetDeviceProcAddr(VkDevice, const char*) { return Export; }
} VULKAN_HPP_DEFAULT_DISPATCHER;
bool adapter = false, frame = false, native = false;
int native_device = 2;
bool Enabled() { return adapter; }
namespace MetalLayer { bool FrameEnabled() { return frame; }
bool NativeBoot() { return native && frame; }
void* MtlDevice() { return &native_device; } }
void* mtl_device = nullptr;
''' + method + r'''
int main() {
    int missing = 0, device = 1;
    for (int a = 0; a < 2; ++a) for (int f = 0; f < 2; ++f) for (int n = 0; n < 2; ++n) {
        adapter = a; frame = f; native = n; mtl_device = nullptr; exports = 0;
        Init(&device); Init(&device);
        const bool required = a || (BB_METAL_BACKEND && f);
        const bool direct = BB_METAL_BACKEND && native && frame;
        if (bool(mtl_device) != required || exports != int(required && !direct)) ++missing;
        if (required && direct && mtl_device != &native_device) ++missing;
    }
    return missing ? 2 : 0;
}
'''


def main():
    source = source_path(ROOT)
    before = subprocess.check_output(['git', '-C', str(source), 'show', REVISION+':'+REL], text=True)
    work = ROOT/'work/metal-resource-init-fixture'
    work.mkdir(exist_ok=True)
    for name, text in [('before', before), ('after', (source/REL).read_text())]:
        cpp = work/(name+'.cpp')
        cpp.write_text(fixture(text))
        for backend in (0, 1):
            binary = work/f'{name}-{backend}'
            subprocess.run(['clang++', '-std=c++20', f'-DBB_METAL_BACKEND={backend}',
                            str(cpp), '-o', str(binary)], check=True)
            run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
            expected = 2 if name == 'before' and backend else 0
            if run.returncode != expected:
                raise RuntimeError(f'{name}/{backend}: {run.returncode} != {expected}: {run.stderr}')
    print('PASS: upstream misses frame-only device initialization; fixed Init covers adapter/frame/native modes, idempotently, with and without compiled Metal backend. Device doubles; no native allocation claim.')


if __name__ == '__main__':
    main()
