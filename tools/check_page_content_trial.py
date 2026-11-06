"""Original bounded content and physical writer fixtures; no game or native GPU."""
import ast,json,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
GPU=r'''
#include "gpu_buffer_history.h"
#include <cassert>
#include <thread>
using namespace SotcBufferHistory;
int main(){
 const U64 page=1ull<<18;
 Registry r(16,32,true);r.Register(1,101,0,4*page);r.Pool(1,0,4*page);
 r.NativeWrite(Kind::ImageDownload,10,0,0,101,32,64,77);
 for(unsigned i=0;i<1000000;++i)r.NativeWrite(Kind::Fill,i,0,0,101,2*page,8,1);
 for(unsigned i=0;i<100;++i)r.Binding(1,32,64,4096,42,7,i);
 auto v=r.Inspect(101,32,64);assert(v.physical_enabled&&v.physical_hits.size()==5);
 unsigned downloads=0;for(const auto& h:v.physical_hits)if(h.event.kind==Kind::ImageDownload){++downloads;assert(h.event.command==10&&h.event.value==77);}
 assert(downloads==1);assert(v.physical_coverage[0].evicted==96);
 assert(r.Inspect(101,96,32).physical_hits.empty()); // same bucket, disjoint exact range
 // Shared pool alias matches; reimported pool epoch cannot inherit old writers.
 r.Register(2,102,0,page);r.Pool(2,0,page);assert(r.Inspect(102,32,64).physical_hits.size()==5);
 r.Remove(1);r.Remove(2);r.Register(1,101,0,page);r.Pool(1,0,page);
 assert(r.Inspect(101,32,64).physical_hits.empty());
 // Archived candidates must predate the copy that created a source node.
 Registry chain(8,16,true);chain.Register(1,201,0,page);chain.Pool(1,0,page);chain.Register(2,202,0,128);
 chain.NativeWrite(Kind::Fill,1,0,0,201,32,32,11);
 for(unsigned i=0;i<20;++i)chain.NativeWrite(Kind::Fill,i,0,0,201,128,8);
 chain.NativeWrite(Kind::Copy,2,201,32,202,0,32);
 chain.NativeWrite(Kind::Fill,3,0,0,201,32,32,22);
 v=chain.Inspect(202,0,32);unsigned prior=0;
 for(const auto& h:v.physical_hits){assert(h.node==1&&h.event.value!=22);if(h.event.value==11)++prior;}
 // Same-bucket writes can evict the old fill: explicit, not fabricated attribution.
 assert(prior==0&&v.physical_coverage[0].evicted>0);
 Registry cutoff(8,16,true);cutoff.Register(1,201,0,page);cutoff.Pool(1,0,page);cutoff.Register(2,202,0,128);
 cutoff.NativeWrite(Kind::Fill,1,0,0,201,32,32,11);cutoff.NativeWrite(Kind::Copy,2,201,32,202,0,32);
 cutoff.NativeWrite(Kind::Fill,3,0,0,201,32,32,22);v=cutoff.Inspect(202,0,32);
 assert(v.physical_hits.size()==1&&v.physical_hits[0].event.value==11&&v.physical_hits[0].node==1);
 // One crossing event appears once per node, not once for each bucket.
 Registry cross(4,4,true);cross.Register(1,1,0,2*page);cross.Pool(1,0,2*page);
 cross.NativeWrite(Kind::Update,1,0,0,1,page-8,16);v=cross.Inspect(1,page-4,8);
 assert(v.physical_hits.size()==1&&v.physical_hits[0].overlap_bytes==8&&v.physical_coverage[0].buckets==2);
 // Event span and report span budgets remain explicit.
 Registry bounded(4,4,true);bounded.Register(1,1,0,1025*page);bounded.Pool(1,0,1025*page);
 bounded.NativeWrite(Kind::Fill,1,0,0,1,0,1025*page);v=bounded.Inspect(1,0,1025*page);
 assert(v.physical_bucket_omitted==1&&v.physical_coverage[0].omitted==769&&v.physical_buckets==1024);
 assert(bounded.Inspect(1,1024*page,8).physical_coverage[0].missing==1);
 Registry disabled;disabled.Register(1,1,0,64);disabled.Pool(1,0,64);disabled.NativeWrite(Kind::Fill,1,0,0,1,0,64);
 v=disabled.Inspect(1,0,64);assert(!v.physical_enabled&&!v.physical_buckets&&v.physical_hits.empty());
 // Exercise JSON serialization of actual archived events through the global registry.
 Register(1,1,0,page);Pool(1,0,page);NativeWrite(Kind::ImageDownload,8,0,0,1,32,16,99);Dump(2,1,32,16);
}
'''
HELPER=r'''
static void expect_sample(unsigned kind,uint64_t physical,unsigned char value) {
 if(!sotc_page_content)return;
 const SotcPageBucket *bucket=sotc_page_ledger.buckets+(physical>>SOTC_PAGE_BUCKET_SHIFT);
 const SotcPageEvent *found=NULL;
 for(unsigned j=0;j<6;++j){const SotcPageEvent *e=j<4?bucket->writes+j:bucket->lifecycle+j-4;
  if(e->kind==kind&&e->sample_status==1&&e->sample_physical==physical&&(!found||found->sequence<e->sequence))found=e;
 }
 assert(found&&found->sample_bytes&&found->sample[0]==value&&found->sample_end_ns>=found->sample_begin_ns);
}
'''
def assignment(path,name):
 tree=ast.parse(path.read_text())
 return next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets))
