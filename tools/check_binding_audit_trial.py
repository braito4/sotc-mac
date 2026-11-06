"""Selected descriptor identity and actual guest/backing-view audit, synthetic only."""
import ast,json,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
GPU=r'''
#include "gpu_binding_audit.h"
#include <cassert>
#include <thread>
using namespace SotcBindingAudit;
static unsigned calls;
extern "C" void sotc_buffer_binding_audit_v1(unsigned seq,unsigned slot,unsigned phase,U64 guest,U64 bytes){
 assert(seq==1&&slot==2&&phase==0&&guest==4096&&bytes==32);++calls;
}
int main(){
 SotcBufferHistory::Register(10,100,16,256);
 unsigned char sharp[16];for(unsigned i=0;i<16;++i)sharp[i]=(unsigned char)i;
 Bound(42,1,1,4096,32,32,false,10,32,40,8,sharp);
 auto r=Find(1,100,48,40);assert(r.sequence&&r.guest==4096&&r.adjust==8&&r.sharp[15]==15);
 assert(!Find(1,100,49,40).sequence&&!Find(2,100,48,40).sequence);
 std::thread other([]{assert(!Find(1,100,48,40).sequence);});other.join();
 Sample(1,2,0,r);assert(calls==1);
 std::ostringstream out;Json(out,r);assert(out.str().find("000102030405060708090a0b0c0d0e0f")!=std::string::npos);
 // A newer nonmatching selected binding invalidates the older exact one.
 Bound(42,1,1,8192,32,32,false,10,96,40,8,sharp);assert(!Find(1,100,48,40).sequence);
 Bound(41,1,1,4096,32,32,false,10,32,40,8,sharp);assert(!Find(1,100,48,40).sequence);
 SotcBufferHistory::Remove(10);SotcBufferHistory::Register(10,100,16,256);assert(!Find(1,100,112,40).sequence);
 Registry bounded;Record record;record.thread=1;record.unified=1;record.native={100,1,0,64,0,0,false};bounded.Add(record);
 record.unified=2;for(unsigned i=0;i<64;++i)bounded.Add(record);assert(!bounded.Find(1,1,record.native).sequence);
}
'''
def assignment(path,name):
 return next(ast.literal_eval(n.value) for n in ast.parse(path.read_text()).body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets))
