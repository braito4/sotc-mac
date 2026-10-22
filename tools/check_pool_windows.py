"""Check actual import range/page-table methods against original CPU controls."""
from pathlib import Path
import subprocess
import tempfile
from build_gpu import ROOT, source_path
from check_buffer_copy_chunks import function


def main():
    base=source_path(ROOT)/'gpu/shadps4/video_core/buffer_cache'
    cpp=(base/'buffer_cache.cpp').read_text()
    header=(base/'buffer_cache.h').read_text()
    text=(ROOT/'tests/pool_windows_fixture.cpp.in').read_text()
    fragments={
        '@WINDOW_METHOD@':function(header,'    size_t PoolWindow('),
        '@START_METHOD@':function(header,'    u64 PoolWindowStart('),
        '@RANGE_METHOD@':function(cpp,'std::optional<std::pair<const Buffer*, u64>> BufferCache::UnifiedRange('),
        '@PAGE_METHOD@':function(cpp,'void BufferCache::UnifiedPageTable('),
    }
    for tag,value in fragments.items(): text=text.replace(tag,value)
    with tempfile.TemporaryDirectory(prefix='pool-window-cpu-',dir=ROOT/'work') as temp:
        out=Path(temp);source=out/'fixture.cpp';binary=out/'fixture';source.write_text(text)
        subprocess.run(['clang++','-std=c++20','-O1','-Wall','-Wextra','-Werror',
                        '-fsanitize=address,undefined','-I'+str(ROOT/'runtime/src'),
                        str(source),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True,timeout=30)


if __name__=='__main__': main()
