"""Actual ordered producer preflight with CPU protocol doubles and sanitizers."""
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
SOURCE=base[:base.index('int main(')].replace('gpu_scatter_guard_metal.h','gpu_producer_input_metal.h')+r'''
int main(int argc,char** argv) {@autoreleasepool {
    assert(argc==2);const std::string mode=argv[1];
    TestDevice* device=[TestDevice new];Shader shader;Pipeline pipeline{&shader};Owned o;o.compute_pipeline=&pipeline;
    if(mode=="unselected")shader.entry="fixture_suffix";
    if(mode=="second")shader.entry="alternate";
    const bool indirect=mode=="indirect"||mode=="dimensions"||mode=="truncated";
    std::array<unsigned,3> slots{1,2,3},push{80,81,82};std::array<uint64_t,3> sizes{32,64,68};
    if(mode=="truncated")sizes[1]=(8ull<<20)+64;
    for(unsigned i=0;i<3;++i){
        shader.bindings.push_back({Metal::ShaderBinding::Kind::Buffer,slots[i],1,0,i});
        auto buffer=[device newBufferWithLength:sizes[i]+16 options:MTLResourceStorageModeShared];
        o.compute_bindings[i].elements.push_back({buffer,16,sizes[i]});o.push[push[i]]=i==0?8:i==1?12:4;
    }
    auto& params=o.compute_bindings[0].elements[0];auto& indices=o.compute_bindings[1].elements[0];
    auto& normals=o.compute_bindings[2].elements[0];
    const id<MTLBuffer> original_params=params.buffer,original_indices=indices.buffer;
    auto* words=(uint32_t*)((char*)indices.buffer.contents+indices.offset+12);
    words[0]=UINT32_MAX;words[1]=UINT32_MAX; // Prefix must not be mistaken for active indices.
    const uint32_t valid[6]={0,7,1,2,3,4};std::memcpy(words+2,valid,sizeof(valid));
    auto* fields=(uint32_t*)((char*)params.buffer.contents+params.offset+8);
    fields[0]=2;fields[1]=2;
    if(mode=="invalid")words[4]=8;
    if(mode=="stride4"||mode=="stride8")words[4]=12;
    if(mode=="short")fields[1]=5;
    if(mode=="wrap")fields[0]=UINT32_MAX;
    if(mode=="zero"){fields[0]=UINT32_MAX;fields[1]=0;}
    if(mode=="truncated"){std::memset(words,0,sizes[1]-12);fields[0]=0;fields[1]=699052;}
    if(mode=="extent")normals.size=4096;
    if(mode=="duplicate_binding")shader.bindings.push_back(shader.bindings[1]);
    if(mode=="push_floor"){o.push[80]=11;o.push[81]=15;o.push[82]=7;}
    auto args=[device newBufferWithLength:32 options:MTLResourceStorageModeShared];
    auto* dims=(uint32_t*)((char*)args.contents+4);dims[0]=mode=="truncated"?20000:1;dims[1]=mode=="dimensions"?2:1;dims[2]=1;
    TestCommand* command=[TestCommand new];o.cmd=(id<MTLCommandBuffer>)command;
    TestBlit* blit=[TestBlit new];blit.command=command;bool committed=false;
    {
        SotcProducerInput::Scope guard(o,(id<MTLDevice>)device,[&]{return (id<MTLBlitCommandEncoder>)blit;},[&]{
            committed=true;[command complete];if(mode=="failed")command.status=MTLCommandBufferStatusError;
            fields[0]=99;fields[1]=99;words[2]=UINT32_MAX;dims[0]=0;
            o.cmd=(id<MTLCommandBuffer>)[TestCommand new];
        },1,1,1,indirect?args:nil,4);
        if(mode=="unselected"||mode=="disabled"){
            assert(!committed&&params.buffer==original_params&&indices.buffer==original_indices);return 0;
        }
        assert(committed&&params.buffer!=original_params&&indices.buffer!=original_indices);
        const auto* saved=(const uint32_t*)((const char*)params.buffer.contents+params.offset+8);
        assert(saved[0]==(mode=="zero"?UINT32_MAX:2)&&saved[1]==(mode=="zero"?0:2));
        const auto* checked=(const uint32_t*)((const char*)indices.buffer.contents+indices.offset+12);
        assert(checked[2]==0&&checked[3]==7);
        if(indirect){assert(guard.args!=args&&guard.args_offset==0);assert(*(uint32_t*)guard.args.contents==1);}
        assert(o.keep.size()==1&&o.keep[0]==indices.buffer);
        std::puts("SIMULATED_PRODUCER_REACHED");
    }
    assert(params.buffer==original_params&&indices.buffer==original_indices);
    assert(params.offset==16&&params.size==32&&indices.offset==16&&indices.size==64);
    assert(fields[0]==99&&words[2]==UINT32_MAX); // Validation never repairs guest memory.
    return 0;
}}
'''

