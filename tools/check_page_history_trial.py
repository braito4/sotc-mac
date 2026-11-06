"""Original physical ledger fixtures plus actual host memory/libc/file hooks; no game/GPU."""
from pathlib import Path
import os,re,json,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
CORE=r'''
#include "buffer_page_history_core.h"
#include <assert.h>
int main(void) {
 SotcPageLedger l={0};assert(sotc_page_ledger_init(&l,3*SOTC_PAGE_BUCKET_SIZE));
 SotcPageEvent e={0};e.kind=3;e.physical=SOTC_PAGE_BUCKET_SIZE-8;e.bytes=16;e.guest=4096;
 sotc_page_ledger_add(&l,e);assert(l.buckets[0].write_count==1&&l.buckets[1].write_count==1);
 assert(l.buckets[1].writes[0].physical==e.physical); // exact span, not whole bucket
 // Heavy traffic elsewhere cannot evict the source region's provenance.
 e.physical=2*SOTC_PAGE_BUCKET_SIZE;e.bytes=4;
 for(unsigned i=0;i<1000000;++i)sotc_page_ledger_add(&l,e);
 assert(l.buckets[0].writes[0].sequence==1&&l.buckets[0].write_count==1);
 assert(l.buckets[2].write_count==1000000);
 e.physical=8;e.kind=1;sotc_page_ledger_add(&l,e);e.kind=2;sotc_page_ledger_add(&l,e);e.kind=1;sotc_page_ledger_add(&l,e);
 assert(l.buckets[0].lifecycle_count==3&&l.buckets[0].lifecycle[0].kind==1&&l.buckets[0].lifecycle[1].kind==2);
 assert(l.buckets[0].writes[0].sequence==1); // prior-lifetime writer retained with older sequence
 assert(!sotc_page_overlap(8,4,16,4)&&sotc_page_overlap(8,4,10,4));
 e.physical=UINT64_MAX-1;e.bytes=4;sotc_page_ledger_add(&l,e);assert(l.invalid==1);
 e.physical=3*SOTC_PAGE_BUCKET_SIZE;e.bytes=8;sotc_page_ledger_add(&l,e);assert(l.omitted==1);
 free(l.buckets);SotcPageLedger no={0};assert(!sotc_page_ledger_init(&no,(UINT64_C(33)<<30)));
 return 0;
}
'''
HOST=r'''
#include "runtime_memory_arm64.c"
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include <assert.h>
int main(int argc,char**argv) {
 assert(argc==2);const int enabled=!strcmp(argv[1],"enabled");
 bootstrap_initialize(1);
 int64_t physical;assert(!direct_allocate(0,GAME_POOL_SIZE,2*PAGE,PAGE,3,&physical));
 void *a=NULL,*alias=NULL;assert(!direct_map(&a,2*PAGE,3,0,physical,PAGE));
 assert(!direct_map(&alias,2*PAGE,3,0,physical,PAGE));
 unsigned char data[32];memset(data,0x53,sizeof(data));
 void*(*copy)(void*,const void*,size_t)=(void*)bootstrap_resolve("Q3VBxCXhUHs#1#u",0);
 void*(*fill)(void*,int,size_t)=(void*)bootstrap_resolve("8zTFvBIAIN8#1#u",0);assert(copy&&fill);
 errno=ERANGE;assert(copy((char*)a+8,data,32)==(char*)a+8&&errno==ERANGE);
 assert(!memcmp((char*)alias+8,data,32));
 unsigned char *bridge=runtime_low_map(16384,PROT_READ|PROT_WRITE);assert(bridge);
 bridge[0]=0x48;bridge[1]=0xb8;uintptr_t target=(uintptr_t)copy;memcpy(bridge+2,&target,8);bridge[10]=0xff;bridge[11]=0xe0;
 bbcpu_add_guest_code((uintptr_t)bridge,16384);uint64_t args[3]={(uintptr_t)a+8,(uintptr_t)data,32};
 assert(bbcpu_call((uintptr_t)bridge,3,args)==(uintptr_t)a+8);
 sotc_buffer_page_history_dump_v1(10,2,0,(uint64_t)physical,2*PAGE);
 assert(fill((char*)a+8,0x17c,32)==(char*)a+8);assert(((unsigned char*)alias)[8]==0x7c);
 uint64_t word=0x1234;errno=ERANGE;assert(runtime_memory_write_backing((uintptr_t)alias+64,&word,8)&&errno==ERANGE);
 assert(*(uint64_t*)((char*)a+64)==word);
 sotc_buffer_page_history_dump_v1(1,2,0,(uint64_t)physical,2*PAGE);
 // Partial backing failure: first page writes are reported, the unmapped tail is not invented.
 assert(!direct_unmap((char*)alias+PAGE,PAGE));
 unsigned char *large=malloc(2*PAGE);assert(large);memset(large,0x91,2*PAGE);
 assert(!runtime_memory_write_backing((uintptr_t)alias,large,2*PAGE));free(large);
 sotc_buffer_page_history_dump_v1(2,2,0,(uint64_t)physical,2*PAGE);
 // Re-map the same physical page under a new guest mapping; old writes retain older sequences.
 assert(!direct_unmap(a,2*PAGE));void *b=NULL;assert(!direct_map(&b,2*PAGE,3,0,physical,PAGE));
 sotc_buffer_page_history_dump_v1(3,2,0,(uint64_t)physical,2*PAGE);
 char folder[]="/tmp/sotc-page-file-XXXXXX",path[1024];assert(mkdtemp(folder));
 snprintf(path,sizeof(path),"%s/input.bin",folder);int file=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);assert(file>=0);
 assert(write(file,"abcdef",6)==6&&close(file)==0);assert(bootstrap_mount_game_root(folder)==0);
 int64_t(*open_guest)(const char*,int,int)=(void*)bootstrap_resolve("1G3lF1Gg1k8#0#F",0);
 int64_t(*pread_guest)(int,void*,uint64_t,int64_t)=(void*)bootstrap_resolve("+r3rMFwItV4#0#F",0);
 int64_t(*close_guest)(int)=(void*)bootstrap_resolve("UK2Tl2DWUns#0#F",0);assert(open_guest&&pread_guest&&close_guest);
 int fd=(int)open_guest("/app0/input.bin",0,0);assert(fd>=3);
 assert(pread_guest(fd,b,32,2)==4&&!memcmp(b,"cdef",4));
 uint64_t before=sotc_page_ledger.serial;assert(pread_guest(fd,b,32,6)==0&&before==sotc_page_ledger.serial);
 assert((uint32_t)pread_guest(fd,b,32,-1)==UINT32_C(0x80020016)&&before==sotc_page_ledger.serial);
 sotc_buffer_page_history_dump_v1(4,2,0,(uint64_t)physical,2*PAGE);
 assert(close_guest(fd)==0);bootstrap_unmount_game_root();assert(unlink(path)==0&&rmdir(folder)==0);
 assert(!direct_release((uint64_t)physical,2*PAGE));
 sotc_buffer_page_history_dump_v1(5,2,0,(uint64_t)physical,2*PAGE);

 if(enabled){assert(sotc_page_ledger.serial>0);assert(sotc_page_ledger.buckets[(uint64_t)physical>>SOTC_PAGE_BUCKET_SHIFT].write_count>=5);}
 else assert(!sotc_page_ledger.buckets&&!sotc_page_ledger.serial);
 puts("PASS: actual map/alias/unmap/remap, imported memcpy/memset, backing bytes/partial failure and errno; no GPU");
 return 0;
}
'''
with tempfile.TemporaryDirectory(dir=ROOT/'work',prefix='page-history-fixture-') as tmp:
 p=Path(tmp);(p/'core.c').write_text(CORE)
 flags=['clang','-std=gnu11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'runtime/src')]
 subprocess.run(flags+['-fsanitize=address,undefined',str(p/'core.c'),'-o',str(p/'core')],check=True)
 subprocess.run([str(p/'core')],check=True)
 print('PASS: bounded ledger, exact intervals, overflow, lifecycle and million-event unrelated-traffic negative control under ASan/UBSan')
 bridge=r'''
#include "gpu_buffer_history.h"
#include <cassert>
static unsigned calls;
extern "C" void sotc_buffer_page_history_dump_v1(unsigned report,unsigned slot,unsigned node,std::uint64_t physical,std::uint64_t bytes) {
 assert(report==1&&slot==2&&node==1&&physical==64&&bytes==32);++calls;
}
int main(){
 SotcBufferHistory::Register(1,101,0,4096);SotcBufferHistory::Pool(1,0,4096);
 SotcBufferHistory::Register(2,102,0,256);
 SotcBufferHistory::NativeWrite(SotcBufferHistory::Kind::Copy,1,101,64,102,0,32);
 SotcBufferHistory::Dump(2,102,0,32);assert(calls==1);
}
'''
 (p/'bridge.cpp').write_text(bridge)
 subprocess.run(['clang++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-Wl,-export_dynamic','-I'+str(ROOT/'runtime/src'),str(p/'bridge.cpp'),'-o',str(p/'bridge')],check=True)
 subprocess.run([str(p/'bridge')],env=dict(os.environ,SOTC_BUFFER_HISTORY='1',SOTC_PAGE_HISTORY='1',SOTC_SCATTER_GUARD_DIR=str(p)),check=True)
 print('PASS: real buffer-history dump discovers host export and requests only the exact pool source interval')
 runtime=ROOT/'work/gpu-source-994846bb7bb5';cpu=runtime/'recomp';host=Path(os.environ.get('SOTC_PAGE_HISTORY_TEST_HOST',str(ROOT/'work/host-page-history-20261106')));deps=ROOT/'work/deps-arm64'
 source=(ROOT/'tools/build_host.sh').read_text().split('trial_support=(',1)[1].split(')\n    if',1)[0]
 variables={'trial_root':ROOT,'runtime_repo':runtime,'trial_work':host}
 support=[str(variables[v]/name) for v,name in re.findall(r'"\$(trial_root|runtime_repo|trial_work)/([^"\n]+)"',source)]
 assert len(support)>20
 (p/'host.c').write_text(HOST)
 includes=[deps/'include',cpu/'src',cpu/'src/cpu',runtime/'src',runtime/'gpu',host]
 libs=[host/'libbbcpu-context.a',deps/'lib/libZydis.a',deps/'lib/libZycore.a']
 subprocess.run(flags+['-O2','-pthread','-DSOTC_CPU_HAS_FP_CONTROL=1']+['-I'+str(x) for x in includes]+[str(p/'host.c')]+support+list(map(str,libs))+['-o',str(p/'host')],check=True)
 for mode in ('enabled','disabled'):
  evidence=p/mode;evidence.mkdir();env=dict(os.environ,SOTC_PAGE_HISTORY='1' if mode=='enabled' else '0',SOTC_MEMORY_BACKING='file',SOTC_SCATTER_GUARD_DIR=str(evidence))
  run=subprocess.run([str(p/'host'),mode],env=env,capture_output=True,text=True);assert run.returncode==0,run.stdout+run.stderr
  if mode=='enabled':
   reports=[json.loads((evidence/('page-history-'+str(i)+'-slot-2-node-0.json')).read_text()) for i in (1,2,3,4,10,5)];assert len(list(evidence.glob('*.json')))==6
   events=[e for b in reports[0]['buckets'] for e in b['events']];kinds={e['kind'] for e in events}
   assert 'memset_returned' in kinds and 'backing_segment_written_locked' in kinds and 'map_locked' in kinds,kinds
   second=[e for b in reports[1]['buckets'] for e in b['events']];assert any(e['kind']=='backing_segment_written_locked' and e['bytes']==16384 for e in second)
   third=[e for b in reports[2]['buckets'] for e in b['events']];assert {'map_locked','unmap_locked'}<={e['kind'] for e in third}
   files=[e for b in reports[3]['buckets'] for e in b['events']];assert any(e['kind']=='file_pread_returned' and e['bytes']==4 and e['value_offset_or_host_caller']==2 for e in files)
   copies=[e for b in reports[4]['buckets'] for e in b['events']];assert any(e['kind']=='memcpy_returned' and e['cpu_rip_observed']>0 for e in copies)
   released=[e for b in reports[5]['buckets'] for e in b['events']];assert any(e['kind']=='physical_zero_returned' for e in released)
   assert all(f.stat().st_mode&0o777==0o600 for f in evidence.glob('*.json'))
  else:assert not list(evidence.iterdir())
  print('PASS: native host hooks',mode)
print('PASS: physical page history fixtures; not game validation')