with tempfile.TemporaryDirectory(prefix='page-content-fixture-',dir=ROOT/'work') as tmp:
 p=Path(tmp);(p/'gpu.cpp').write_text(GPU)
 subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(ROOT/'runtime/src'),str(p/'gpu.cpp'),'-o',str(p/'gpu')],check=True)
 subprocess.run([str(p/'gpu')],env=dict(os.environ,SOTC_BUFFER_HISTORY='1',SOTC_GPU_PAGE_HISTORY='1',SOTC_SCATTER_GUARD_DIR=str(p)),check=True)
 d=json.loads((p/'history-1-slot-2.json').read_text());assert d['schema']==3 and d['physical_history']['enabled']
 assert d['physical_history']['candidates'][0]['event']['value']==99
 print('PASS: physical archive preserves old transfer through million unrelated events; intent isolation, exact spans, aliases/epochs, copy cutoffs, budgets and JSON under ASan/UBSan')
 original=ROOT/'tools/check_page_history_trial.py';host=assignment(original,'HOST')
 host=host.replace('int main(int argc,char**argv)',HELPER+'\nint main(int argc,char**argv)')
 host=host.replace(' unsigned char data[32];',r'''
 if(enabled){
  expect_sample(SOTC_PAGE_MAP,physical,0);
  SotcPageEvent sample={0};sample.kind=SOTC_PAGE_MAP;unsigned budget=0;
  sotc_page_sample(&sample,physical,physical+64,&budget);assert(sample.sample_status==3&&!sample.sample_bytes);
  sample=(SotcPageEvent){0};sample.kind=SOTC_PAGE_MAP;budget=1;
  sotc_page_sample(&sample,POOL_SIZE+FLEX_SPAN,POOL_SIZE+FLEX_SPAN+1,&budget);assert(sample.sample_status==2&&!sample.sample_bytes);
  sample=(SotcPageEvent){0};sample.kind=SOTC_PAGE_MAP;budget=1;
  sotc_page_sample(&sample,physical,physical+3,&budget);assert(sample.sample_status==1&&sample.sample_bytes==3);
 }
 unsigned char data[32];''')
 host=host.replace(' assert(!memcmp((char*)alias+8,data,32));',' assert(!memcmp((char*)alias+8,data,32));if(enabled)expect_sample(SOTC_PAGE_MEMCPY,physical+8,0x53);')
 host=host.replace(' uint64_t word=0x1234;', ' if(enabled)expect_sample(SOTC_PAGE_MEMSET,physical+8,0x7c);\n uint64_t word=0x1234;')
 host=host.replace(' assert(!direct_unmap(a,2*PAGE));', ' ((unsigned char*)a)[0]=0xa7; /* direct store has no import hook */\n assert(!direct_unmap(a,2*PAGE));')
 host=host.replace(' sotc_buffer_page_history_dump_v1(3,2,0,', ' if(enabled){expect_sample(SOTC_PAGE_UNMAP,physical,0xa7);expect_sample(SOTC_PAGE_MAP,physical,0xa7);}\n sotc_buffer_page_history_dump_v1(3,2,0,')
 host=host.replace(' sotc_buffer_page_history_dump_v1(5,2,0,',' if(enabled)expect_sample(SOTC_PAGE_ZERO,physical,0);\n sotc_buffer_page_history_dump_v1(5,2,0,')
 tree=ast.parse(original.read_text())
 for n in tree.body:
  if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='HOST' for t in n.targets):n.value=ast.Constant(host)
 ast.fix_missing_locations(tree);source=ast.unparse(tree).replace('ROOT = Path(__file__).resolve().parents[1]','ROOT = Path('+repr(str(ROOT))+')')
 needle="assert len(list(evidence.glob('*.json'))) == 6";assert needle in source
 source=source.replace(needle,needle+'''
            samples=[e['sample'] for r in reports for b in r['buckets'] for e in b['events']]
            assert all(r['schema']==3 for r in reports)
            if os.environ.get('SOTC_PAGE_CONTENT')=='1':
                assert any(s['status']==1 and s['bytes'] and len(s['hex'])==s['bytes']*2 for s in samples)
            else: assert all(s['status']==0 and not s['bytes'] and not s['hex'] for s in samples)
''')
 script=p/'host-check.py';script.write_text(source)
 for content in ('1','0'):
  subprocess.run(['python3',str(script)],env=dict(os.environ,SOTC_PAGE_CONTENT=content,SOTC_PAGE_HISTORY_TEST_HOST=str(ROOT/'work/host-page-content-20261106')),check=True)
  print('PASS: actual host sampling map/unmap/reuse, direct-store observation, import/backing writes, zero release, serialization, errno and disabled content='+content)
print('PASS: page-content diagnostics; no game or native GPU execution')
