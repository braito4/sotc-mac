/* Original bounded, opt-in observation of configured guest fields at reviewed
 * basic-block entries. Uses bbcpu's existing probe API without guest code or
 * return-slot edits. No game-specific site, field or address is compiled in.
 * Requires cpu_fault_diagnostics.h for the protected, GPU-guard-aware reader.
 * Observations are not exhaustive write tracing or allocation identities.
 */
#ifndef SOTC_CPU_FIELD_TRACE_H
#define SOTC_CPU_FIELD_TRACE_H
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <time.h>
#include "diagnostic_event_journal.h"

enum { SOTC_TRACE_SITES = 28, SOTC_TRACE_RECORDS = 2048 };
typedef struct {
    uint64_t site; unsigned reg; int64_t offset; unsigned on_change;
    unsigned has_match, match_reg; uint64_t match_offset;
} SotcTraceSite;
typedef struct {
    _Atomic unsigned busy;
    _Atomic uint64_t sequence, site, object, field, value, status, thread, ticks;
} SotcTraceRecord;
static SotcTraceSite sotc_trace_sites[SOTC_TRACE_SITES];
static unsigned sotc_trace_site_count;
/* Cancellation may arrive while startup publishes the image base. */
static _Atomic uint64_t sotc_trace_image_base;
static SotcTraceRecord sotc_trace_records[SOTC_TRACE_RECORDS];
static _Atomic uint64_t sotc_trace_next, sotc_trace_busy_drops, sotc_trace_threads;
static _Atomic uint64_t sotc_trace_suppressed;
static _Atomic uint64_t sotc_trace_filtered;
static _Atomic unsigned sotc_trace_frozen;
static _Thread_local uint64_t sotc_trace_thread;
static SotcEventJournal sotc_trace_journal;
typedef struct { uint64_t site, object, field, value, status; unsigned valid; } SotcTraceSeen;
static _Thread_local SotcTraceSeen sotc_trace_seen[128];

/* Startup only: site:register:offset[:on-change[:match-reg:match-image-offset]].
 * A match compares a register value, without following it as a pointer.
 * A site must be a reviewed block entry; arbitrary interior instructions may
 * not be observed by the pinned JIT. Failure leaves the previous selection. */
static int sotc_cpu_field_trace_init(const char *config) {
    if (!config || !*config) { sotc_trace_site_count=0; return 0; }
    SotcTraceSite sites[SOTC_TRACE_SITES]; unsigned count=0;
    const char *p=config; size_t length=0;
    while (length<2048 && p[length]) ++length;
    if (length==2048) return -1;
    while (*p) {
        uint64_t site, reg, offset;
        if (count==SOTC_TRACE_SITES ||
            sotc_fault_number(&p,0,UINT64_MAX,&site) || *p++!=':' ||
            sotc_fault_number(&p,10,15,&reg) || *p++!=':') return -1;
        const int negative=*p=='-'; if (negative || *p=='+') ++p;
        if (sotc_fault_number(&p,0,(uint64_t)INT64_MAX+(unsigned)negative,&offset)) return -1;
        uint64_t on_change=0;
        if (*p==':') {
            ++p;
            if (sotc_fault_number(&p,10,1,&on_change)) return -1;
        }
        uint64_t match_reg=0, match_offset=0; unsigned has_match=0;
        if (*p==':') {
            ++p; has_match=1;
            if (sotc_fault_number(&p,10,15,&match_reg) || *p++!=':' ||
                sotc_fault_number(&p,0,UINT64_MAX,&match_offset)) return -1;
        }
        if (*p && *p!=',') return -1;
        const int64_t signed_offset=negative ?
            (offset==(uint64_t)INT64_MAX+1 ? INT64_MIN : -(int64_t)offset) : (int64_t)offset;
        for (unsigned i=0;i<count;++i)
            if (sites[i].site==site && sites[i].reg==reg && sites[i].offset==signed_offset) return -1;
        sites[count++]=(SotcTraceSite){site,(unsigned)reg,signed_offset,(unsigned)on_change,
                                     has_match,(unsigned)match_reg,match_offset};
        if (*p==',' && !*++p) return -1;
    }
    for (unsigned i=0;i<count;++i) sotc_trace_sites[i]=sites[i];
    sotc_trace_site_count=count;
    return 0;
}

/* Termination only: stop newly entering probes before reporting other context.
 * Already-entered callbacks may still publish; never wait for them or suspend
 * guest threads. This preserves evidence, not a synchronized guest snapshot. */
static void sotc_cpu_field_trace_freeze(void) {
    atomic_store_explicit(&sotc_trace_frozen,1,memory_order_release);
}

