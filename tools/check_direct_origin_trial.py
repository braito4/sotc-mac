"""Direct producer capture and validated import-boundary observations; no game/GPU."""
import ast,json,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
UNIT=r'''
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
static int bbcpu_is_guest_code(uint64_t at){return at>=4096&&at<8192;}
#include "cpu_import_origin.h"
static void* other(void* unused){(void)unused;SotcImportOrigin out;assert(!sotc_cpu_import_origin_v1(9000,1,&out));return 0;}
int main(void){
 assert(sotc_import_trace_enabled());SotcImportOrigin out;
 uint64_t ret=4112,rsp=(uintptr_t)&ret;errno=EDOM;
 SotcImportOrigin empty=sotc_import_enter(9000,rsp,rsp+8,9000);
 assert(errno==EDOM&&sotc_cpu_import_origin_v1(9000,rsp,&out));
 assert(out.guest_return==4112&&!out.guest_instruction&&out.return_status==3);
 assert(!sotc_cpu_import_origin_v1(9001,rsp,&out)&&!out.active);
 pthread_t t;assert(!pthread_create(&t,0,other,0)&&!pthread_join(t,0));
 SotcImportOrigin outer=sotc_import_enter(4120,rsp,rsp,9008);
 assert(sotc_cpu_import_origin_v1(4120,rsp,&out)&&out.guest_instruction==4120&&!out.guest_return&&out.return_status==0);
 sotc_import_leave(outer);assert(sotc_cpu_import_origin_v1(9000,rsp,&out)&&out.guest_return==4112);
 sotc_import_leave(empty);assert(!sotc_cpu_import_origin_v1(9000,rsp,&out));
 empty=sotc_import_enter(9000,1,9,9000);assert(sotc_cpu_import_origin_v1(9000,1,&out)&&!out.guest_return&&out.return_status==1);sotc_import_leave(empty);
 ret=0xdeadbeef;empty=sotc_import_enter(9000,rsp,rsp+8,9000);assert(sotc_cpu_import_origin_v1(9000,rsp,&out)&&out.return_status==2&&!out.guest_return);sotc_import_leave(empty);
 empty=sotc_import_enter(4100,rsp,rsp+8,9000);assert(sotc_cpu_import_origin_v1(4100,rsp,&out)&&out.guest_instruction==4100&&!out.guest_return);sotc_import_leave(empty);
}
'''
def assignment(path,name):
 tree=ast.parse(path.read_text())
 return next(n.value for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets))
