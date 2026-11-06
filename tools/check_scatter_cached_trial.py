"""Test cached guard and consumer-completion ownership with Metal-protocol doubles."""
import ast
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
# Reuse the original guard fixtures without importing/running their test script.
tree=ast.parse((ROOT/'tools/check_scatter_guard_trial.py').read_text())
source=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign)
            and any(isinstance(t,ast.Name) and t.id=='SOURCE' for t in n.targets))
source=source.replace('#include "gpu_scatter_guard_metal.h"','#include "gpu_scatter_cached_guard.h"')
source=source.replace('SotcScatterGuard::Scope','SotcScatterCache::Scope')
extra=r'''
void PoolTests() {
    using namespace SotcScatterCache;
    TestDevice* d=[TestDevice new];TestDevice* other=[TestDevice new];
    const auto device=(id<MTLDevice>)d;
    Pool pool(32);
    auto a=pool.Acquire(device,16);
    TestCommand* consumer=[TestCommand new];
    pool.RetainUntil((id<MTLCommandBuffer>)consumer,device,a);
    auto b=pool.Acquire(device,16);
    assert(a!=b && pool.Read().held==0 && pool.Read().hits==0);
    [consumer complete];
    assert(pool.Read().held==16 && pool.Read().returned==1);
    auto c=pool.Acquire(device,8);
    assert(c==a && pool.Read().held==0 && pool.Read().hits==1);
    pool.Return(device,c,true);pool.Return(device,b,true);
    auto foreign=pool.Acquire((id<MTLDevice>)other,16);
    assert(foreign!=a && foreign!=b); // Never cross devices.
    pool.Return((id<MTLDevice>)other,foreign,true);
    assert(pool.Read().held==32 && pool.Read().discarded==1);
    auto large=pool.Acquire(device,40);pool.Return(device,large,true);
    assert(pool.Read().held==32 && pool.Read().discarded==2);
    auto failed=pool.Acquire(device,16);
    TestCommand* error=[TestCommand new];
    pool.RetainUntil((id<MTLCommandBuffer>)error,device,failed);
    error.status=MTLCommandBufferStatusError;
    for(MTLCommandBufferHandler h in error.handlers)h((id<MTLCommandBuffer>)error);
    [error.handlers removeAllObjects];
    assert(pool.Read().held==16 && pool.Read().discarded==3);
    Pool disabled(0);auto fresh=disabled.Acquire(device,16);disabled.Return(device,fresh,true);
    assert(disabled.Read().held==0 && disabled.Read().returned==0);
    Pool fit(64);
    auto small=fit.Acquire(device,8),big=fit.Acquire(device,32);
    fit.Return(device,big,true);fit.Return(device,small,true);
    assert(fit.Acquire(device,4)==small && fit.Acquire(device,16)==big);
    std::puts("POOL_TESTS_PASSED");
}
'''
source=source.replace('int main(int argc,char** argv)',extra+'\nint main(int argc,char** argv)')
source=source.replace('const bool indirect=mode==',
                      'if(mode=="pool"){PoolTests();return 0;}\n    const bool indirect=mode==',1)
needle='assert(*count_word==99 && index_words[0]==UINT32_MAX);'
source=source.replace(needle,r'''
    // The copy command has finished; the consumer has not. Nothing is reusable yet.
    assert(SotcScatterCache::Cache().Read().held==0);
    assert(SotcScatterCache::Cache().Read().returned==0);
    id<MTLBuffer> checked_snapshot=o.keep[0];
    [(TestCommand*)o.cmd complete];
    if(std::strcmp(std::getenv("SOTC_SCATTER_CACHE_MB"),"0")!=0){
        assert(SotcScatterCache::Cache().Read().returned==1);
        auto reused=SotcScatterCache::Cache().Acquire((id<MTLDevice>)device,checked_snapshot.length);
        assert(reused==checked_snapshot && SotcScatterCache::Cache().Read().hits==1);
        SotcScatterCache::Cache().Return((id<MTLDevice>)device,reused,true);
    } else assert(SotcScatterCache::Cache().Read().held==0);
    '''+needle)
source=source.replace('return 0;\n}}',r'''
    if(mode=="reused_invalid") {
        *count_word=3;index_words[0]=0;index_words[1]=7;index_words[2]=8;
        command=[TestCommand new];o.cmd=(id<MTLCommandBuffer>)command;blit.command=command;
        SotcScatterCache::Scope next(o,(id<MTLDevice>)device,[&]{return (id<MTLBlitCommandEncoder>)blit;},[&]{
            [command complete];o.cmd=(id<MTLCommandBuffer>)[TestCommand new];
        },1,1,1);
        std::puts("SECOND_DISPATCH_REACHED");
    }
    return 0;
}}''')

with tempfile.TemporaryDirectory(dir=ROOT/'work') as tmp:
    directory=Path(tmp);path=directory/'test.mm';binary=directory/'test';path.write_text(source)
    subprocess.run(['clang++','-std=c++20','-fobjc-arc','-fblocks','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-framework','Foundation','-framework','Metal',
                    '-I'+str(ROOT/'runtime/src'),str(path),'-o',str(binary)],check=True)
    for mode,budget in [('pool','16'),('direct','16'),('indirect','16'),('direct','0'),
                        ('unselected','16'),('disabled','16'),('invalid','16'),
                        ('truncated','16'),('dimensions','16'),('direct','bad'),('reused_invalid','16')]:
        evidence=directory/(mode+'-'+budget);evidence.mkdir()
        env=dict(os.environ,SOTC_SCATTER_GUARD='fixture,1,8,80,2,81,10,89,8,13,92,12,64',
                 SOTC_SCATTER_GUARD_DIR=str(evidence),SOTC_SCATTER_CACHE_MB=budget)
        if mode=='disabled':env.pop('SOTC_SCATTER_GUARD')
        result=subprocess.run([str(binary),mode],env=env,capture_output=True,text=True)
        expected=23 if mode in ('invalid','truncated','dimensions','reused_invalid') or budget=='bad' else 0
        assert result.returncode==expected,(mode,budget,result.returncode,result.stderr)
        if expected:
            marker='SECOND_DISPATCH_REACHED' if mode=='reused_invalid' else 'SIMULATED_DISPATCH_REACHED'
            assert marker not in result.stdout
        if mode in ('invalid','truncated','reused_invalid'):
            capture='capture-2' if mode=='reused_invalid' else 'capture-1'
            meta=json.loads((evidence/(capture+'.json')).read_text());data=(evidence/(capture+'.bin')).read_bytes()
            assert struct.unpack_from('<I',data,meta['count_offset'])[0]==((8<<20)//4+1 if mode=='truncated' else 3)
            if mode!='truncated':assert struct.unpack_from('<III',data,meta['index_offset'])==(0,7,8)
        print('PASS:',mode,'budget='+budget)
print('PASS: cached guard, copy/consumer completion separation, bounded idle bytes, best-fit, device isolation, failed commands and guard regression fixtures (ASan/UBSan); no native GPU execution')