static void sotc_cpu_field_probe(const uint64_t regs[16], uint64_t address, uint32_t tag) {
    if (tag>=sotc_trace_site_count ||
        atomic_load_explicit(&sotc_trace_frozen,memory_order_acquire)) return;
    const int saved_errno=errno;
    const SotcTraceSite site=sotc_trace_sites[tag];
    /* Reject unrelated calls before any protected guest read, clock query,
     * cache update or ring reservation. The equality is diagnostic selection,
     * not validation of the pointed-to object or its lifetime. */
    if (site.has_match &&
        (site.match_offset>UINT64_MAX-sotc_trace_image_base ||
         regs[site.match_reg]!=sotc_trace_image_base+site.match_offset)) {
        atomic_fetch_add_explicit(&sotc_trace_filtered,1,memory_order_relaxed);
        errno=saved_errno; return;
    }
    const uint64_t object=regs[site.reg];
    const uint64_t magnitude=site.offset<0 ? (uint64_t)(-(site.offset+1))+1 : (uint64_t)site.offset;
    const int overflow=site.offset<0 ? object<magnitude : object>UINT64_MAX-magnitude;
    const uint64_t field=overflow ? 0 : site.offset<0 ? object-magnitude : object+magnitude;
    uint64_t value=0, status=overflow ? 0 : 1;
    if (!overflow && sotc_fault_read(&value,field,1)==1) status|=2;
    /* Optional suppression only at high-frequency consumer sites. Lifecycle
     * sites should retain every observation, including address reuse. This
     * direct-mapped thread-local cache never follows pointers and may evict
     * samples on collision; eviction adds records, never hides a changed value.
     * Changes and ABA between observations remain outside this diagnostic. */
    SotcTraceSeen *seen=NULL;
    if (site.on_change) {
        seen=&sotc_trace_seen[((field>>3)^(address>>4))&127];
        if (seen->valid && seen->site==address && seen->object==object && seen->field==field &&
            seen->value==value && seen->status==status) {
            atomic_fetch_add_explicit(&sotc_trace_suppressed,1,memory_order_relaxed);
            sotc_event_journal_suppressed(&sotc_trace_journal);
            errno=saved_errno; return;
        }
    }
    struct timespec now;
    uint64_t ticks=0;
    if (!clock_gettime(CLOCK_MONOTONIC,&now)) {
        ticks=(uint64_t)now.tv_sec*UINT64_C(1000000000)+(uint64_t)now.tv_nsec;
        status|=4;
    }
    if (!sotc_trace_thread)
        sotc_trace_thread=atomic_fetch_add_explicit(&sotc_trace_threads,1,memory_order_relaxed)+1;
    /* Independent per-site tails survive a fatal signal/full stderr pipe.
     * An unrelated hot site cannot evict this site's evidence. No extra reads. */
    /* Optional seventh payload word joins numeric thread identities with the
     * pointer journal. Legacy records leave this word zero. No extra reads. */
    const uint64_t evidence[SOTC_JOURNAL_DATA_WORDS]={address,object,field,value,status,sotc_trace_thread,(uintptr_t)pthread_self()};
    sotc_event_journal_record(&sotc_trace_journal,tag,ticks,evidence);
    const uint64_t ticket=atomic_fetch_add_explicit(&sotc_trace_next,1,memory_order_relaxed);
    SotcTraceRecord *r=&sotc_trace_records[ticket%SOTC_TRACE_RECORDS];
    unsigned expected=0;
    if (!atomic_compare_exchange_strong_explicit(&r->busy,&expected,1,memory_order_acquire,memory_order_relaxed)) {
        atomic_fetch_add_explicit(&sotc_trace_busy_drops,1,memory_order_relaxed);
        errno=saved_errno; return;
    }
    atomic_store_explicit(&r->sequence,0,memory_order_release);
    atomic_thread_fence(memory_order_seq_cst);
    atomic_store_explicit(&r->site,address,memory_order_relaxed);
    atomic_store_explicit(&r->object,object,memory_order_relaxed);
    atomic_store_explicit(&r->field,field,memory_order_relaxed);
    atomic_store_explicit(&r->value,value,memory_order_relaxed);
    atomic_store_explicit(&r->status,status,memory_order_relaxed);
    atomic_store_explicit(&r->thread,sotc_trace_thread,memory_order_relaxed);
    atomic_store_explicit(&r->ticks,ticks,memory_order_relaxed);
    atomic_store_explicit(&r->sequence,ticket+1,memory_order_release);
    atomic_store_explicit(&r->busy,0,memory_order_release);
    if (seen) *seen=(SotcTraceSeen){address,object,field,value,status&3,1};
    errno=saved_errno;
}

