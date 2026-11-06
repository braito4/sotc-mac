#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
enum {KIND_RESERVED,KIND_DIRECT,KIND_FLEXIBLE};
typedef struct {uintptr_t start,end;int kind,prot,type;uint64_t phys;} Vma;
static Vma vmas[4];static size_t vma_count;static int locks;
static void read_lock(void){++locks;}static void read_unlock(void){--locks;}
static size_t vma_index(uintptr_t a){size_t i=0;while(i<vma_count && vmas[i].end<=a)++i;return i;}
static int sotc_single_pool_phys(uintptr_t a,uint64_t *p,uintptr_t *e){size_t i=vma_index(a);if(i==vma_count||a<vmas[i].start||vmas[i].kind==KIND_RESERVED)return 0;*p=vmas[i].phys+a-vmas[i].start;*e=vmas[i].end;return 1;}
#include "pool_contiguous_span.h"
int main(int argc,char **argv){assert(argc==2);setenv("SOTC_POOL_CONTIGUOUS",argv[1],1);
 for(int kind=KIND_DIRECT;kind<=KIND_FLEXIBLE;++kind){
 vma_count=3;for(unsigned i=0;i<3;++i)vmas[i]=(Vma){0x1000+i*0x1000,0x2000+i*0x1000,kind,0x33,0,0x9000+i*0x1000};
 Vma before[4];memcpy(before,vmas,sizeof vmas);uint64_t p=77;uintptr_t e=88;
 assert(!sotc_contiguous_pool_phys(0,&p,&e)&&p==77&&e==88);
 assert(sotc_contiguous_pool_phys(0x1800,&p,&e)&&p==0x9800);
 assert(e==(argv[1][0]=='1'?0x4000:0x2000));assert(!memcmp(before,vmas,sizeof vmas));
 if(argv[1][0]=='1'){
  for(int failure=0;failure<6;++failure){memcpy(vmas,before,sizeof vmas);
   switch(failure){case 0:vmas[1].phys+=0x1000;break;case 1:vmas[1].start+=0x100;break;case 2:vmas[1].kind=KIND_RESERVED;break;case 3:vmas[1].prot=1;break;case 4:vmas[1].type=1;break;case 5:vmas[1].kind=kind==KIND_DIRECT?KIND_FLEXIBLE:KIND_DIRECT;break;}
   assert(sotc_contiguous_pool_phys(0x1800,&p,&e)&&e==0x2000&&p==0x9800);
  }
  memcpy(vmas,before,sizeof vmas);vmas[0].phys=UINT64_MAX-0xfff;vmas[1].phys=0;
  assert(sotc_contiguous_pool_phys(0x1000,&p,&e)&&e==0x2000);
 }
 assert(!locks);
 }
 return 0;
}
