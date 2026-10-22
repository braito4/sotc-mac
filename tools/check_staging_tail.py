"""Compare actual preceding/adapted staging methods with original CPU doubles."""
from pathlib import Path
import json
import os
import subprocess
import tempfile
from build_gpu import source_path, PATCH_DIR
from check_buffer_copy_chunks import function

ROOT=Path(__file__).resolve().parents[1]
BASE='gpu/shadps4/video_core/renderer_vulkan/vk_staging_buffer_pool'

def fixture(tree):
    source=(tree/(BASE+'.cpp')).read_text()
    header=(tree/(BASE+'.h')).read_text()
    request=function(source,'StagingBufferRef StagingBufferPool::RequestLarge(')
    adapted='tail_open' in request
    declaration=request.split('{',1)[0].replace('StagingBufferPool::','').strip()+';'
    methods='\n'.join(function(source,s) for s in (
        'u64 RoundAllocationSize(', 'StagingBufferRef StagingBufferPool::RequestLarge(',
        'void StagingBufferPool::FreeDeferred('))
    text=(ROOT/'tests/staging_tail_fixture.cpp.in').read_text()
    for name,value in {'METHODS':methods,'REQUEST_DECLARATION':declaration,
                       'ADAPTED':str(adapted).lower(),
                       'ALIGNMENT_ARGUMENT':',alignment' if adapted else '',
                       'FLUSH':function(header,'void Flush() const noexcept')}.items():
        text=text.replace('@'+name+'@',value)
    return text

def main():
    source=source_path(ROOT)
    request=function((source/(BASE+'.cpp')).read_text(),'StagingBufferRef StagingBufferPool::Request(')
    assert 'return RequestLarge(size, type, deferred, alignment);' in request
    # Remove only this overlay in an isolated file fixture, preserving every other correction.
    with tempfile.TemporaryDirectory(prefix='staging-tail-',dir=ROOT/'work') as temporary:
        before=Path(temporary)
        for ext in ('.cpp','.h'):
            target=before/(BASE+ext);target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes((source/(BASE+ext)).read_bytes())
        patch='\n'.join(json.loads((PATCH_DIR/'sotc-staging-tail.patch.json').read_text())['lines'])+'\n'
        subprocess.run(['git','-C',str(before),'apply','-R'],input=patch,text=True,check=True,capture_output=True)
        old=(before/(BASE+'.cpp')).read_text()
        new=(source/(BASE+'.cpp')).read_text()
        for signature in ('u64 RoundAllocationSize(', 'void StagingBufferPool::TrimLarge(',
                          'void StagingBufferPool::FreeDeferred('):
            assert function(old,signature)==function(new,signature)
        for name,tree,expected in (('before',before,2),('adapted',source,0)):
            cpp=before/(name+'.cpp');binary=before/name
            cpp.write_text(fixture(tree))
            subprocess.run(['clang++','-std=c++20','-O1','-Wall','-Wextra','-Werror',
                            '-I'+str(ROOT/'runtime/src'),str(cpp),'-o',str(binary)],check=True)
            result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=10,
                                  env=dict(os.environ,SOTC_MEMORY_TRACE='0'))
            print(name+': '+result.stdout,end='')
            if result.returncode!=expected:
                raise RuntimeError(f'{name}: expected {expected}, got {result.returncode}: {result.stderr}')
    print('PASS: actual-source tail allocation lowers synthetic retained capacity; no GPU/native savings claim')

if __name__=='__main__':
    main()