/* After executable-range validation, before any guest threads. Code ranges
 * must be registered before execution, as for every other bbcpu probe. */
static void sotc_cpu_field_trace_install(uint64_t image_base) {
    sotc_trace_image_base=image_base;
    for (unsigned i=0;i<sotc_trace_site_count;++i)
        bbcpu_set_probe(image_base+sotc_trace_sites[i].site,sotc_cpu_field_probe,i);
}

/* Terminating path: bounded atomic snapshots, no lock, allocation or guest read.
 * A signal can interrupt a probe or another thread can overwrite its slot;
 * uncommitted/replaced records are skipped, never waited on. */
static void sotc_cpu_field_trace_dump_limited(int fd, unsigned limit) {
    if (!sotc_trace_site_count) return;
    const uint64_t end=atomic_load_explicit(&sotc_trace_next,memory_order_acquire);
    const uint64_t first=end>SOTC_TRACE_RECORDS ? end-SOTC_TRACE_RECORDS : 0;
    if (limit>SOTC_TRACE_RECORDS) limit=SOTC_TRACE_RECORDS;
    const uint64_t start=end-first>limit ? end-limit : first;
    char begin[192];size_t b=sotc_fault_text(begin,"CPU field trace dump: snapshot_end=");
    b+=sotc_fault_hex(begin+b,end);b+=sotc_fault_text(begin+b," event_limit=");
    b+=sotc_fault_hex(begin+b,limit);b+=sotc_fault_text(begin+b," budget_omitted=");
    b+=sotc_fault_hex(begin+b,start-first);b+=sotc_fault_text(begin+b," frozen=");
    b+=sotc_fault_hex(begin+b,atomic_load_explicit(&sotc_trace_frozen,memory_order_acquire));
    b+=sotc_fault_text(begin+b," newest_first=");b+=sotc_fault_hex(begin+b,1);begin[b++]='\n';
    if (write(fd,begin,b)!=(ssize_t)b) return;
    static const char header[]="CPU field trace: bounded site observations; not exhaustive writes or validated object lifetimes\n";
    if (write(fd,header,sizeof(header)-1)!=(ssize_t)(sizeof(header)-1)) return;
    uint64_t retained=0;
    // Output may fill before the footer. Preserve the closest observations
    // first; the offline analyzer sorts reservation sequences for comparison.
    for (uint64_t back=0;back<end-start;++back) {
        const uint64_t ticket=end-1-back;
        const SotcTraceRecord *r=&sotc_trace_records[ticket%SOTC_TRACE_RECORDS];
        const uint64_t sequence=atomic_load_explicit(&r->sequence,memory_order_acquire);
        if (sequence!=ticket+1) continue;
        const uint64_t values[]={
            sequence, atomic_load_explicit(&r->site,memory_order_relaxed),
            atomic_load_explicit(&r->object,memory_order_relaxed),
            atomic_load_explicit(&r->field,memory_order_relaxed),
            atomic_load_explicit(&r->value,memory_order_relaxed),
            atomic_load_explicit(&r->status,memory_order_relaxed),
            atomic_load_explicit(&r->thread,memory_order_relaxed),
            atomic_load_explicit(&r->ticks,memory_order_relaxed)};
        atomic_thread_fence(memory_order_acquire);
        if (atomic_load_explicit(&r->sequence,memory_order_acquire)!=sequence) continue;
        static const char *const labels[]={" seq="," site="," object="," field="," value="," status="," thread="," ns="};
        char line[384];size_t n=sotc_fault_text(line,"CPU field trace event:");
        for (unsigned i=0;i<8;++i) {
            n+=sotc_fault_text(line+n,labels[i]); n+=sotc_fault_hex(line+n,values[i]);
        }
        line[n++]='\n';
        if (write(fd,line,n)!=(ssize_t)n) return;
        ++retained;
    }
    char line[384];size_t n=sotc_fault_text(line,"CPU field trace summary: attempted=");
    n+=sotc_fault_hex(line+n,end);n+=sotc_fault_text(line+n," retained=");
    n+=sotc_fault_hex(line+n,retained);n+=sotc_fault_text(line+n," older_than_ring=");
    n+=sotc_fault_hex(line+n,first);n+=sotc_fault_text(line+n," busy_dropped=");
    n+=sotc_fault_hex(line+n,atomic_load_explicit(&sotc_trace_busy_drops,memory_order_relaxed));
    n+=sotc_fault_text(line+n," skipped_in_window=");n+=sotc_fault_hex(line+n,end-start-retained);
    n+=sotc_fault_text(line+n," budget_omitted=");n+=sotc_fault_hex(line+n,start-first);
    n+=sotc_fault_text(line+n," unchanged_suppressed=");
    n+=sotc_fault_hex(line+n,atomic_load_explicit(&sotc_trace_suppressed,memory_order_relaxed));
    n+=sotc_fault_text(line+n," filtered=");
    n+=sotc_fault_hex(line+n,atomic_load_explicit(&sotc_trace_filtered,memory_order_relaxed));
    n+=sotc_fault_text(line+n," image_base=");n+=sotc_fault_hex(line+n,sotc_trace_image_base);
    line[n++]='\n';(void)write(fd,line,n);
}
static void sotc_cpu_field_trace_dump(int fd) {
    sotc_cpu_field_trace_dump_limited(fd,SOTC_TRACE_RECORDS);
}

