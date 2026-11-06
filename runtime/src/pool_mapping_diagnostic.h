/* Bounded observational probe, included after the pinned memory implementation.
 * Reads its real VMA table under the existing lock; never changes a mapping.
 * Snapshot is after the original query, so concurrent map changes remain possible.
 */
#ifndef SOTC_POOL_MAPPING_DIAGNOSTIC_H
#define SOTC_POOL_MAPPING_DIAGNOSTIC_H
static pthread_once_t sotc_pool_probe_once=PTHREAD_ONCE_INIT;
static pthread_mutex_t sotc_pool_probe_gate=PTHREAD_MUTEX_INITIALIZER;
static int sotc_pool_probe_on;
static struct timespec sotc_pool_probe_start;
static uint64_t sotc_pool_probe_seen[256];
static unsigned sotc_pool_probe_count;
static void sotc_pool_probe_initialize(void) {
    const char *p=getenv("SOTC_POOL_MAPPING_TRACE");
    sotc_pool_probe_on=p && !strcmp(p,"1");
    clock_gettime(CLOCK_MONOTONIC,&sotc_pool_probe_start);
}
int runtime_memory_pool_phys(uintptr_t address,uint64_t *phys,uintptr_t *end) {
    int result=sotc_unobserved_pool_phys(address,phys,end);
    pthread_once(&sotc_pool_probe_once,sotc_pool_probe_initialize);
    /* The observed fallback bases are MiB-aligned. Do not trace every table page. */
    if (!sotc_pool_probe_on || (address&((1u<<20)-1))) return result;
    struct timespec now; clock_gettime(CLOCK_MONOTONIC,&now);
    if (now.tv_sec-sotc_pool_probe_start.tv_sec<180) return result;
    Vma snapshot[4]; size_t count=0;
    read_lock();
    size_t at=vma_index(address);
    for (size_t j=at;j<vma_count && count<4;++j) snapshot[count++]=vmas[j];
    read_unlock();
    uint64_t fingerprint=address;
    for(size_t i=0;i<count;++i) fingerprint=(fingerprint*UINT64_C(1099511628211))^
        snapshot[i].start^snapshot[i].end^snapshot[i].phys^(unsigned)snapshot[i].kind;
    pthread_mutex_lock(&sotc_pool_probe_gate);
    for(unsigned i=0;i<sotc_pool_probe_count;++i) if(sotc_pool_probe_seen[i]==fingerprint) {
        pthread_mutex_unlock(&sotc_pool_probe_gate); return result;
    }
    if(sotc_pool_probe_count==256) { pthread_mutex_unlock(&sotc_pool_probe_gate); return result; }
    unsigned serial=sotc_pool_probe_count++;
    sotc_pool_probe_seen[serial]=fingerprint;
    char line[1600];
    int used=snprintf(line,sizeof(line),"Pool mapping probe: sample=%u query=%#llx ok=%d physical=%#llx end=%#llx snapshot_after_query=1",
        serial,(unsigned long long)address,result,(unsigned long long)(result ? *phys : 0),
        (unsigned long long)(result ? *end : 0));
    for(size_t i=0;i<count && used>0 && (size_t)used<sizeof(line)-160;++i) {
        const Vma *v=&snapshot[i];
        used+=snprintf(line+used,sizeof(line)-(size_t)used," vma%zu=[%#llx,%#llx) kind=%d phys=%#llx prot=%x",
            i,(unsigned long long)v->start,(unsigned long long)v->end,v->kind,(unsigned long long)v->phys,v->prot);
    }
    printf("%s\n",line);
    if(sotc_pool_probe_count==256) printf("Pool mapping probe: limit=256 reached; later snapshots omitted\n");
    pthread_mutex_unlock(&sotc_pool_probe_gate);
    return result;
}
#endif
