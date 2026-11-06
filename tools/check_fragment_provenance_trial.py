"""Synthetic provenance identity/coverage tests and actual guard failure hook."""
import ast
import os
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
SOURCE=r'''
#include "gpu_fragment_provenance.h"
#include <cassert>
extern void OtherTranslationUnit();
int main() {
 using namespace SotcFragment;
 Registry r(3,2);r.Register(1,101,16);
 Copy a;a.guest=4096;a.bytes=64;a.backend=1;a.offset=32;a.written=true;a.shader=42;
 r.Add(a);Copy b=a;b.offset=128;b.written=false;r.Add(b);
 auto m=r.Find(101,144,64);assert(m.found&&m.complete&&m.copy.sequence==2);
 assert(m.prior_writes.size()==1&&m.prior_writes[0].shader==42);
 Copy overwrite=b;overwrite.offset=132;overwrite.bytes=4;overwrite.guest=8192;r.Add(overwrite);
 m=r.Find(101,144,64);assert(m.found&&!m.complete&&m.copy.sequence==3);
 r.Remove(1);assert(!r.Find(101,144,64).found);
 r.Register(1,101,16);assert(!r.Find(101,144,64).found); // Same addresses, new object registration.
 r.Add(b);m=r.Find(101,144,64);assert(m.complete&&m.copy.sequence==4&&m.evicted==1);
 r.Register(2,102,0);r.Register(3,103,0);Copy missing=b;missing.backend=3;r.Add(missing);
 assert(!r.Find(103,128,64).found&&r.Find(101,144,64).alias_omitted==1);
 Copy overflow=b;overflow.offset=UINT64_MAX;r.Add(overflow);
 assert(r.Find(101,144,64).copy.sequence==4);
 assert(Overlap(UINT64_MAX-3,4,UINT64_MAX-1,1));
 assert(!Overlap(8,4,12,4)&&!Overlap(0,0,0,8));
 Registry none(0,0);none.Register(1,101,0);none.Add(a);
 assert(none.Find(101,32,64).evicted==1&&!none.Find(101,32,64).found);
 U64 generation=2;assert(Generation(&generation)==2&&Generation(nullptr)==UINT64_MAX);
 setenv("SOTC_FRAGMENT_PROVENANCE","1",1);
 OtherTranslationUnit(); // Inline registry must be shared across renderer compilation units.
 assert(Get().Find(201,0,32).found);
 std::uint32_t data[8]={1,2,3,4,5,6,7,8};
 Register(300,301,0);
 const auto query=[](uintptr_t at,U64* physical,uintptr_t* end){*physical=at+4096;*end=at+32;return true;};
 Copied(4096,32,300,0,9,false,42,2,data,2,&generation,query);
 Report(2,301,8,16,data+2,16); // Exact offset within captured prefix.
 data[2]=99;Report(2,301,8,16,data+2,16); // Mutation must be visible, not mislabeled equal.
 Report(2,999,0,16,data,16);
 return 0;
}
'''
OTHER=r'''
#include "gpu_fragment_provenance.h"
void OtherTranslationUnit(){
 SotcFragment::Get().Register(200,201,0);SotcFragment::Copy c;
 c.backend=200;c.bytes=32;SotcFragment::Get().Add(c);
}
'''

with tempfile.TemporaryDirectory(dir=ROOT/'work') as tmp:
    p=Path(tmp);(p/'main.cpp').write_text(SOURCE);(p/'other.cpp').write_text(OTHER)
    flags=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
           '-I'+str(ROOT/'runtime/src')]
    subprocess.run(flags+[str(p/'main.cpp'),str(p/'other.cpp'),'-o',str(p/'test')],check=True)
    result=subprocess.run([str(p/'test')],capture_output=True,text=True)
    assert result.returncode==0,result.stderr
    assert 'compared_bytes=16 equal=1' in result.stderr
    assert 'compared_bytes=16 equal=0' in result.stderr
    assert 'lookup=unavailable' in result.stderr
    print('PASS: registration reuse, partial overwrite, history/live limits, offsets, overflow, prefix comparison and shared registry across translation units')
    tree=ast.parse((ROOT/'tools/check_scatter_guard_trial.py').read_text())
    source=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign)
                and any(isinstance(t,ast.Name) and t.id=='SOURCE' for t in n.targets))
    source=source.replace('gpu_scatter_guard_metal.h','gpu_scatter_provenance_guard.h')
    source=source.replace('SotcScatterGuard::Scope','SotcFragment::Scope')
    source=source.replace('auto args=[device newBufferWithLength:32',r'''
    const auto identity=reinterpret_cast<std::uintptr_t>((__bridge void*)index.buffer);
    SotcFragment::Register(500,identity,0);
    SotcFragment::U64 generation=2;
    const auto map=[](uintptr_t at,SotcFragment::U64* physical,uintptr_t* end){*physical=at;*end=at+16384;return true;};
    SotcFragment::Copied(4096,index.size,500,index.offset,1,true,7,0,
        (char*)index.buffer.contents+index.offset,2,&generation,map);
    SotcFragment::Copied(4096,index.size,500,index.offset,2,false,42,2,
        (char*)index.buffer.contents+index.offset,2,&generation,map);
    auto args=[device newBufferWithLength:32''')
    (p/'guard.mm').write_text(source)
    subprocess.run(flags+['-fobjc-arc','-fblocks','-framework','Foundation','-framework','Metal',
                          str(p/'guard.mm'),'-o',str(p/'guard')],check=True)
    for mode in ('direct','indirect','invalid'):
        evidence=p/mode;evidence.mkdir()
        env=dict(os.environ,SOTC_FRAGMENT_PROVENANCE='1',SOTC_SCATTER_CACHE_MB='16',
                 SOTC_SCATTER_GUARD='fixture,1,8,80,2,81,10,89,8,13,92,12,64',
                 SOTC_SCATTER_GUARD_DIR=str(evidence))
        result=subprocess.run([str(p/'guard'),mode],env=env,capture_output=True,text=True)
        assert result.returncode==(23 if mode=='invalid' else 0),(mode,result.stderr)
        if mode=='invalid':
            assert result.stderr.count('Fragment provenance:')==4
            assert 'lookup=unavailable' in result.stderr
            assert 'lookup=matched full_span=1' in result.stderr
            assert 'compared_bytes=20 equal=1' in result.stderr
            assert 'role=prior_writable_fallback' in result.stderr
            assert 'SIMULATED_DISPATCH_REACHED' not in result.stdout
            assert (evidence/'capture-1.bin').is_file()
        else:assert 'Fragment provenance:' not in result.stderr
        print('PASS: guard failure hook',mode)
print('PASS: actual provenance and guard checks under ASan/UBSan; no native GPU execution')
