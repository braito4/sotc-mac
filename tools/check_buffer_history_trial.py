"""Bounded metadata-history causality, alias lifetime and strict-guard integration fixtures.

Uses original synthetic data and a fake Metal device; never executes the game or GPU.
"""
import ast
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = r'''
#include "gpu_fragment_provenance.h"
#include <cassert>
#include <thread>
extern void OtherTranslationUnit();
using namespace SotcBufferHistory;
int main(int argc,char**) {
 if(argc>1){assert(!Enabled());Register(1,101,0,64);assert(Get().Inspect(101,0,64).total==0);return 0;}
 assert(Enabled());
 // Physical aliases in overlapping native pool windows must identify the same byte range.
 Registry r;r.Register(1,101,0,1024);r.Pool(1,0,1024);
 r.Register(2,102,0,1024);r.Pool(2,512,1024);
 r.NativeWrite(Kind::Fill,11,0,0,101,600,32,0xab);
 auto v=r.Inspect(102,88,32);assert(v.hits.size()==1&&v.hits[0].physical_match);
 assert(v.hits[0].event.value==0xab&&v.hits[0].overlap_bytes==32);
 r.Binding(2,88,32,4096,42,7,13);v=r.Inspect(101,600,32);
 assert(v.hits[0].event.kind==Kind::ShaderWriteIntent&&v.hits[0].event.shader==42);
 // Partial copy intersections are translated precisely; later writes cannot explain older copies.
 Registry chain;chain.Register(1,201,0,1024);chain.Register(2,202,0,1024);chain.Register(3,203,0,1024);
 chain.NativeWrite(Kind::Fill,1,0,0,201,100,100,11);
 chain.NativeWrite(Kind::Copy,2,201,100,202,200,100);
 chain.NativeWrite(Kind::Fill,3,0,0,201,100,100,22);
 chain.NativeWrite(Kind::Copy,4,202,220,203,300,60);
 v=chain.Inspect(203,310,20);assert(v.nodes.size()==3&&v.hits.size()==3);
 assert(v.nodes[1].span.offset==230&&v.nodes[1].span.bytes==20);
 assert(v.nodes[2].span.offset==130&&v.nodes[2].span.bytes==20);
 assert(v.hits[2].event.value==11); // not the later 22
 // Identical raw handles after removal are new lifetimes; shared aliases retain one lifetime.
 Registry life;life.Register(1,301,0,128);life.Register(2,301,32,64);
 life.NativeWrite(Kind::Fill,1,0,0,301,40,8);life.Remove(1);
 assert(life.Inspect(301,40,8).hits.size()==1);life.Remove(2);life.Register(1,301,0,128);
 assert(life.Inspect(301,40,8).hits.empty());
 // Pool import epochs reset once all imports disappear, even at identical physical offsets.
 r.Remove(1);r.Remove(2);r.Register(3,103,0,1024);r.Pool(3,0,1024);
 assert(r.Inspect(103,600,32).hits.empty());
 // Unknown native identity must never match solely by raw pointer.
 Registry unknown;unknown.NativeWrite(Kind::Fill,1,0,0,999,0,32);
 assert(unknown.Inspect(999,0,32).hits.empty());
 // Overflow, empty ranges, evicted ring events, live-registry and traversal budgets are explicit.
 assert(!SpanValid(UINT64_MAX-3,4)&&!Overlap(0,0,0,4)&&!Overlap(0,4,4,4));
 unknown.NativeWrite(Kind::Copy,1,201,UINT64_MAX-1,203,0,4);
 assert(unknown.Inspect(999,0,32).invalid_ranges==1);
 Registry small(2,1);small.Register(1,101,0,64);small.Register(2,102,0,64);
 for(unsigned i=0;i<3;++i)small.NativeWrite(Kind::Fill,i,0,0,101,0,64);
 v=small.Inspect(101,0,64);assert(v.hits.size()==2&&v.evicted==2&&v.alias_omitted==1);
 Registry none(0,0);none.Register(1,1,0,64);none.Mapping(4096,32,false);
 v=none.Inspect(1,0,64);assert(v.hits.empty()&&v.evicted==1);
 Registry budget;budget.Register(1,1,0,64);
 for(unsigned i=0;i<12;++i)budget.NativeWrite(Kind::Fill,i,0,0,1,0,64);
 v=budget.Inspect(1,0,64);assert(v.hits.size()==8&&v.matches_omitted==4);
 Registry nodes;for(unsigned i=1;i<=9;++i)nodes.Register(i,i,0,64);
 for(unsigned i=2;i<=9;++i)nodes.NativeWrite(Kind::Copy,i,1,0,i,0,64);
 for(unsigned i=2;i<=9;++i)nodes.NativeWrite(Kind::Copy,20+i,i,0,1,0,64);
 v=nodes.Inspect(1,0,64);assert(v.nodes.size()==16&&v.nodes_omitted>0);
 // Guest mapping notices are separate evidence, never invented allocation ownership.
 life.Mapping(4096,32,true);life.Mapping(4104,8,false);v=life.Inspect(301,0,64,4096,32);
 unsigned maps=0;for(const auto& e:v.lifecycle)if(e.kind==Kind::Map||e.kind==Kind::Unmap)++maps;
 assert(maps==2);
 // Cross-translation-unit singleton and CPU snapshot provenance over discontiguous segments.
 OtherTranslationUnit();assert(Get().Inspect(401,0,32).hits.size()==1);
 Register(501,501,0,4096);Pool(501,0,4096);Register(502,502,0,128);
 NativeWrite(Kind::Fill,7,0,0,501,64,16,1);NativeWrite(Kind::Fill,8,0,0,501,128,16,2);
 const auto query=[](U64 at,U64* physical,uintptr_t* end){
   if(at<4112){*physical=64+at-4096;*end=4112;return true;}
   if(at<4128){*physical=128+at-4112;*end=4128;return true;}return false;
 };
 CpuCopy(502,8,40,4096,99,2,9,query);v=Get().Inspect(502,8,40);
 assert(v.hits.size()==4&&v.nodes.size()==3);
 unsigned snapshots=0;U64 covered=0;for(const auto& h:v.hits)if(h.node==0){++snapshots;covered+=h.overlap_bytes;}
 assert(snapshots==2&&covered==32); // trailing mapping gap remains unattributed
 // Concurrent event producers serialize metadata safely within the bounded ring.
 Registry concurrent(128);concurrent.Register(1,1,0,64);std::vector<std::thread> threads;
 for(unsigned i=0;i<4;++i)threads.emplace_back([&]{for(unsigned j=0;j<1000;++j)concurrent.NativeWrite(Kind::Fill,j,0,0,1,0,64);});
 for(auto& t:threads)t.join();v=concurrent.Inspect(1,0,64);assert(v.total==4001&&v.evicted==3873);
 Dump(2,502,8,40,4096,40);Dump(2,502,8,40);
 return 0;
}
'''
OTHER = r'''
#include "gpu_buffer_history.h"
void OtherTranslationUnit(){SotcBufferHistory::Register(401,401,0,32);
 SotcBufferHistory::NativeWrite(SotcBufferHistory::Kind::Update,1,0,0,401,0,32);}
'''
with tempfile.TemporaryDirectory(dir=ROOT/'work') as tmp:
    p=Path(tmp)
    flags=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
           '-I'+str(ROOT/'runtime/src')]
    (p/'main.cpp').write_text(SOURCE);(p/'other.cpp').write_text(OTHER)
    subprocess.run(flags+[str(p/'main.cpp'),str(p/'other.cpp'),'-o',str(p/'test')],check=True)
    env=dict(os.environ,SOTC_BUFFER_HISTORY='1',SOTC_SCATTER_GUARD_DIR=str(p))
    collision=p/'history-2-slot-2.json';collision.write_text('keep existing evidence')
    result=subprocess.run([str(p/'test')],env=env,capture_output=True,text=True)
    assert result.returncode==0,result.stderr
    assert 'mapping incomplete copied=32 requested=40' in result.stderr
    assert 'saved=0' in result.stderr and collision.read_text()=='keep existing evidence'
    report=json.loads((p/'history-1-slot-2.json').read_text())
    assert len(report['candidates'])==4 and report['nodes'][0]['span']['bytes']==40
    assert (p/'history-1-slot-2.json').stat().st_mode&0o777==0o600
    disabled=dict(env,SOTC_BUFFER_HISTORY='0')
    subprocess.run([str(p/'test'),'disabled'],env=disabled,check=True)
    print('PASS: physical aliases, copy cutoffs/intersections, native/pool reuse, unknown identity, overflow, budgets, CPU gaps, mapping notices, concurrent producers, shared registry, private JSON and no-overwrite')
    tree=ast.parse((ROOT/'tools/check_scatter_guard_trial.py').read_text())
    source=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign)
                and any(isinstance(t,ast.Name) and t.id=='SOURCE' for t in n.targets))
    source=source.replace('gpu_scatter_guard_metal.h','gpu_scatter_provenance_guard.h').replace('SotcScatterGuard::Scope','SotcFragment::Scope')
    source=source.replace('auto args=[device newBufferWithLength:32',r'''
    const auto identity=reinterpret_cast<std::uintptr_t>((__bridge void*)index.buffer);
    SotcBufferHistory::Register(500,identity,0,index.buffer.length);
    SotcBufferHistory::NativeWrite(SotcBufferHistory::Kind::Update,9,0,0,identity,index.offset,index.size);
    auto args=[device newBufferWithLength:32''')
    (p/'guard.mm').write_text(source)
    subprocess.run(flags+['-fobjc-arc','-fblocks','-framework','Foundation','-framework','Metal',str(p/'guard.mm'),'-o',str(p/'guard')],check=True)
    for mode in ('direct','indirect','invalid'):
        evidence=p/mode;evidence.mkdir()
        runenv=dict(env,SOTC_SCATTER_GUARD='fixture,1,8,80,2,81,10,89,8,13,92,12,64',SOTC_SCATTER_GUARD_DIR=str(evidence))
        result=subprocess.run([str(p/'guard'),mode],env=runenv,capture_output=True,text=True)
        assert result.returncode==(23 if mode=='invalid' else 0),(mode,result.stderr)
        reports=list(evidence.glob('history-*.json'))
        if mode=='invalid':
            assert len(reports)==4 and 'SIMULATED_DISPATCH_REACHED' not in result.stdout
            entries=[json.loads(f.read_text()) for f in reports]
            assert any(any(c['event']['kind']=='update_encoded' for c in e['candidates']) for e in entries)
            assert (evidence/'capture-1.bin').is_file()
        else:assert not reports
        print('PASS: actual guard history hook',mode)
    # Reuse the original bundle protocol fixture, adding real history hooks for each binding.
    guard_tree=ast.parse((ROOT/'tools/check_scatter_guard_trial.py').read_text())
    base=next(ast.literal_eval(n.value) for n in guard_tree.body if isinstance(n,ast.Assign)
              and any(isinstance(t,ast.Name) and t.id=='SOURCE' for t in n.targets))
    bundle_tree=ast.parse((ROOT/'tools/check_producer_bundle_trial.py').read_text())
    expression=next(n.value for n in bundle_tree.body if isinstance(n,ast.Assign)
                    and any(isinstance(t,ast.Name) and t.id=='SOURCE' for t in n.targets))
    bundle=eval(compile(ast.Expression(expression),'<original bundle fixture>','eval'),{'base':base})
    bundle=bundle.replace('o.compute_bindings[i-1].elements.push_back({b,16,128});',r'''
        o.compute_bindings[i-1].elements.push_back({b,16,128});
        const auto identity=reinterpret_cast<std::uintptr_t>((__bridge void*)b);
        SotcBufferHistory::Register(i,identity,0,b.length);
        SotcBufferHistory::NativeWrite(SotcBufferHistory::Kind::Fill,1,0,0,identity,16,128,0x71);
    ''')
    (p/'bundle.mm').write_text(bundle)
    subprocess.run(flags+['-fobjc-arc','-fblocks','-framework','Foundation','-framework','Metal',str(p/'bundle.mm'),'-o',str(p/'bundle')],check=True)
    evidence=p/'bundle-evidence';evidence.mkdir()
    runenv=dict(env,SOTC_FRAGMENT_PROVENANCE='1',SOTC_PRODUCER_BUNDLE='fixture,5,4,64',SOTC_SCATTER_GUARD_DIR=str(evidence))
    result=subprocess.run([str(p/'bundle'),'direct_after_fragment'],env=runenv,capture_output=True,text=True)
    assert result.returncode==0,result.stderr
    reports=[json.loads(f.read_text()) for f in evidence.glob('history-*.json')]
    assert len(reports)==5 and len(list(evidence.glob('bundle-*.json')))==2
    assert {r['slot'] for r in reports}==set(range(1,6))
    assert all(any(c['event']['kind']=='fill_encoded' for c in r['candidates']) for r in reports)
    print('PASS: actual first-producer capture writes five binding histories; later captures remain bounded')
print('PASS: history and real guard hooks under ASan/UBSan; synthetic only, no GPU/game validation')
