/* Original SotC pool query extension. No mapping mutation or lifetime change.
 * Opt-in: merge only virtually AND physically contiguous, identically attributed
 * VMA spans under the existing read lock. No staging output is copied back here.
 */
#ifndef SOTC_POOL_CONTIGUOUS_SPAN_H
#define SOTC_POOL_CONTIGUOUS_SPAN_H
static pthread_once_t sotc_span_once=PTHREAD_ONCE_INIT;
static int sotc_span_enabled;
static void sotc_span_init(void) {
    const char *v=getenv("SOTC_POOL_CONTIGUOUS");
    sotc_span_enabled=v && !strcmp(v,"1");
}
static int sotc_contiguous_pool_phys(uintptr_t address,uint64_t *phys,uintptr_t *end) {
    pthread_once(&sotc_span_once,sotc_span_init);
    if (!sotc_span_enabled) return sotc_single_pool_phys(address,phys,end);
    read_lock();
    size_t i=vma_index(address);
    int ok=i<vma_count && vmas[i].start<=address && address<vmas[i].end &&
        (vmas[i].kind==KIND_DIRECT || vmas[i].kind==KIND_FLEXIBLE);
    if (ok) {
        const Vma first=vmas[i];
        uintptr_t limit=first.end;
        *phys=first.phys+(address-first.start);
        while (++i<vma_count) {
            const Vma next=vmas[i];
            const uint64_t extent=limit-first.start;
            if (next.start!=limit || next.end<=next.start ||
                next.kind!=first.kind || next.prot!=first.prot || next.type!=first.type ||
                extent>UINT64_MAX-first.phys || next.phys!=first.phys+extent) break;
            limit=next.end;
        }
        *end=limit;
    }
    read_unlock();
    return ok;
}
#endif
