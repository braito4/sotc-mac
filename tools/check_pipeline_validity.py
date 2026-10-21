"""Compile the actual cache/Metal registration guards with CPU-only synthetic handles.

The pinned source must reproduce invalid-handle acceptance; the private adapted
source must reject it. No graphics library, GPU or game input is used.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = Path('gpu/shadps4/video_core/renderer_vulkan')

def fixture(source):
    cache = (source / BASE / 'vk_pipeline_cache.cpp').read_text()
    start = cache.index('    if (const auto found = graphics_pipelines.find(sel.graphics_key);')
    end = cache.index('    // bbport BB_ASYNC_PIPELINES:', start)
    cache_guard = cache[start:end]
    shadow = (source / BASE / 'vk_metal_shadow.h').read_text()
    start = shadow.index('    if (!Enabled() || !pipeline')
    end = shadow.index('    std::array<bool,', start)
    shadow_guard = shadow[start:end]
    return r'''
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
struct Pipeline { std::uint64_t value; auto Handle() const { return value; } };
struct Selection { int graphics_key; };
std::map<int, std::unique_ptr<Pipeline>> graphics_pipelines;
Pipeline* Cached(Selection sel) {
@CACHE@
    return nullptr;
}
bool Enabled() { return true; }
unsigned registrations = 0;
void Register(const std::unique_ptr<Pipeline>& pipeline) {
@SHADOW@
    ++registrations;
}
int main() {
    bool cache_ok = Cached({100}) == nullptr;
    graphics_pipelines[1] = nullptr;
    cache_ok &= Cached({1}) == nullptr;
    graphics_pipelines[2] = std::make_unique<Pipeline>(Pipeline{0});
    cache_ok &= Cached({2}) == nullptr;
    graphics_pipelines[3] = std::make_unique<Pipeline>(Pipeline{99});
    cache_ok &= Cached({3}) == graphics_pipelines[3].get();
    graphics_pipelines[3]->value = 0;
    cache_ok &= Cached({3}) == nullptr;
    Register(graphics_pipelines[1]);
    Register(graphics_pipelines[2]);
    bool shadow_ok = registrations == 0;
    graphics_pipelines[3]->value = 99;
    Register(graphics_pipelines[3]);
    shadow_ok &= registrations == 1;
    std::printf("cache rejects invalid=%d; Metal rejects null registration=%d\n", cache_ok, shadow_ok);
    return cache_ok && shadow_ok ? 0 : 2;
}
'''.replace('@CACHE@', cache_guard).replace('@SHADOW@', shadow_guard)

def check_sources(pinned, adapted, output):
    output.mkdir(parents=True, exist_ok=True)
    for name, source, expected in (('pinned', pinned, 2), ('adapted', adapted, 0)):
        cpp = output / (name + '.cpp'); binary = output / name
        cpp.write_text(fixture(source))
        subprocess.run(['c++', '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror',
                        str(cpp), '-o', str(binary)], check=True, capture_output=True, text=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
        if result.returncode != expected:
            raise RuntimeError(f'{name} guard regression: expected {expected}, got {result.returncode}: '
                               + result.stdout + result.stderr)
        print(name + ': ' + result.stdout.strip())
    print('PASS: actual cached-pipeline and Metal registration guards; no GPU initialized')

if __name__ == '__main__':
    check_sources(ROOT / 'runtime/upstream', ROOT / 'work/gpu-source',
                  ROOT / 'work/pipeline-validity-fixture')
