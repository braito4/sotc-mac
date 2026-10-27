"""Exercise actual display selection, cache, and presenter acquisition with CPU device doubles."""
from pathlib import Path
import subprocess
from build_gpu import ROOT, source_path
from check_buffer_copy_chunks import function

BASE = 'gpu/shadps4/video_core/renderer_vulkan'

def fixture(tree):
    text = (tree/BASE/'vk_temporal_upscaler.cpp').read_text()
    override = function(text, 'bool TemporalUpscaler::DisplayOverride(')
    mirror = function(text, 'vk::ImageView TemporalUpscaler::Mirror(')
    presenter = (tree/BASE/'vk_presenter.cpp').read_text()
    start = presenter.index('    if (upscaled) {') + len('    if (upscaled) {')
    acquisition = presenter[start:presenter.index('        image_size =', start)]
    adapted = 'vk::Format view_format' in override
    template = (ROOT/'tests/display_view_fixture.cpp.in').read_text()
    for name, value in {'METHODS':mirror+'\n'+override, 'ACQUISITION':acquisition,
                        'FORMAT_DECL':', vk::Format view_format' if adapted else '',
                        'FORMAT_ARG':', format' if adapted else '',
                        'ADAPTED':str(adapted).lower()}.items():
        template = template.replace('@'+name+'@', value)
    return template

def main():
    source = source_path(ROOT)
    work = ROOT/'work/display-view-fixture'; work.mkdir(exist_ok=True)
    for name, tree, expected in [('before', ROOT/'runtime/upstream', 2), ('adapted', source, 0)]:
        cpp=work/(name+'.cpp'); binary=work/name
        cpp.write_text(fixture(tree))
        subprocess.run(['clang++','-std=c++20','-DVULKAN_HPP_NO_CONSTRUCTORS','-O1','-fsanitize=undefined',
                        '-fno-sanitize-recover=undefined',
                        '-I'+str(source/'deps/macos-arm64/include'),str(cpp),'-o',str(binary)],check=True)
        run=subprocess.run([str(binary)],capture_output=True,text=True,timeout=10)
        print(run.stdout,end='')
        if run.returncode != expected:
            raise RuntimeError(f'{name}: expected {expected}, got {run.returncode}: {run.stderr}')
    print('PASS: actual display/presenter methods retain views for delayed consumers, reuse by format/mapping, and release with image cache; CPU doubles, no GPU claim')

if __name__ == '__main__': main()
