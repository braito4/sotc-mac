/* Included after the checked memory backend, sharing its real VMA table/lock.
 * Lock order: memory lock -> ledger lock. Dumps use only the ledger lock.
 * Optional content reads use only the permanent backing view under the VMA lock.
 * No guest callback, guest alias read, mprotect or GPU call occurs here. */
#ifndef SOTC_BUFFER_PAGE_HISTORY_RUNTIME_H
#define SOTC_BUFFER_PAGE_HISTORY_RUNTIME_H
#include "buffer_page_history_core.h"
#include "guest_failure_state.h"
#include "cpu_import_origin_api.h"
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
__attribute__((weak)) int bootstrap_cpu_failure_state(SotcGuestFailureState *out);
static pthread_once_t sotc_page_once=PTHREAD_ONCE_INIT;
static pthread_mutex_t sotc_page_gate=PTHREAD_MUTEX_INITIALIZER;
static SotcPageLedger sotc_page_ledger;
static int sotc_page_on,sotc_page_content;
static unsigned sotc_page_reports;
static void sotc_page_init(void) {
    const char *s=getenv("SOTC_PAGE_HISTORY");
    if(s&&!strcmp(s,"1")) {
        sotc_page_on=sotc_page_ledger_init(&sotc_page_ledger,POOL_SIZE+FLEX_SPAN);
        const char *content=getenv("SOTC_PAGE_CONTENT");sotc_page_content=content&&!strcmp(content,"1");
        fprintf(stderr,"Page history: enabled=%d bucket_bytes=%llu retained_writes=4 retained_lifecycle=2 allocation_bytes=%llu content_samples=%d; direct guest stores absent\n",
            sotc_page_on,(unsigned long long)SOTC_PAGE_BUCKET_SIZE,
            (unsigned long long)(sotc_page_ledger.count*sizeof(SotcPageBucket)),sotc_page_content);
    }
}
static int sotc_page_enabled(void) {pthread_once(&sotc_page_once,sotc_page_init);return sotc_page_on;}
static uint64_t sotc_page_now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000u+(uint64_t)t.tv_nsec;}
/* Caller already owns the VMA lock, then the ledger lock. Sampling never queries
 * mappings or waits for GPU work. The backing view bypasses guest read traps.
 * 64 bytes per intersected bucket, at most 256 samples (16 KiB) per observation.
 * Atomic byte loads avoid compiler widening; the whole sample is NOT atomic. */