with tempfile.TemporaryDirectory(prefix='direct-origin-fixture-',dir=ROOT/'work') as tmp:
 p=Path(tmp);(p/'origin.c').write_text(UNIT)
 subprocess.run(['clang','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-pthread','-I'+str(ROOT/'runtime/src'),str(p/'origin.c'),'-o',str(p/'origin')],check=True)
 subprocess.run([str(p/'origin')],env=dict(os.environ,SOTC_PAGE_HISTORY='1'),check=True)
 print('PASS: nested/TLS origin restoration, state mismatch rejection, interpreter CALL vs pushed return, safe unreadable/non-guest returns and errno under ASan/UBSan')
 base=ast.literal_eval(assignment(ROOT/'tools/check_scatter_guard_trial.py','SOURCE'))
 source=eval(compile(ast.Expression(assignment(ROOT/'tools/check_producer_bundle_trial.py','SOURCE')),'<original synthetic fixture>','eval'),{'base':base})
 source=source.replace('bool active=mode!="disabled"&&mode!="unselected"&&mode!="notarmed"&&mode!="readonly"&&mode!="reused";','bool active=mode!="disabled"&&mode!="unselected"&&mode!="toosmall";')
 (p/'bundle.mm').write_text(source)
 subprocess.run(['clang++','-std=c++20','-fobjc-arc','-fblocks','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-framework','Foundation','-framework','Metal','-I'+str(ROOT/'runtime/src'),str(p/'bundle.mm'),'-o',str(p/'bundle')],check=True)
 for mode in ('notarmed','readonly','reused','direct','indirect','budget','disabled','unselected','toosmall'):
  d=p/mode;d.mkdir();env=dict(os.environ,SOTC_PRODUCER_DIRECT='1',SOTC_FRAGMENT_PROVENANCE='1',SOTC_PRODUCER_BUNDLE='fixture,5,4,256' if mode=='toosmall' else 'fixture,5,4,64',SOTC_SCATTER_GUARD_DIR=str(d))
  if mode=='disabled':env.pop('SOTC_PRODUCER_BUNDLE')
  result=subprocess.run([str(p/'bundle'),mode],env=env,capture_output=True,text=True);assert result.returncode==0,(mode,result.stderr)
  files=list(d.glob('bundle-*.json'));expected=0 if mode in ('disabled','unselected','toosmall') else 32 if mode=='budget' else 1;assert len(files)==expected
  if files:
   m=json.loads((d/'bundle-1.json').read_text());assert m['fragment_prerequisite_disabled'] and m['completed']
   assert any(c['changed_bytes']>0 for c in m['comparisons'])
   if mode in ('notarmed','readonly','reused'):assert not m['output_fallback_observed']
  print('PASS: producer capture without fragmented arming',mode)
 # Reuse real host fixture, replacing tail-jump-only entry with a real CALL and continuation.
 original=(ROOT/'tools/check_page_history_trial.py').read_text()
 host=ast.literal_eval(assignment(ROOT/'tools/check_page_history_trial.py','HOST'))
 old='bridge[0]=0x48;bridge[1]=0xb8;uintptr_t target=(uintptr_t)copy;memcpy(bridge+2,&target,8);bridge[10]=0xff;bridge[11]=0xe0;'
 new='const unsigned char head[]={0x48,0x83,0xec,8,0x48,0xb8};memcpy(bridge,head,sizeof(head));uintptr_t target=(uintptr_t)copy;memcpy(bridge+6,&target,8);const unsigned char tail[]={0xff,0xd0,0x48,0x83,0xc4,8,0xc3};memcpy(bridge+14,tail,sizeof(tail));'
 assert old in host;host=host.replace(old,new)
 # Validate precise instruction/continuation in the actual ledger, not merely a nonzero RIP.
 needle=' sotc_buffer_page_history_dump_v1(10,2,0,(uint64_t)physical,2*PAGE);'
 check=r'''
 if(enabled){
   const SotcPageBucket *bucket=sotc_page_ledger.buckets+((uint64_t)physical>>SOTC_PAGE_BUCKET_SHIFT);int found=0;
   for(unsigned j=0;j<4;++j){const SotcPageEvent *e=bucket->writes+j;
     if(e->kind!=SOTC_PAGE_MEMCPY||e->host_target!=(uintptr_t)copy)continue;
     if(e->guest_return==(uintptr_t)bridge+16&&e->return_status==3)found=1;
     if(e->guest_instruction==(uintptr_t)bridge+14&&e->return_status==0)found=1;
   }assert(found);
 }
'''
 host=host.replace(needle,check+needle)
 # Execute the existing runner with its HOST constant changed in an AST, preserving all real hooks.
 tree=ast.parse(original)
 for node in tree.body:
  if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='HOST' for t in node.targets):node.value=ast.Constant(host)
 ast.fix_missing_locations(tree)
 script=p/'host-check.py';script.write_text(ast.unparse(tree).replace('ROOT = Path(__file__).resolve().parents[1]','ROOT = Path('+repr(str(ROOT))+')'))
 for jit in ('1','0'):
  env=dict(os.environ,BB_JIT=jit,SOTC_PAGE_HISTORY_TEST_HOST=str(ROOT/'work/host-direct-origin-20261106'))
  subprocess.run(['python3',str(script)],env=env,check=True)
  print('PASS: real guest CALL origin plus complete host page-history regressions; BB_JIT='+jit)
print('PASS: direct-output/import-boundary trial; no game or native GPU execution')