with tempfile.TemporaryDirectory(dir=ROOT/'work') as tmp:
    p=Path(tmp);(p/'test.mm').write_text(SOURCE)
    subprocess.run(['clang++','-std=c++20','-fobjc-arc','-fblocks','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-framework','Foundation','-framework','Metal',
                    '-I'+str(ROOT/'runtime/src'),str(p/'test.mm'),'-o',str(p/'test')],check=True)
    modes=('direct','indirect','second','unselected','disabled','invalid','short','wrap','zero',
           'truncated','extent','duplicate_binding','push_floor','failed','dimensions',
           'stride4','stride8','malformed','duplicate_profile','collision')
    failures={'invalid','short','wrap','truncated','extent','duplicate_binding','failed','dimensions','stride8','malformed','duplicate_profile','collision'}
    evidence_modes={'invalid','short','wrap','truncated','stride8'}
    for mode in modes:
        out=p/mode;out.mkdir()
        layout='fixture,1,80,0,4,2,81,3,82,8,64'
        if mode=='stride4':layout=layout.replace(',8,64',',4,64')
        if mode=='second':layout+=';alternate,1,80,0,4,2,81,3,82,8,64'
        if mode=='malformed':layout=layout.replace(',3,82,',',2,82,')
        if mode=='duplicate_profile':layout+=';'+layout
        env=dict(os.environ,SOTC_PRODUCER_INPUT_GUARD=layout,SOTC_SCATTER_GUARD_DIR=str(out))
        if mode=='disabled':env.pop('SOTC_PRODUCER_INPUT_GUARD')
        if mode=='collision':
            (out/'producer-input-1.bin').write_bytes(b'preserved')
            # Exercise evidence collision with a bad index.
        arg='invalid' if mode=='collision' else mode
        result=subprocess.run([str(p/'test'),arg],env=env,capture_output=True,text=True)
        assert result.returncode==(23 if mode in failures else 0),(mode,result.returncode,result.stderr)
        if mode in failures:assert 'SIMULATED_PRODUCER_REACHED' not in result.stdout
        if mode in evidence_modes:
            m=json.loads((out/'producer-input-1.json').read_text());raw=(out/'producer-input-1.bin').read_bytes()
            assert m['snapshot_completed'] and not m['producer_encoded']
            assert len(raw)==m['index_offset']+max(4,m['index_bytes'])
            if mode=='invalid':
                assert m['invalid']==1 and m['checked']==6
                assert struct.unpack_from('<I',raw,m['index_offset']+12+16)[0]==8
            if mode=='wrap':assert m['arithmetic_wrap']
            if mode=='truncated':assert m['truncated'] and not m['input_short']
            assert 'binary_saved=1 metadata_saved=1' in result.stderr
        if mode=='collision':assert (out/'producer-input-1.bin').read_bytes()==b'preserved'
        print('PASS:',mode)
print('PASS: producer inputs validated before dispatch; immutable source/args, restored bindings, stride distinction, short/wrap/truncation/error evidence under ASan/UBSan; CPU doubles only')