static void sotc_page_sample(SotcPageEvent *e,uint64_t lo,uint64_t hi,void *context) {
    unsigned *remaining=context;
    if(e->kind==SOTC_PAGE_CPU_WRITE_NOTICE)return;
    e->sample_physical=lo;
    if(!*remaining){e->sample_status=3;return;}--*remaining;
    if(!backing_base||lo>=hi||hi>POOL_SIZE+FLEX_SPAN){e->sample_status=2;return;}
    e->sample_bytes=(unsigned)(hi-lo<sizeof(e->sample)?hi-lo:sizeof(e->sample));
    e->sample_begin_ns=sotc_page_now();
    for(unsigned i=0;i<e->sample_bytes;++i)e->sample[i]=__atomic_load_n(backing_base+lo+i,__ATOMIC_RELAXED);
    e->sample_end_ns=sotc_page_now();e->sample_status=1;
}
static void sotc_page_record_locked(SotcPageEvent e) {
    unsigned remaining=256;
    sotc_page_ledger_add_sampled(&sotc_page_ledger,e,sotc_page_content?sotc_page_sample:NULL,&remaining);
}
static SotcPageEvent sotc_page_event(unsigned kind,uintptr_t guest,uint64_t bytes,uintptr_t source,uint64_t aux) {
    SotcPageEvent e={0};e.kind=kind;e.guest=guest;e.bytes=bytes;e.source=source;e.aux=aux;
    e.ns=sotc_page_now();e.thread=(uintptr_t)pthread_self();
    /* Existing TLS only; no stack walk, dereference of guest memory or CPU allocation. */
    SotcGuestFailureState state={0};
    if(bootstrap_cpu_failure_state&&bootstrap_cpu_failure_state(&state)) {
        e.pc=state.rip;
        SotcImportOrigin origin={0};
        /* x86 RSP is register 4 in the checked CPU snapshot. Query only TLS;
         * return-address validation happened at the bridge before this lock. */
        if(sotc_cpu_import_origin_v1&&sotc_cpu_import_origin_v1(state.rip,state.regs[4],&origin)) {
            e.host_target=origin.target;e.guest_instruction=origin.guest_instruction;
            e.guest_return=origin.guest_return;e.return_status=origin.return_status;
        }
    }
    return e;
}
void sotc_buffer_page_mapping_v1(unsigned kind,uintptr_t guest,uint64_t physical,uint64_t bytes) {
    const int saved=errno;
    if(sotc_page_enabled()) {
        SotcPageEvent e=sotc_page_event(kind,guest,bytes,0,0);e.physical=physical;
        pthread_mutex_lock(&sotc_page_gate);sotc_page_record_locked(e);pthread_mutex_unlock(&sotc_page_gate);
    }
    errno=saved;
}
void sotc_buffer_page_backing_v1(uintptr_t guest,uint64_t physical,uint64_t bytes,uintptr_t source,uintptr_t caller) {
    const int saved=errno;
    if(sotc_page_enabled()) {
        SotcPageEvent e=sotc_page_event(SOTC_PAGE_BACKING_WRITE,guest,bytes,source,caller);e.physical=physical;
        pthread_mutex_lock(&sotc_page_gate);sotc_page_record_locked(e);pthread_mutex_unlock(&sotc_page_gate);
    }
    errno=saved;
}
void sotc_buffer_page_cpu_v1(unsigned kind,uintptr_t guest,uint64_t bytes,uintptr_t source,uint64_t aux) {
    const int saved=errno;
    if(sotc_page_enabled()&&sotc_page_span(guest,bytes)) {
        SotcPageEvent e=sotc_page_event(kind,guest,bytes,source,aux);
        /* CPU-import maps are observations AFTER the operation, not a lifetime pin. */
        read_lock();
        size_t i=vma_index(guest);unsigned segments=0;
        const uintptr_t end=guest+bytes;
        for(;i<vma_count&&vmas[i].start<end;++i) {
            const Vma v=vmas[i];if(v.kind==KIND_RESERVED)continue;
            if(++segments>256){pthread_mutex_lock(&sotc_page_gate);++sotc_page_ledger.omitted;pthread_mutex_unlock(&sotc_page_gate);break;}
            uintptr_t lo=v.start>guest?v.start:guest,hi=v.end<end?v.end:end;
            if(hi<=lo)continue;
            SotcPageEvent part=e;part.guest=lo;part.bytes=hi-lo;part.physical=v.phys+(lo-v.start);
            if(kind==SOTC_PAGE_MEMCPY&&source<=UINTPTR_MAX-(lo-guest))part.source=source+(lo-guest);
            pthread_mutex_lock(&sotc_page_gate);sotc_page_record_locked(part);pthread_mutex_unlock(&sotc_page_gate);
        }
        read_unlock();
    }
    errno=saved;
}
static const char *sotc_page_kind(unsigned k) {
    switch(k){case SOTC_PAGE_MAP:return "map_locked";case SOTC_PAGE_UNMAP:return "unmap_locked";
    case SOTC_PAGE_MEMCPY:return "memcpy_returned";case SOTC_PAGE_MEMSET:return "memset_returned";
    case SOTC_PAGE_FILE_READ:return "file_read_returned";case SOTC_PAGE_FILE_PREAD:return "file_pread_returned";
    case SOTC_PAGE_BACKING_WRITE:return "backing_segment_written_locked";
    case SOTC_PAGE_CPU_WRITE_NOTICE:return "cpu_write_notice";
    case SOTC_PAGE_ZERO:return "physical_zero_returned";default:return "unknown";}
}
/* Called by the isolated renderer via dlsym; snapshots are at report time, NOT
 * at the earlier source-copy time. The caller never holds the memory lock. */
