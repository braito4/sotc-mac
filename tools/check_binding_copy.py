"""Actual unified binding/copy and staging methods with original CPU doubles; no GPU."""
from pathlib import Path
import os
import subprocess
from build_gpu import source_path
from check_buffer_copy_chunks import function

ROOT = Path(__file__).resolve().parents[1]
CACHE = 'gpu/shadps4/video_core/buffer_cache/buffer_cache.cpp'
POOL = 'gpu/shadps4/video_core/renderer_vulkan/vk_staging_buffer_pool.cpp'
RASTER = 'gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp'

def fixture(tree):
    text = (tree/CACHE).read_text()
    obtain = function(text, 'std::pair<const Buffer*, u64> BufferCache::ObtainBuffer(')
    # Compile the actual unified-memory route. Other renderer architectures are
    # not exercised here; a sentinel marks their fallthrough instead of stubbing
    # the many unrelated residency/image backends.
    obtain = obtain[:obtain.index('    // bbport: a read-only binding of blocks')] + '\n    return {};\n}'
    adapted = 'binding_copies' in obtain
    copy = function(text, 'std::pair<const Buffer*, u64> BufferCache::UnifiedCopy(')
    pool = (source_path(ROOT)/POOL).read_text()
    methods = '\n'.join(function(pool, signature) for signature in (
        'u64 RoundAllocationSize(', 'StagingBufferRef StagingBufferPool::RequestLarge('))
    result = (ROOT/'tests/binding_copy_fixture.cpp.in').read_text()
    declaration = obtain[:obtain.index('{')].replace('BufferCache::', '').strip() + ';'
    for name, value in {'ADAPTED':str(adapted).lower(), 'STAGING_METHODS':methods,
                        'OBTAIN_DECLARATION':declaration, 'OBTAIN_METHOD':obtain,
                        'COPY_METHOD':copy, 'SCOPE_ARGUMENT':', &scope' if adapted else ''}.items():
        result = result.replace('@'+name+'@', value)
    return result

def main():
    source = source_path(ROOT)
    bind = function((source/RASTER).read_text(), 'void Rasterizer::BindBuffers(')
    assert bind.index('SotcBindingCopy::Scope<const VideoCore::Buffer*> binding_copies;') < bind.index('for (u32 buffer_index')
    assert bind.index('binding_copies.NextBinding(buffer_index);') < bind.index('const auto& desc =')
    assert 'size, desc.is_written, desc.is_formatted, &binding_copies)' in bind
    assert 'bound_buffers.emplace_back(buffer, offset, size, desc.is_written)' in bind
    assert 'runtime.IsBufferAccessed(buffer, offset, size, desc.is_written)' in bind
    work = ROOT/'work/binding-copy-fixture';work.mkdir(exist_ok=True)
    for name, tree, expected in (('before',ROOT/'runtime/upstream',2),('adapted',source,0)):
        cpp, binary = work/(name+'.cpp'), work/name
        cpp.write_text(fixture(tree))
        subprocess.run(['clang++','-std=c++20','-O1','-Wall','-Wextra','-Werror',
                        '-I'+str(ROOT/'runtime/src'),str(cpp),'-o',str(binary)],check=True)
        run = subprocess.run([str(binary)],capture_output=True,text=True,timeout=10,
                             env=dict(os.environ,SOTC_MEMORY_TRACE='0',SOTC_VISUAL_TRACE='0'))
        print(run.stdout,end='')
        if run.returncode != expected:
            raise RuntimeError(f'{name}: expected {expected}, got {run.returncode}: {run.stderr}')
    print('PASS: duplicate read-only copies retain 6 GiB before / 3 GiB after in symbolic CPU capacity fixtures; no GPU/native-memory claim')

if __name__ == '__main__':
    main()
