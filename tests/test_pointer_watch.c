/* Original numeric-watch capacity/concurrency controls. No guest execution
 * or inferred allocation identities; native protected-reader controls below. */
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include "cpu_fault_diagnostics.h"
#include "cpu_field_trace.h"
#include "cpu_pointer_trace.h"

/* This CPU fixture registers no GPU-owned ranges. Actual native protected
 * reads are linked from the platform adapter, without a mock reader. */
unsigned runtime_memory_trap_reasons(uintptr_t address) {(void)address;return 0;}

static uint64_t keys[17];
static void *observe(void *unused) {
    (void)unused;
    for(unsigned i=0;i<10000;++i) {
        const uint64_t field=UINT64_C(0x40000000)+(i%64)*8;
        SotcPointerWatch *w=sotc_pointer_find(field,field,0,field,1,1,0);
        uint64_t previous[3]={0};sotc_pointer_history(w,field,i,i+1,previous);
    }
    return NULL;
}
int main(void) {
    assert(!sotc_pointer_init("0:7:0:0:1:1@3"));
    assert(sotc_pointer_sites[0].group==3);
    assert(sotc_pointer_init("0:7:0:0:1:1@4"));
    assert(sotc_pointer_init("0:7:0:0:1:1@-1"));
    assert(sotc_pointer_init("0:7:0:0:1:1@"));
    uint64_t header[SOTC_JOURNAL_HEADER_WORDS]={0};sotc_pointer_journal.map=header;
    /* Deliberately collide seventeen distinct fields in sixteen bounded
     * candidate slots. Refusal must preserve every admitted field. */
    unsigned count=0;
    for(uint64_t field=UINT64_C(0x10000000);count<17;field+=8)
        if(sotc_pointer_hash(field)==0)keys[count++]=field;
    SotcPointerWatch *original[16];
    for(unsigned i=0;i<16;++i) {
        original[i]=sotc_pointer_find(keys[i],keys[i],0,keys[i],1,1,0);
        assert(original[i]);
    }
    assert(!sotc_pointer_find(keys[16],keys[16],0,keys[16],1,1,0));
    assert(header[176]==16 && header[177]==1);
    for(unsigned i=0;i<16;++i)
        assert(original[i]==sotc_pointer_find(keys[i],keys[i],0,keys[i],1,1,0));
    /* Saturation/collision in bank zero cannot occupy another bank's slots,
     * including when exactly the same numeric field is selected there. */
    for(unsigned group=1;group<SOTC_POINTER_GROUPS;++group) {
        SotcPointerWatch *w=sotc_pointer_find(keys[16],keys[16],0,keys[16],1,1,group);
        assert(w && w>=sotc_pointer_persistent+group*SOTC_POINTER_GROUP_SLOTS &&
            w<sotc_pointer_persistent+(group+1)*SOTC_POINTER_GROUP_SLOTS);
        assert(w==sotc_pointer_find(keys[16],keys[16],0,keys[16],1,1,group));
        assert(header[224+group*2]==1 && header[225+group*2]==0);
    }
    assert(!sotc_pointer_find(keys[16],keys[16],0,keys[16],1,1,0));
    pthread_t threads[8];
    for(unsigned i=0;i<8;++i)assert(!pthread_create(threads+i,NULL,observe,NULL));
    for(unsigned i=0;i<8;++i)assert(!pthread_join(threads[i],NULL));
    for(unsigned i=0;i<64;++i) {
        const uint64_t field=UINT64_C(0x40000000)+i*8;
        SotcPointerWatch *w=sotc_pointer_find(field,field,0,field,1,1,0);assert(w);
        unsigned matches=0;
        for(unsigned j=0;j<SOTC_POINTER_PERSISTENT;++j)
            if(atomic_load(&sotc_pointer_persistent[j].stamp) && atomic_load(&sotc_pointer_persistent[j].field)==field)++matches;
        assert(matches==1);
        uint64_t previous[3]={0};sotc_pointer_history(w,field,123,456,previous);
        assert(previous[0]);
    }
    /* Force refusal on a real protected-readable word; verify the production
     * probe's per-target counter and the observe-only negative control. */
    uint64_t journal[512+3*16]={0},regs[16]={0};
    const size_t page=(size_t)sysconf(_SC_PAGESIZE);
    uint64_t *word=bb_low_map(page,PROT_READ|PROT_WRITE);assert(word);*word=123;
    sotc_pointer_journal=(SotcEventJournal){journal,3,1};
    for(unsigned i=0;i<SOTC_POINTER_GROUP_SLOTS;++i) {
        atomic_store(&sotc_pointer_persistent[i].stamp,1);
        atomic_store(&sotc_pointer_persistent[i].field,8);
    }
    for(unsigned group=1;group<SOTC_POINTER_GROUPS;++group)
        assert(sotc_pointer_find(0x55000000,0x55000000,0,0x55000000,1,1,group));
    for(unsigned i=0;i<SOTC_POINTER_PERSISTENT;++i) {
        atomic_store(&sotc_pointer_persistent[i].stamp,1);
        atomic_store(&sotc_pointer_persistent[i].field,8);
    }
    regs[7]=(uintptr_t)word;
    assert(regs[7]<(UINT64_C(1)<<40));
    assert(!sotc_pointer_init("0:7:0:0:1:1"));
    sotc_pointer_probe(regs,0,0);
    assert(journal[177]==1 && journal[192]==1);
    assert((journal[512+5+6]&15)==15);
    assert(!sotc_pointer_init("0:7:0:0:1:2"));
    sotc_pointer_probe(regs,0,0);
    assert(journal[177]==1 && journal[192]==1);
    assert((journal[512+5+6]&15)==15);
    assert(!munmap(word,page));
    puts("PASS: persistent refusal preserves keys; concurrent keys remain unique; production per-probe refusals exclude observe-only targets");
    return 0;
}