void sotc_buffer_page_history_dump_v1(unsigned report,unsigned slot,unsigned node,uint64_t physical,uint64_t bytes) {
    const int saved=errno;
    if(!sotc_page_enabled()||!sotc_page_span(physical,bytes)){errno=saved;return;}
    pthread_mutex_lock(&sotc_page_gate);
    const unsigned number=++sotc_page_reports;
    uint64_t first=physical>>SOTC_PAGE_BUCKET_SHIFT,last=(physical+bytes-1)>>SOTC_PAGE_BUCKET_SHIFT;
    if(number>128||first>=sotc_page_ledger.count){pthread_mutex_unlock(&sotc_page_gate);fprintf(stderr,"Page history: report omitted number=%u out_of_range_or_budget=1\n",number);errno=saved;return;}
    uint64_t count=last-first+1;if(count>256)count=256;
    if(count>sotc_page_ledger.count-first)count=sotc_page_ledger.count-first;
    SotcPageBucket *copy=malloc((size_t)count*sizeof(*copy));
    if(copy)memcpy(copy,sotc_page_ledger.buckets+first,(size_t)count*sizeof(*copy));
    const uint64_t serial=sotc_page_ledger.serial,omitted=sotc_page_ledger.omitted,invalid=sotc_page_ledger.invalid,ns=sotc_page_now();
    pthread_mutex_unlock(&sotc_page_gate);
    if(!copy){fprintf(stderr,"Page history: snapshot allocation failed\n");errno=saved;return;}
    const char *dir=getenv("SOTC_SCATTER_GUARD_DIR");char path[4096];int fd=-1;
    if(dir&&dir[0]=='/') {
        int n=snprintf(path,sizeof(path),"%s/page-history-%u-slot-%u-node-%u.json",dir,report,slot,node);
        if(n>0&&(size_t)n<sizeof(path))fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
    }
    FILE *f=fd>=0?fdopen(fd,"w"):NULL;if(!f&&fd>=0)close(fd);
    int ok=0;unsigned events=0;
    if(f) {
        fprintf(f,"{\"schema\":3,\"clock\":\"CLOCK_MONOTONIC\",\"report\":%u,\"slot\":%u,\"node\":%u,\"physical\":%llu,\"bytes\":%llu,\"snapshot_monotonic_ns\":%llu,\"total_observations\":%llu,\"omitted\":%llu,\"invalid\":%llu,\"bucket_bytes\":%llu,\"buckets_truncated\":%s,\"buckets\":[",report,slot,node,(unsigned long long)physical,(unsigned long long)bytes,(unsigned long long)ns,(unsigned long long)serial,(unsigned long long)omitted,(unsigned long long)invalid,(unsigned long long)SOTC_PAGE_BUCKET_SIZE,count<last-first+1?"true":"false");
        for(uint64_t i=0;i<count;++i) {
            SotcPageBucket *b=copy+i;
            fprintf(f,"%s{\"physical\":%llu,\"writes_seen\":%llu,\"lifecycle_seen\":%llu,\"writes_evicted\":%llu,\"lifecycle_evicted\":%llu,\"events\":[",i?",":"",(unsigned long long)((first+i)<<SOTC_PAGE_BUCKET_SHIFT),(unsigned long long)b->write_count,(unsigned long long)b->lifecycle_count,(unsigned long long)(b->write_count>4?b->write_count-4:0),(unsigned long long)(b->lifecycle_count>2?b->lifecycle_count-2:0));
            unsigned n=0;
            for(unsigned j=0;j<6;++j) {
                const SotcPageEvent *e=j<4?b->writes+j:b->lifecycle+j-4;
                if(!e->sequence||!sotc_page_overlap(physical,bytes,e->physical,e->bytes))continue;
                fprintf(f,"%s{\"seq\":%llu,\"ns\":%llu,\"kind\":\"%s\",\"guest\":%llu,\"physical\":%llu,\"bytes\":%llu,\"source_or_fd\":%llu,\"value_offset_or_host_caller\":%llu,\"cpu_rip_observed\":%llu,\"thread\":%llu,\"host_target\":%llu,\"guest_instruction\":%llu,\"guest_return\":%llu,\"return_status\":%u",n++?",":"",(unsigned long long)e->sequence,(unsigned long long)e->ns,sotc_page_kind(e->kind),(unsigned long long)e->guest,(unsigned long long)e->physical,(unsigned long long)e->bytes,(unsigned long long)e->source,(unsigned long long)e->aux,(unsigned long long)e->pc,(unsigned long long)e->thread,(unsigned long long)e->host_target,(unsigned long long)e->guest_instruction,(unsigned long long)e->guest_return,e->return_status);
                fprintf(f,",\"sample\":{\"status\":%u,\"physical\":%llu,\"bytes\":%u,\"begin_ns\":%llu,\"end_ns\":%llu,\"hex\":\"",e->sample_status,(unsigned long long)e->sample_physical,e->sample_bytes,(unsigned long long)e->sample_begin_ns,(unsigned long long)e->sample_end_ns);
                for(unsigned k=0;k<e->sample_bytes;++k)fprintf(f,"%02x",e->sample[k]);
                fputs("\"}}",f);++events;
            }
            fputs("]}",f);
        }
        fputs("],\"limits\":\"At report time, not earlier copy time. Exact spans within coarse retention buckets, not guest allocation identities. CPU RIP may be a host import. Guest instruction and return are registered-code observations at the host boundary, not unwound callers. Return status: 0 unavailable/not applicable, 1 unreadable, 2 non-guest, 3 guest continuation. Samples: 0 disabled/not applicable, 1 captured, 2 unavailable, 3 budget omitted. Up to 64 bytes per bucket, 256 sampled buckets per observation. Backing-view byte loads under VMA lock, not an atomic or GPU-synchronized snapshot. Equality applies only to sampled bytes; same physical range is not allocation identity. Direct guest stores and GPU writes absent. CPU import mapping observed after operation. A CPU notice is not proof of a completed write. Empty history is not proof of no write.\"}\n",f);
        ok=!ferror(f)&&fflush(f)==0&&fsync(fd)==0;if(fclose(f)!=0)ok=0;
    }
    fprintf(stderr,"Page history report: report=%u slot=%u node=%u buckets=%llu events=%u saved=%d\n",report,slot,node,(unsigned long long)count,events,ok);
    free(copy);errno=saved;
}
#endif