#include "fiber_failure_diagnostics.h"
static inline void sotc_cpu_field_trace_fiber_failure(int fd,const SotcFiberFailure *evidence) {
    sotc_cpu_field_trace_freeze();
    if (sotc_fiber_failure_prepare(fd)) {
        sotc_fiber_failure_write(fd,evidence);
        sotc_cpu_field_trace_dump(fd);
    }
}

/* One small, nonblocking write per fatal entry, including a nested reporter.
 * No locks, allocation, guest reads, provider metadata or waiting for another
 * reporter. A full/closed/unsupported output may lose this minimum too. */
static void sotc_cpu_field_trace_fatal_minimum(int fd,unsigned signal_number,
                                             uint64_t address,uint64_t pc,uint64_t lr) {
    sotc_cpu_field_trace_freeze();
    const uint64_t end=atomic_load_explicit(&sotc_trace_next,memory_order_acquire);
    uint64_t values[8]={0};unsigned sampled=0;
    for (unsigned back=0;back<8 && back<end;++back) {
        const uint64_t ticket=end-1-back;
        const SotcTraceRecord *r=&sotc_trace_records[ticket%SOTC_TRACE_RECORDS];
        const uint64_t sequence=atomic_load_explicit(&r->sequence,memory_order_acquire);
        if (sequence!=ticket+1) continue;
        const uint64_t snapshot[]={sequence,atomic_load_explicit(&r->site,memory_order_relaxed),
            atomic_load_explicit(&r->object,memory_order_relaxed),atomic_load_explicit(&r->field,memory_order_relaxed),
            atomic_load_explicit(&r->value,memory_order_relaxed),atomic_load_explicit(&r->status,memory_order_relaxed),
            atomic_load_explicit(&r->thread,memory_order_relaxed),atomic_load_explicit(&r->ticks,memory_order_relaxed)};
        atomic_thread_fence(memory_order_acquire);
        if (atomic_load_explicit(&r->sequence,memory_order_acquire)!=sequence) continue;
        for (unsigned i=0;i<8;++i) values[i]=snapshot[i];
        sampled=1;break;
    }
    char line[512];
    _Static_assert(sizeof(line)<=PIPE_BUF,"Fatal minimum must fit one atomic pipe write");
    size_t n=sotc_fault_text(line,"CPU fatal minimum:");
    const uint64_t fields[]={signal_number,address,pc,lr,(uintptr_t)pthread_self(),end,sampled};
    static const char *const labels[]={" signal="," address="," pc="," lr="," reporter="," next="," sample="};
    for(unsigned i=0;i<7;++i) {n+=sotc_fault_text(line+n,labels[i]);n+=sotc_fault_hex(line+n,fields[i]);}
    if (sampled) {
        static const char *const names[]={" seq="," site="," object="," field="," value="," status="," thread="," ns="};
        for(unsigned i=0;i<8;++i) {n+=sotc_fault_text(line+n,names[i]);n+=sotc_fault_hex(line+n,values[i]);}
    }
    line[n++]='\n';
    const int flags=fcntl(fd,F_GETFL);struct stat info;
#ifdef F_SETNOSIGPIPE
    (void)fcntl(fd,F_SETNOSIGPIPE,1);
#endif
    sigset_t blocked;sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);
    if (!pthread_sigmask(SIG_BLOCK,&blocked,NULL) && flags>=0 && !fstat(fd,&info) &&
        (S_ISFIFO(info.st_mode) || S_ISSOCK(info.st_mode)) &&
        ((flags&O_NONBLOCK) || !fcntl(fd,F_SETFL,flags|O_NONBLOCK)))
        (void)write(fd,line,n);
}
#endif
