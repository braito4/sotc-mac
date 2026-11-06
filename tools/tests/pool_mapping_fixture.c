#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef struct {uintptr_t start,end;int kind,prot,type;uint64_t phys;} Vma;
static Vma vmas[]={{0x100000,0x200000,2,3,0,0x400000},{0x200000,0x300000,2,3,0,0x500000}};
static size_t vma_count=2;
static int reads;
static void read_lock(void){++reads;}
static void read_unlock(void){--reads;}
static size_t vma_index(uintptr_t p){return p>=0x200000 ? 1:0;}
static int sotc_unobserved_pool_phys(uintptr_t p,uint64_t *phys,uintptr_t *end){if(p==0)return 0;*phys=p+0x300000;*end=p+0x100000;return 1;}
#include "pool_mapping_diagnostic.h"
int main(int argc,char **argv){assert(argc==2);setenv("SOTC_POOL_MAPPING_TRACE",argv[1],1);
 Vma before[2];memcpy(before,vmas,sizeof(vmas));uint64_t phys=77;uintptr_t end=88;
 assert(!runtime_memory_pool_phys(0,&phys,&end));assert(phys==77 && end==88);
 sotc_pool_probe_start.tv_sec-=181;
 for(unsigned i=1;i<400;++i){uintptr_t p=(uintptr_t)i<<20;assert(runtime_memory_pool_phys(p,&phys,&end));assert(phys==p+0x300000 && end==p+0x100000);}
 assert(sotc_pool_probe_count==(argv[1][0]=='1'?256u:0u));
 assert(!reads && !memcmp(before,vmas,sizeof(vmas)));puts("PASS: result preserved, VMA unchanged, bounded trace");}
