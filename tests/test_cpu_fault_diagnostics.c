/* Original synthetic native memory/guard tests; no game or GPU. */
#include "platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
static uintptr_t guarded;
static unsigned reads;
unsigned runtime_memory_trap_reasons(uintptr_t address) {
    return guarded && address>=guarded && address-guarded<8;
}
static size_t observed_read(void *out,uintptr_t at,size_t n) {
    ++reads;return bb_read_memory(out,at,n);
}
#define bb_read_memory observed_read
#include "cpu_fault_diagnostics.h"
#undef bb_read_memory
int main(void) {
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *p=bb_low_map(3*page,PROT_NONE);assert(p);
    assert(!mprotect(p+page,page,PROT_READ|PROT_WRITE));
    uint64_t *object=(void *)(p+page+128),*edge=(void *)(p+2*page-8);
    object[0]=0x1234;object[1]=0x5678;object[2]=UINT64_C(0xdeadbeef01234565);object[3]=0x90;
    *edge=0xaabb;
    uint64_t words[4]={0};
    assert(sotc_fault_read(words,(uintptr_t)object,4)==4 && words[2]==object[2]);
    assert(sotc_fault_read(words,(uintptr_t)edge,4)==1 && words[0]==*edge);
    assert(sotc_fault_read(words,(uintptr_t)p,1)==0);
    assert(sotc_fault_read(words,UINT64_MAX-3,4)==0);
    assert(sotc_fault_read(words,(uintptr_t)&words,1)==0); /* host stack excluded */
    guarded=(uintptr_t)object+7;reads=0;
    assert(sotc_fault_read(words,(uintptr_t)object,4)==0 && reads==0);
    guarded=0;
    uint64_t *frame=(void *)(p+page+512);
    frame[-8]=UINT64_C(0x9274a16b);
    frame[0]=(uintptr_t)(frame+2);frame[1]=BB_LOW_MIN+0x28;
    frame[2]=(uintptr_t)frame;frame[3]=BB_LOW_MIN+0x38; /* backward cycle */
    BbGuestRegs regs={0};regs.r[12]=(uintptr_t)object;regs.r[13]=(uintptr_t)object;
    regs.r[4]=(uintptr_t)edge;regs.r[5]=(uintptr_t)frame;
    FILE *out=tmpfile();assert(out);
    sotc_cpu_fault_context(fileno(out),&regs,BB_LOW_MIN,128);
    assert(!fseek(out,0,SEEK_SET));char text[8192];
    size_t n=fread(text,1,sizeof(text)-1,out);text[n]=0;fclose(out);
    assert(n<sizeof(text)-1);
    assert(strstr(text,"0xdeadbeef01234565"));
    assert(strstr(text,"CPU fault memory r12") && !strstr(text,"CPU fault memory r13"));
    assert(strstr(text,"words=0x1 0xaabb stopped_unreadable_or_guarded"));
    assert(strstr(text,"image_offset=0x28") && strstr(text,"image_offset=0x38"));
    assert(strstr(text,"CPU fault memory frame_locals_candidate") && strstr(text,"0x9274a16b"));
    const char *first=strstr(text,"CPU fault frame candidate");assert(first);
    const char *second=strstr(first+1,"CPU fault frame candidate");assert(second);
    assert(!strstr(second+1,"CPU fault frame candidate"));
    assert(strstr(text,"no recovery attempted"));
    assert(object[2]==UINT64_C(0xdeadbeef01234565));
    assert(!munmap(p,3*page));reads=0;
    assert(sotc_fault_read(words,(uintptr_t)object,4)==0 && reads==1);
    puts("PASS: fatal memory snapshots, page edges, protected/unmapped reads, GPU-guard exclusion, deduplication and bounded cyclic frame candidates");
}
