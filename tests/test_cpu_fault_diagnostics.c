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
    assert(!sotc_cpu_fault_fields_init(NULL) && !sotc_fault_field_count);
    assert(!sotc_cpu_fault_fields_init("12:0x130:2,13:-16,0:+24:8"));
    assert(sotc_fault_field_count==3 && sotc_fault_fields[0].offset==304 &&
           sotc_fault_fields[1].offset==-16 && sotc_fault_fields[2].words==8);
    const char *invalid[]={"16:0","-1:0","12:","12:0x","12:0:0","12:0:9",
        "12:0,","12:0;1:0","12:0:1:2"," 12:0","12:0  ",
        "12:9223372036854775808","12:-9223372036854775809",
        "12:18446744073709551616","12:0x10000000000000000"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        assert(sotc_cpu_fault_fields_init(invalid[i])==-1);
        assert(sotc_fault_field_count==3 && sotc_fault_fields[0].offset==304);
    }
    char many[128]="";
    for(unsigned i=0;i<16;++i) strcat(many,"1:0,");
    many[strlen(many)-1]=0;
    assert(!sotc_cpu_fault_fields_init(many) && sotc_fault_field_count==16);
    strcat(many,",1:0");assert(sotc_cpu_fault_fields_init(many)==-1);
    char long_config[1025];memset(long_config,'1',1024);long_config[1024]=0;
    assert(sotc_cpu_fault_fields_init(long_config)==-1);
    assert(!sotc_cpu_fault_fields_init("0:-9223372036854775808"));
    assert(sotc_fault_fields[0].offset==INT64_MIN);
    assert(!sotc_cpu_fault_fields_init(""));
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
    /* Fields outside the old four-word window, including unaligned storage,
     * page edges and wrapped addresses. No game offsets or layouts. */
    const uint64_t marker=UINT64_C(0x8765432101234567);
    memcpy((unsigned char *)object+305,&marker,8);
    regs.r[0]=UINT64_MAX-3;regs.r[1]=3;regs.r[13]=(uintptr_t)edge;
    assert(!sotc_cpu_fault_fields_init("12:305,13:0:2,0:8,1:-8"));
    out=tmpfile();assert(out);
    sotc_cpu_fault_context(fileno(out),&regs,BB_LOW_MIN,128);
    rewind(out);n=fread(text,1,sizeof(text)-1,out);text[n]=0;fclose(out);
    assert(strstr(text,"CPU fault field candidate reg=r12 offset=0x131"));
    assert(strstr(text,"0x8765432101234567"));
    assert(strstr(text,"words=0x1 0xaabb stopped_unreadable_or_guarded"));
    assert(strstr(text,"reg=rax offset=0x8 address_overflow; not read"));
    assert(strstr(text,"reg=rcx offset=-0x8 address_overflow; not read"));
    guarded=(uintptr_t)object+305;reads=0;
    assert(sotc_fault_read(words,(uintptr_t)object+305,1)==0 && reads==0);
    guarded=0;
    uint64_t unchanged;memcpy(&unchanged,(unsigned char *)object+305,8);
    assert(unchanged==marker);
    assert(!sotc_cpu_fault_fields_init(NULL));
    assert(object[2]==UINT64_C(0xdeadbeef01234565));
    assert(!munmap(p,3*page));reads=0;
    assert(sotc_fault_read(words,(uintptr_t)object,4)==0 && reads==1);
    puts("PASS: fatal memory snapshots, page edges, protected/unmapped reads, GPU-guard exclusion, deduplication and bounded cyclic frame candidates");
}
