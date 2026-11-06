"""Ordered producer snapshots using deferred Metal-protocol doubles, no native GPU."""
import ast
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
tree=ast.parse((ROOT/'tools/check_scatter_guard_trial.py').read_text())
base=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign)
          and any(isinstance(t,ast.Name) and t.id=='SOURCE' for t in n.targets))
SOURCE=base[:base.index('int main(')].replace('gpu_scatter_guard_metal.h','gpu_fragment_producer_metal.h')+r'''
int main(int argc,char** argv) {@autoreleasepool {
    assert(argc==2);std::string mode=argv[1];
    TestDevice* device=[TestDevice new];Shader shader;shader.entry="producer\"fixture";
    shader.bindings.push_back({Metal::ShaderBinding::Kind::Buffer,5,1,0,4});
    Pipeline pipeline{&shader};Owned o;o.compute_pipeline=&pipeline;o.push[81]=12;
    id<MTLBuffer> buffer=[device newBufferWithLength:160 options:MTLResourceStorageModeShared];
    // Aligned binding includes a 12-byte prefix; snapshot must start at the logical copy.
    o.compute_bindings[4].elements.push_back({buffer,16,76});
    std::memset(buffer.contents,0x44,160);
    auto* words=(std::uint32_t*)((char*)buffer.contents+28);
    for(unsigned i=0;i<16;++i)words[i]=1000+i;
    auto args=[device newBufferWithLength:32 options:MTLResourceStorageModeShared];
    const std::uint32_t dims[3]={3,2,1};std::memcpy((char*)args.contents+4,dims,12);
    auto identity=reinterpret_cast<std::uintptr_t>((__bridge void*)buffer);
    SotcFragment::Register(100,identity,0);SotcFragment::U64 gen=2;
    const auto map=[](uintptr_t at,SotcFragment::U64* physical,uintptr_t* end){*physical=at+4096;*end=at+16384;return true;};
    SotcFragment::Copied(4096,64,100,28,9,mode!="readonly",mode=="unselected"?43:42,4,
        words,2,&gen,map);
    if(mode=="reused"){SotcFragment::Remove(100);SotcFragment::Register(100,identity,0);}
    if(mode=="extent")o.compute_bindings[4].elements[0].size=32;
    const bool active=mode!="disabled"&&mode!="readonly"&&mode!="unselected"&&mode!="reused";
    const unsigned loops=mode=="budget"?17:1;
    for(unsigned i=0;i<loops;++i){
        TestCommand* command=[TestCommand new];o.cmd=(id<MTLCommandBuffer>)command;
        TestBlit* blit=[TestBlit new];blit.command=command;
        auto begin=[&]{return (id<MTLBlitCommandEncoder>)blit;};
        bool committed=false;
        SotcFragmentProducer::Capture capture(o,(id<MTLDevice>)device,begin,1,1,1,
            mode=="indirect"?args:nil,4);
        // Emulate a dispatch queued between pre/post copies; no bytes change at encode time.
        if(mode!="unchanged") [command.copies addObject:[^{for(unsigned j=0;j<16;++j)words[j]=j+5;} copy]];
        if(i==0)assert(words[0]==1000);
        capture.Finish(o,begin,[&]{
            committed=true;[command complete];
            if(mode=="failed")command.status=MTLCommandBufferStatusError;
            // Reuse immediately after completion: saved output must remain its own snapshot.
            words[0]=0xffffffff;
            o.cmd=(id<MTLCommandBuffer>)[TestCommand new];
        });
        assert(committed==(active&&i<16));
        assert(o.compute_bindings[4].elements[0].buffer==buffer);
        assert(o.compute_bindings[4].elements[0].offset==16);
        if(committed)assert(words[0]==0xffffffff); // No copyback repaired/replaced this input.
    }
    return 0;
}}
'''

with tempfile.TemporaryDirectory(dir=ROOT/'work') as tmp:
    p=Path(tmp);(p/'test.mm').write_text(SOURCE)
    subprocess.run(['clang++','-std=c++20','-fobjc-arc','-fblocks','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-framework','Foundation','-framework','Metal',
                    '-I'+str(ROOT/'runtime/src'),str(p/'test.mm'),'-o',str(p/'test')],check=True)
    modes=('direct','indirect','unchanged','disabled','readonly','unselected','reused','extent','failed','budget','collision','selection')
    for mode in modes:
        out=p/mode;out.mkdir()
        env=dict(os.environ,SOTC_FRAGMENT_PROVENANCE='1',SOTC_FRAGMENT_PRODUCER='2a:4',
                 SOTC_SCATTER_GUARD_DIR=str(out))
        if mode=='disabled':env.pop('SOTC_FRAGMENT_PRODUCER')
        if mode=='selection':env['SOTC_FRAGMENT_PRODUCER']='2a:999'
        if mode=='collision':(out/'producer-1.bin').write_bytes(b'preserved')
        result=subprocess.run([str(p/'test'),mode],env=env,text=True,capture_output=True)
        expected=23 if mode in ('extent','failed','collision','selection') else 0
        assert result.returncode==expected,(mode,result.stderr)
        if mode in ('direct','indirect','unchanged','budget'):
            count=16 if mode=='budget' else 1
            assert len(list(out.glob('*.json')))==count
            m=json.loads((out/'producer-1.json').read_text());b=(out/'producer-1.bin').read_bytes()
            assert m['entry']=='producer"fixture' and m['completed']
            assert m['offset']==28 and m['bound_offset']==16 and m['bytes']==64
            assert len(b)==576 and b[m['push_offset']+81]==12
            assert struct.unpack_from('<III',b)==((3,2,1) if mode=='indirect' else (1,1,1))
            assert struct.unpack_from('<16I',b,m['before_offset'])==tuple(range(1000,1016))
            assert struct.unpack_from('<16I',b,m['after_offset'])==tuple(range(1000,1016) if mode=='unchanged' else range(5,21))
            assert bytes.fromhex(m['cpu_copy_head'])==b[320:384]
            assert m['before_after_prefix_equal']==(mode=='unchanged')
            if mode=='budget':assert 'budget exhausted omitted=1' in result.stderr
        elif mode=='collision':assert (out/'producer-1.bin').read_bytes()==b'preserved'
        else:assert not list(out.glob('producer-*'))
        print('PASS:',mode)
print('PASS: ordered before/dispatch/after snapshots, alignment, completion, immutable evidence, budget and refusal paths under ASan/UBSan; no GPU execution')
