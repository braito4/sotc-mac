"""Actual bundle observer with ordered CPU protocol doubles, no native GPU."""
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
SOURCE=base[:base.index('int main(')].replace('gpu_scatter_guard_metal.h','gpu_producer_bundle_metal.h')+r'''
int main(int argc,char** argv) {@autoreleasepool {
    assert(argc==2);const std::string mode=argv[1];
    TestDevice* device=[TestDevice new];Shader shader;Pipeline pipeline{&shader};Owned o;o.compute_pipeline=&pipeline;
    if(mode=="unselected")shader.entry="other";
    o.push[81]=12;
    for(unsigned i=1;i<=5;++i){
        shader.bindings.push_back({Metal::ShaderBinding::Kind::Buffer,i,1,0,i-1});
        auto b=[device newBufferWithLength:144 options:MTLResourceStorageModeShared];
        std::memset(b.contents,0x71,144);
        o.compute_bindings[i-1].elements.push_back({b,16,128});
    }
    auto& output=o.compute_bindings[4].elements[0];
    auto& counter=o.compute_bindings[3].elements[0];
    auto* counter_words=(std::uint32_t*)((char*)counter.buffer.contents+16);counter_words[2]=20;
    SotcFragment::U64 gen=2;
    const auto map=[](uintptr_t at,SotcFragment::U64* physical,uintptr_t* end){*physical=at+4096;*end=at+16384;return true;};
    const auto metal=reinterpret_cast<std::uintptr_t>((__bridge void*)output.buffer);
    SotcFragment::Register(100,metal,0);
    if(mode!="notarmed")SotcFragment::Copied(4096,128,100,16,9,mode!="readonly",42,4,
        (char*)output.buffer.contents+16,2,&gen,map);
    if(mode=="reused"){SotcFragment::Remove(100);SotcFragment::Register(100,metal,0);}
    if(mode=="extent")o.compute_bindings[1].elements[0].size=500;
    if(mode=="missingcounter")shader.bindings.erase(shader.bindings.begin()+3);
    if(mode=="truncated"){
        auto huge=[device newBufferWithLength:(8ull<<20)+4 options:MTLResourceStorageModeShared];
        o.compute_bindings[1].elements[0]={huge,0,(8ull<<20)+4};
    }
    auto args=[device newBufferWithLength:32 options:MTLResourceStorageModeShared];
    const std::uint32_t dims[3]={3,2,1};std::memcpy((char*)args.contents+4,dims,12);
    bool active=mode!="disabled"&&mode!="unselected"&&mode!="notarmed"&&mode!="readonly"&&mode!="reused"&&mode!="prefix";
    const bool dual=mode=="dual_budget"||mode=="dual_arming";
    const unsigned loops=dual?40:mode=="budget"?33:mode=="direct_after_fragment"?2:1;
    for(unsigned i=0;i<loops;++i){
        if(dual){
            shader.entry=i<20?"fixture":"alternate";
            if(mode=="dual_arming"&&i==20){
                auto direct=[device newBufferWithLength:144 options:MTLResourceStorageModeShared];
                std::memset(direct.contents,0x71,144);output.buffer=direct;
            }
        }
        if(i==1&&mode=="direct_after_fragment"){
            auto direct=[device newBufferWithLength:144 options:MTLResourceStorageModeShared];
            std::memset(direct.contents,0x71,144);output.buffer=direct;
        }
        auto* words=(std::uint32_t*)((char*)output.buffer.contents+16);
        const auto old=words[20];const auto old_counter=counter_words[2];
        TestCommand* command=[TestCommand new];o.cmd=(id<MTLCommandBuffer>)command;
        TestBlit* blit=[TestBlit new];blit.command=command;auto begin=[&]{return (id<MTLBlitCommandEncoder>)blit;};
        bool committed=false;
        SotcProducerBundle::Capture capture(o,(id<MTLDevice>)device,begin,1,1,1,mode=="indirect"?args:nil,4);
        // Only changes output beyond the old 64-byte prefix, plus the append counter.
        [command.copies addObject:[^{words[20]=5;words[21]=6;words[22]=7;counter_words[2]+=3;} copy]];
        assert(words[20]==old&&counter_words[2]==old_counter);
        capture.Finish(o,begin,[&]{
            committed=true;[command complete];
            if(mode=="failed")command.status=MTLCommandBufferStatusError;
            // Completion-time reuse must not alter saved before/after data.
            words[20]=0xffffffff;counter_words[2]=0xffffffff;
            o.cmd=(id<MTLCommandBuffer>)[TestCommand new];
        });
        const bool expected_capture=dual?(i%20<16&&(mode!="dual_arming"||i<20)):(active&&i<32);
        assert(committed==expected_capture);
        if(committed)assert(words[20]==0xffffffff&&counter_words[2]==0xffffffff);
        assert(output.offset==16&&output.size==128); // No rebind or writeback.
    }
    return 0;
}}
'''
with tempfile.TemporaryDirectory(dir=ROOT/'work') as tmp:
    p=Path(tmp);(p/'test.mm').write_text(SOURCE)
    subprocess.run(['clang++','-std=c++20','-fobjc-arc','-fblocks','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-framework','Foundation','-framework','Metal',
                    '-I'+str(ROOT/'runtime/src'),str(p/'test.mm'),'-o',str(p/'test')],check=True)
    modes=('direct','indirect','direct_after_fragment','notarmed','readonly','reused','disabled',
           'unselected','extent','missingcounter','failed','truncated','budget','selection','collision',
           'dual_budget','dual_arming','duplicate','trailing','three_entries','prefix')
    for mode in modes:
        out=p/mode;out.mkdir()
        env=dict(os.environ,SOTC_FRAGMENT_PROVENANCE='1',SOTC_PRODUCER_BUNDLE='fixture,5,4,64',SOTC_SCATTER_GUARD_DIR=str(out))
        if mode=='disabled':env.pop('SOTC_PRODUCER_BUNDLE')
        if mode=='selection':env['SOTC_PRODUCER_BUNDLE']='fixture,5,5,64'
        if mode in ('dual_budget','dual_arming'):env['SOTC_PRODUCER_BUNDLE']='fixture|alternate,5,4,64'
        if mode=='duplicate':env['SOTC_PRODUCER_BUNDLE']='fixture|fixture,5,4,64'
        if mode=='trailing':env['SOTC_PRODUCER_BUNDLE']='fixture|,5,4,64'
        if mode=='three_entries':env['SOTC_PRODUCER_BUNDLE']='fixture|alternate|third,5,4,64'
        if mode=='prefix':env['SOTC_PRODUCER_BUNDLE']='fixture_long|other,5,4,64'
        if mode=='collision':(out/'bundle-1.bin').write_bytes(b'preserved')
        result=subprocess.run([str(p/'test'),mode],env=env,capture_output=True,text=True)
        expected=23 if mode in ('extent','missingcounter','failed','selection','collision','duplicate','trailing','three_entries') else 0
        assert result.returncode==expected,(mode,result.stderr)
        if mode in ('direct','indirect','direct_after_fragment','truncated','budget','dual_budget','dual_arming'):
            count=32 if mode in ('budget','dual_budget') else 16 if mode=='dual_arming' else 2 if mode=='direct_after_fragment' else 1
            assert len(list(out.glob('*.json')))==count
            m=json.loads((out/'bundle-1.json').read_text());raw=(out/'bundle-1.bin').read_bytes()
            assert m['completed'] and m['output_fallback_observed'] and len(m['bindings'])==5
            assert raw[m['push_offset']+81]==12
            assert struct.unpack_from('<III',raw)==((3,2,1) if mode=='indirect' else (1,1,1))
            byslot={x['slot']:x for x in m['bindings']};v=byslot[5];c=byslot[4]
            assert raw[v['before']:v['before']+64]==raw[v['after']:v['after']+64]
            assert struct.unpack_from('<III',raw,v['after']+80)==(5,6,7)
            assert struct.unpack_from('<I',raw,c['before']+8)==(20,)
            assert struct.unpack_from('<I',raw,c['after']+8)==(23,)
            assert all(x['changed_bytes']>0 for x in m['comparisons'])
            assert byslot[2]['truncated']==(mode=='truncated')
            if mode=='direct_after_fragment':
                m2=json.loads((out/'bundle-2.json').read_text());assert not m2['output_fallback_observed']
            if mode=='budget':assert 'budget exhausted omitted=1' in result.stderr
            if mode in ('dual_budget','dual_arming'):
                assert m['entry']=='fixture' and m['selection_budget']==16
                assert 'budget exhausted omitted=1' in result.stderr
                if mode=='dual_budget':
                    second=json.loads((out/'bundle-17.json').read_text())
                    assert second['entry']=='alternate' and second['selection_index']==1
        elif mode=='collision':assert (out/'bundle-1.bin').read_bytes()==b'preserved'
        else:assert not list(out.glob('bundle-*'))
        print('PASS:',mode)
print('PASS: full input/output/counter snapshots, writes beyond prefix, fragment arming followed by direct bindings, ordering/retention, truncation and refusal paths under ASan/UBSan; no native GPU')
