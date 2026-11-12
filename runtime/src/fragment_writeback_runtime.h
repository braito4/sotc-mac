/* Included in the actual memory backend TU. Validate the entire group under the
 * memory-table lock before any store. No guest view, callbacks or observers under
 * this lock. GPU work must be complete before either refresh or publication.
 * This is atomic against mapping changes, not arbitrary concurrent guest stores. */
#ifndef SOTC_FRAGMENT_WRITEBACK_RUNTIME_H
#define SOTC_FRAGMENT_WRITEBACK_RUNTIME_H
#include "fragment_writeback_api.h"
int sotc_fragment_transfer_batch_v2(const SotcFragmentTransfer *records,unsigned count,
                                    unsigned write_back) {
    if (!records || !count || count>2 || write_back>1 || !backing_base) return 0;
    for (unsigned r=0;r<count;++r) {
        const SotcFragmentTransfer *v=&records[r];
        if (!v->data || !v->size || v->size>(UINT64_C(8)<<20) ||
            v->size>UINTPTR_MAX-v->address || (v->generation&1)) return 0;
    }
    struct Part {uint64_t physical,bytes;unsigned record;uintptr_t guest;} parts[256];
    unsigned used=0;
    read_lock();
    const uint64_t observed=__atomic_load_n(&table_generation,__ATOMIC_ACQUIRE);
    const uint64_t pool_bytes=POOL_SIZE;
    int ok=1;
    for (unsigned r=0;ok && r<count;++r) {
        const SotcFragmentTransfer *v=&records[r];
        if (observed!=v->generation) {ok=0;break;}
        const uintptr_t end=v->address+v->size;
        for (uintptr_t at=v->address;ok && at<end;) {
            const size_t i=vma_index(at);
            if (i==vma_count || vmas[i].start>at || vmas[i].end<=at ||
                (vmas[i].kind!=KIND_DIRECT && vmas[i].kind!=KIND_FLEXIBLE)) {ok=0;break;}
            const uint64_t n=(vmas[i].end<end ? vmas[i].end : end)-at,delta=at-vmas[i].start;
            if (vmas[i].phys>pool_bytes || delta>pool_bytes-vmas[i].phys ||
                n>pool_bytes-vmas[i].phys-delta || used==256) {ok=0;break;}
            const uint64_t physical=vmas[i].phys+delta;
            /* Reject physical aliases too: distinct guest ranges can share backing. */
            for (unsigned j=0;j<used;++j) {
                const uint64_t old=parts[j].physical;
                if (physical>=old ? physical-old<parts[j].bytes : old-physical<n) {ok=0;break;}
            }
            if (!ok) break;
            parts[used++]=(struct Part){physical,n,r,at};at+=n;
        }
    }
    for (unsigned i=0;ok && i<used;++i) {
        const struct Part *part=&parts[i];const SotcFragmentTransfer *v=&records[part->record];
        unsigned char *backing=backing_base+part->physical;
        unsigned char *staging=(unsigned char *)v->data+(part->guest-v->address);
        if (write_back) store_once(backing,staging,part->bytes);
        else memcpy(staging,backing,part->bytes);
    }
    read_unlock();
    if (!ok) fprintf(stderr,"Fragment group rejected: phase=%u outputs=%u expected_generation=%llu observed_generation=%llu; no backing stores\n",
                     write_back,count,(unsigned long long)records[0].generation,(unsigned long long)observed);
    return ok;
}
/* Keep the existing host entry point for previous renderer controls. */
int sotc_fragment_transfer_v1(uintptr_t address,void *data,uint64_t size,
                              uint64_t generation,unsigned write_back) {
    const SotcFragmentTransfer record={address,data,size,generation};
    return sotc_fragment_transfer_batch_v2(&record,1,write_back);
}
#endif