with tempfile.TemporaryDirectory(prefix='binding-audit-fixture-',dir=ROOT/'work') as tmp:
 p=Path(tmp);(p/'gpu.cpp').write_text(GPU)
 subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-Wl,-export_dynamic','-I'+str(ROOT/'runtime/src'),str(p/'gpu.cpp'),'-o',str(p/'gpu')],check=True)
 subprocess.run([str(p/'gpu')],env=dict(os.environ,SOTC_BUFFER_HISTORY='1',SOTC_BINDING_AUDIT='1',SOTC_BINDING_AUDIT_SHADER='42'),check=True)
 print('PASS: selected descriptor raw bytes, native base/epoch/extents, newest-binding rejection, thread isolation, retention and host-export bridge under ASan/UBSan')
 original=ROOT/'tools/check_page_history_trial.py';host=assignment(original,'HOST')
 needle=' assert(!memcmp((char*)alias+8,data,32));';assert needle in host
 host=host.replace(needle,needle+r'''
 if(enabled){
   errno=ERANGE;sotc_buffer_binding_audit_v1(1,2,0,(uintptr_t)a,2*PAGE);assert(errno==ERANGE);
   ((unsigned char*)a)[8]=0xa7;sotc_buffer_binding_audit_v1(1,2,1,(uintptr_t)a,2*PAGE);
   assert(!kernel_mprotect(alias,PAGE,0));
   sotc_buffer_binding_audit_v1(2,2,0,(uintptr_t)alias,PAGE);
   assert(!kernel_mprotect(alias,PAGE,3));
   // Deliberately give a synthetic alias different contents from the backend's VMA.
   void *probe=NULL;assert(!direct_map(&probe,PAGE,3,0,physical,PAGE));
   assert(mmap(probe,PAGE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)==probe);
   memset(probe,0x66,PAGE);sotc_buffer_binding_audit_v1(3,2,0,(uintptr_t)probe,PAGE);
   assert(!direct_unmap(probe,PAGE));
   sotc_buffer_binding_audit_v1(4,2,0,(uintptr_t)probe,PAGE); // gap, no unsafe read
   // Duplicate file must not overwrite the original before sample.
   sotc_buffer_binding_audit_v1(1,2,0,(uintptr_t)a,2*PAGE);
   ((unsigned char*)a)[8]=0x53;
 }
''')
 tree=ast.parse(original.read_text())
 for n in tree.body:
  if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='HOST' for t in n.targets):n.value=ast.Constant(host)
 ast.fix_missing_locations(tree);source=ast.unparse(tree).replace('ROOT = Path(__file__).resolve().parents[1]','ROOT = Path('+repr(str(ROOT))+')')
 source=source.replace("assert len(list(evidence.glob('*.json'))) == 6", "assert len(list(evidence.glob('page-history-*.json'))) == 6"+'''
            audits={f.name:json.loads(f.read_text()) for f in evidence.glob('binding-audit-*.json')}
            assert len(audits)==5
            first=audits['binding-audit-1-slot-2-before.json']['samples'][0]
            later=audits['binding-audit-1-slot-2-after.json']['samples'][0]
            assert first['guest_read']==first['backing_read']==1 and first['guest_hex']==first['backing_hex']
            assert bytes.fromhex(first['guest_hex'])[8]==0x53 and bytes.fromhex(later['guest_hex'])[8]==0xa7
            protected=audits['binding-audit-2-slot-2-before.json']['samples'][0]
            assert protected['guest_read']==0 and protected['backing_read']==1 and protected['prot']==0
            mismatch=audits['binding-audit-3-slot-2-before.json']['samples'][0]
            assert mismatch['guest_read']==mismatch['backing_read']==1 and mismatch['guest_hex']!=mismatch['backing_hex']
            gap=audits['binding-audit-4-slot-2-before.json']['samples'][0]
            assert not gap['guest_read'] and not gap['backing_read']
''')
 script=p/'host-check.py';script.write_text(source)
 subprocess.run(['python3',str(script)],env=dict(os.environ,SOTC_BINDING_AUDIT='1',SOTC_PAGE_HISTORY_TEST_HOST=str(ROOT/'work/host-binding-audit-20261106')),check=True)
 print('PASS: real host equal/changed/mismatched guest/backing views, unreadable protected alias, gap, errno and exclusive private files')
 # Exercise actual producer Capture/Finish: five before calls and five after calls,
 # matched through native ranges, with raw descriptor metadata written to the bundle.
 base=assignment(ROOT/'tools/check_scatter_guard_trial.py','SOURCE')
 tree=ast.parse((ROOT/'tools/check_producer_bundle_trial.py').read_text())
 expr=next(n.value for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='SOURCE' for t in n.targets))
 source=eval(compile(ast.Expression(expr),'<synthetic fixture>','eval'),{'base':base})
 source=source.replace('int main(int argc,char** argv)',r'''static unsigned before_calls,after_calls;
extern "C" void sotc_buffer_binding_audit_v1(unsigned seq,unsigned slot,unsigned phase,uint64_t guest,uint64_t bytes){
 assert(seq==1&&slot>=1&&slot<=5&&guest==4096*slot&&bytes==128);
 if(phase)++after_calls;else ++before_calls;
}
int main(int argc,char** argv)''')
 needle='    auto& output=o.compute_bindings[4].elements[0];'
 source=source.replace(needle,r'''    for(unsigned i=1;i<=5;++i){
        auto& b=o.compute_bindings[i-1].elements[0];
        const auto identity=reinterpret_cast<uintptr_t>((__bridge void*)b.buffer);
        SotcBufferHistory::Register(200+i,identity,0,b.buffer.length);
        unsigned char raw[16]={};raw[0]=(unsigned char)i;
        SotcBindingAudit::Bound(42,i-1,i-1,4096*i,128,128,i>=4,200+i,16,128,0,raw);
    }
'''+needle)
 source=source.replace('        assert(committed==(active&&i<32));','        assert(before_calls==5&&after_calls==5);\n        assert(committed==(active&&i<32));')
 (p/'bundle.mm').write_text(source);out=p/'bundle';out.mkdir()
 subprocess.run(['clang++','-std=c++20','-fobjc-arc','-fblocks','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-Wl,-export_dynamic','-framework','Foundation','-framework','Metal','-I'+str(ROOT/'runtime/src'),str(p/'bundle.mm'),'-o',str(p/'bundle-test')],check=True)
 subprocess.run([str(p/'bundle-test'),'direct'],env=dict(os.environ,SOTC_BUFFER_HISTORY='1',SOTC_BINDING_AUDIT='1',SOTC_BINDING_AUDIT_SHADER='42',SOTC_FRAGMENT_PROVENANCE='1',SOTC_PRODUCER_BUNDLE='fixture,5,4,64',SOTC_SCATTER_GUARD_DIR=str(out)),check=True)
 meta=json.loads((out/'bundle-1.json').read_text());assert all(b['binding_audit']['matched'] for b in meta['bindings'])
 assert [b['binding_audit']['guest'] for b in meta['bindings']]==[4096*i for i in range(1,6)]
 print('PASS: actual producer before/after audit bridge and all five descriptor associations under ASan/UBSan; fake Metal only')

print('PASS: binding audit diagnostics; no game or native GPU execution')
