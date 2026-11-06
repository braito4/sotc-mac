#ifndef SOTC_BUFFER_PAGE_HISTORY_CORE_H
#define SOTC_BUFFER_PAGE_HISTORY_CORE_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/* Retain four write observations and two lifecycle observations per physical
 * 256 KiB bucket. Exact byte intervals are retained; buckets are NOT allocations.
 * The owner serializes access. Unrelated GPU events never consume this ledger. */
#define SOTC_PAGE_BUCKET_SHIFT 18
#define SOTC_PAGE_BUCKET_SIZE (UINT64_C(1)<<SOTC_PAGE_BUCKET_SHIFT)
typedef struct {
    uint64_t sequence,ns,guest,physical,bytes,source,aux,pc,thread;
    uint64_t host_target,guest_instruction,guest_return;
    unsigned kind,return_status;
    uint64_t sample_physical,sample_begin_ns,sample_end_ns;
    unsigned sample_bytes,sample_status;
    unsigned char sample[64];
} SotcPageEvent;
typedef struct {
    SotcPageEvent writes[4],lifecycle[2];
    uint64_t write_count,lifecycle_count;
} SotcPageBucket;
typedef struct {
    SotcPageBucket *buckets;
    uint64_t count,serial,omitted,invalid;
} SotcPageLedger;
static inline int sotc_page_span(uint64_t a,uint64_t n) {return n && n<=UINT64_MAX-a;}
static inline int sotc_page_overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m) {
    return sotc_page_span(a,n)&&sotc_page_span(b,m)&&(a>=b?a-b<m:b-a<n);
}
static inline int sotc_page_ledger_init(SotcPageLedger *r,uint64_t bytes) {
    if(!bytes || bytes>(UINT64_C(32)<<30))return 0;
    r->count=(bytes+SOTC_PAGE_BUCKET_SIZE-1)>>SOTC_PAGE_BUCKET_SHIFT;
    r->buckets=calloc((size_t)r->count,sizeof(*r->buckets));return r->buckets!=NULL;
}
typedef void (*SotcPageSampler)(SotcPageEvent *,uint64_t,uint64_t,void *);
static inline void sotc_page_ledger_add_sampled(SotcPageLedger *r,SotcPageEvent e,
                                               SotcPageSampler sample,void *context) {
    if(!sotc_page_span(e.physical,e.bytes)){++r->invalid;return;}
    e.sequence=++r->serial;
    uint64_t first=e.physical>>SOTC_PAGE_BUCKET_SHIFT,last=(e.physical+e.bytes-1)>>SOTC_PAGE_BUCKET_SHIFT;
    if(!r->buckets||first>=r->count){++r->omitted;return;}
    if(last>=r->count){last=r->count-1;++r->omitted;}
    for(uint64_t p=first;p<=last;++p){
        SotcPageBucket *b=r->buckets+p;
        SotcPageEvent part=e;
        if(sample){
            const uint64_t low=p<<SOTC_PAGE_BUCKET_SHIFT,high=low+SOTC_PAGE_BUCKET_SIZE;
            sample(&part,e.physical>low?e.physical:low,
                   e.physical+e.bytes<high?e.physical+e.bytes:high,context);
        }
        if(e.kind==1||e.kind==2)b->lifecycle[b->lifecycle_count++%2]=part;
        else b->writes[b->write_count++%4]=part;
    }
}
static inline void sotc_page_ledger_add(SotcPageLedger *r,SotcPageEvent e) {
    sotc_page_ledger_add_sampled(r,e,NULL,NULL);
}
#endif
