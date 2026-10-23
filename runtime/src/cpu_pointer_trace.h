/* Original opt-in, bounded pointer-chain and imported-copy observations.
 * Requires cpu_field_trace.h. No guest/code/protection edits, driver callbacks,
 * exhaustive writer claim or automatic game retry. Selections stay private.
 */
#ifndef SOTC_CPU_POINTER_TRACE_H
#define SOTC_CPU_POINTER_TRACE_H
#include "guest_failure_state.h"
enum { SOTC_POINTER_SITES=12, SOTC_POINTER_WATCHES=16,
       SOTC_POINTER_PERSISTENT=8192, SOTC_POINTER_HASH_TRIES=16 };
/* Optional fifth selection word is 1 for a single pointer-valued field.
 * This mode never follows the loaded value. Fourth word must be zero. */
/* Sixth word: 0 rotating, 1 persistent (never evicted), 2 observation only.
 * Optional seventh/eighth words index the second chain read: register:scale.
 * A persistent numeric address is not an allocation or ownership identity. */
typedef struct { uint64_t site; unsigned reg; int64_t offset, inner; unsigned field_only, policy, indexed, index_reg, scale; } SotcPointerSite;
typedef struct {
    _Atomic unsigned busy;
    _Atomic uint64_t stamp, object, source, header, field, field_only;
    _Atomic uint64_t last_value, last_ns, last_valid;
} SotcPointerWatch;
static SotcPointerSite sotc_pointer_sites[SOTC_POINTER_SITES];
static unsigned sotc_pointer_count;
static SotcPointerWatch sotc_pointer_watches[SOTC_POINTER_WATCHES];
static SotcPointerWatch sotc_pointer_persistent[SOTC_POINTER_PERSISTENT];
static _Atomic uint64_t sotc_pointer_pages[256],sotc_pointer_persistent_used;
static _Atomic uint64_t sotc_pointer_next;
static uint64_t sotc_pointer_image, sotc_pointer_size;
static SotcEventJournal sotc_pointer_journal;
static _Thread_local unsigned sotc_pointer_entered;

static int sotc_pointer_signed(const char **p,int64_t *out) {
    const int negative=**p=='-';if (negative || **p=='+') ++*p;
    uint64_t n;
    if (sotc_fault_number(p,0,(uint64_t)INT64_MAX+(unsigned)negative,&n)) return -1;
    *out=negative ? (n==(uint64_t)INT64_MAX+1 ? INT64_MIN : -(int64_t)n) : (int64_t)n;
    return 0;
}
static int sotc_pointer_init(const char *config) {
    if (!config || !*config) {sotc_pointer_count=0;return 0;}
    const char *p=config;size_t length=0;SotcPointerSite selected[SOTC_POINTER_SITES];unsigned count=0;
    while (length<2048 && p[length]) ++length;
    if (length==2048) return -1;
    while (*p) {
        uint64_t site,reg,field_only=0,policy=0,index_reg=0,scale=0;unsigned indexed=0;int64_t offset,inner;
        if (count==SOTC_POINTER_SITES || sotc_fault_number(&p,0,UINT64_MAX,&site) || *p++!=':' ||
            sotc_fault_number(&p,10,15,&reg) || *p++!=':' || sotc_pointer_signed(&p,&offset) ||
            *p++!=':' || sotc_pointer_signed(&p,&inner)) return -1;
        if(*p==':') {++p;if(sotc_fault_number(&p,10,1,&field_only))return -1;}
        if(*p==':') {++p;if(sotc_fault_number(&p,10,2,&policy))return -1;}
        if(*p==':') {
            ++p;indexed=1;
            if(sotc_fault_number(&p,10,15,&index_reg) || *p++!=':' ||
               sotc_fault_number(&p,10,8,&scale) || (scale!=1 && scale!=2 && scale!=4 && scale!=8))return -1;
        }
        if(field_only && indexed)return -1;
        if((*p && *p!=',') || (field_only && inner))return -1;
        for(unsigned i=0;i<count;++i)if(selected[i].site==site && selected[i].reg==reg &&
            selected[i].offset==offset && selected[i].inner==inner && selected[i].field_only==field_only &&
            selected[i].policy==policy && selected[i].indexed==indexed && selected[i].index_reg==index_reg && selected[i].scale==scale)return -1;
        selected[count++]=(SotcPointerSite){site,(unsigned)reg,offset,inner,(unsigned)field_only,(unsigned)policy,indexed,(unsigned)index_reg,(unsigned)scale};
        if(*p==',' && !*++p)return -1;
    }
    memcpy(sotc_pointer_sites,selected,count*sizeof(*selected));sotc_pointer_count=count;return 0;
}
static int sotc_pointer_add(uint64_t base,int64_t offset,uint64_t *out) {
    const uint64_t magnitude=offset<0 ? (uint64_t)(-(offset+1))+1 : (uint64_t)offset;
    if(offset<0 ? base<magnitude : base>UINT64_MAX-magnitude)return 0;
    *out=offset<0 ? base-magnitude : base+magnitude;return 1;
}
/* First out-of-guest-range link is a selection clue, not type/lifetime proof.
 * It is retained separately from hot tails and never changes guest execution. */
static uint64_t sotc_pointer_now(void) {
    struct timespec now;uint64_t ns=0;
    if(!clock_gettime(CLOCK_MONOTONIC,&now))ns=(uint64_t)now.tv_sec*UINT64_C(1000000000)+now.tv_nsec;
    return ns;
}
static void sotc_pointer_record(unsigned channel,uint64_t ns,uint64_t data[SOTC_JOURNAL_DATA_WORDS],const uint64_t previous[3]) {
    sotc_event_journal_record(&sotc_pointer_journal,channel,ns,data);
    /* Do not classify an ARM64 top-byte tag alone as an out-of-range link. */
    if(sotc_pointer_journal.map && (data[6]&8) &&
       (data[5]&UINT64_C(0x00ffffffffffffff))>=(UINT64_C(1)<<40)) {
        uint64_t *focus=sotc_pointer_journal.map+160,expected=0;
        if(__atomic_compare_exchange_n(focus,&expected,UINT64_MAX,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)) {
            __atomic_store_n(focus+1,ns,__ATOMIC_RELAXED);
            for(unsigned i=0;i<SOTC_JOURNAL_DATA_WORDS;++i)__atomic_store_n(focus+2+i,data[i],__ATOMIC_RELAXED);
            for(unsigned i=0;i<3;++i)__atomic_store_n(focus+13+i,previous[i],__ATOMIC_RELAXED);
            __atomic_store_n(focus,1,__ATOMIC_RELEASE);
        }
    }
}
static void sotc_pointer_stat(unsigned word) {
    if(sotc_pointer_journal.map)__atomic_fetch_add(sotc_pointer_journal.map+word,1,__ATOMIC_RELAXED);
}
static unsigned sotc_pointer_hash(uint64_t field) {
    uint64_t key=field>>3;key^=key>>17;key*=UINT64_C(0x9e3779b97f4a7c15);key^=key>>32;
    return (unsigned)key&(SOTC_POINTER_PERSISTENT-1);
}
static unsigned sotc_pointer_page_bit(uint64_t field) {
    uint64_t page=field>>12;page^=page>>14;return (unsigned)page&16383;
}
static SotcPointerWatch *sotc_pointer_find(uint64_t object,uint64_t source,uint64_t header,uint64_t field,unsigned field_only,unsigned policy) {
    if(policy==1) {
        /* Immutable key, bounded probing, no eviction. An interrupted or
         * contended admission is refused, never waited for or called covered. */
        for(unsigned n=0;n<SOTC_POINTER_HASH_TRIES;++n) {
            SotcPointerWatch *w=&sotc_pointer_persistent[(sotc_pointer_hash(field)+n)&(SOTC_POINTER_PERSISTENT-1)];
            if(atomic_load_explicit(&w->stamp,memory_order_acquire)) {
                if(atomic_load_explicit(&w->field,memory_order_relaxed)==field)return w;
                continue;
            }
            unsigned expected=0;
            if(!atomic_compare_exchange_strong_explicit(&w->busy,&expected,1,memory_order_acquire,memory_order_relaxed)) {
                sotc_pointer_stat(10);sotc_pointer_stat(177);return NULL;
            }
            if(atomic_load_explicit(&w->stamp,memory_order_relaxed)) {
                const int match=atomic_load_explicit(&w->field,memory_order_relaxed)==field;
                atomic_store_explicit(&w->busy,0,memory_order_release);
                if(match)return w;
                continue;
            }
            atomic_store_explicit(&w->object,object,memory_order_relaxed);
            atomic_store_explicit(&w->source,source,memory_order_relaxed);
            atomic_store_explicit(&w->header,header,memory_order_relaxed);
            atomic_store_explicit(&w->field_only,field_only,memory_order_relaxed);
            atomic_store_explicit(&w->field,field,memory_order_relaxed);
            const unsigned bit=sotc_pointer_page_bit(field);
            atomic_fetch_or_explicit(sotc_pointer_pages+bit/64,UINT64_C(1)<<(bit%64),memory_order_relaxed);
            atomic_store_explicit(&w->stamp,1,memory_order_release);
            atomic_fetch_add_explicit(&sotc_pointer_persistent_used,1,memory_order_release);
            atomic_store_explicit(&w->busy,0,memory_order_release);
            sotc_pointer_stat(9);sotc_pointer_stat(176);return w;
        }
        sotc_pointer_stat(177);return NULL;
    }
    /* One immutable publication per slot; readers never wait for a writer. */
    for(unsigned i=0;i<SOTC_POINTER_WATCHES;++i)
        if(atomic_load_explicit(&sotc_pointer_watches[i].stamp,memory_order_acquire) &&
           atomic_load_explicit(&sotc_pointer_watches[i].field,memory_order_relaxed)==field)return &sotc_pointer_watches[i];
    const uint64_t ticket=atomic_fetch_add_explicit(&sotc_pointer_next,1,memory_order_relaxed);
    SotcPointerWatch *w=&sotc_pointer_watches[ticket%SOTC_POINTER_WATCHES];unsigned expected=0;
    if(!atomic_compare_exchange_strong_explicit(&w->busy,&expected,1,memory_order_acquire,memory_order_relaxed)) {
        if(sotc_pointer_journal.map)__atomic_fetch_add(sotc_pointer_journal.map+10,1,__ATOMIC_RELAXED);
        return NULL;
    }
    const uint64_t previous=atomic_load_explicit(&w->stamp,memory_order_relaxed);
    if(previous>ticket+1) {atomic_store_explicit(&w->busy,0,memory_order_release);return NULL;}
    atomic_store_explicit(&w->stamp,0,memory_order_release);
    atomic_store_explicit(&w->object,object,memory_order_relaxed);
    atomic_store_explicit(&w->source,source,memory_order_relaxed);
    atomic_store_explicit(&w->header,header,memory_order_relaxed);
    atomic_store_explicit(&w->field_only,field_only,memory_order_relaxed);
    atomic_store_explicit(&w->field,field,memory_order_release);
    atomic_store_explicit(&w->last_valid,0,memory_order_relaxed);
    atomic_store_explicit(&w->stamp,ticket+1,memory_order_release);
    atomic_store_explicit(&w->busy,0,memory_order_release);
    if(sotc_pointer_journal.map) {
        __atomic_fetch_add(sotc_pointer_journal.map+9,1,__ATOMIC_RELAXED);
        if(previous)__atomic_fetch_add(sotc_pointer_journal.map+11,1,__ATOMIC_RELAXED);
    }
    return w;
}
static void sotc_pointer_history(SotcPointerWatch *w,uint64_t field,uint64_t value,uint64_t ns,uint64_t previous[3]) {
    if(!w)return;
    unsigned expected=0;
    if(!atomic_compare_exchange_strong_explicit(&w->busy,&expected,1,memory_order_acquire,memory_order_relaxed)) {
        sotc_pointer_stat(178);return;
    }
    if(atomic_load_explicit(&w->stamp,memory_order_relaxed) && atomic_load_explicit(&w->field,memory_order_relaxed)==field) {
        previous[0]=atomic_load_explicit(&w->last_valid,memory_order_relaxed);
        previous[1]=atomic_load_explicit(&w->last_value,memory_order_relaxed);
        previous[2]=atomic_load_explicit(&w->last_ns,memory_order_relaxed);
        atomic_store_explicit(&w->last_value,value,memory_order_relaxed);
        atomic_store_explicit(&w->last_ns,ns,memory_order_relaxed);
        atomic_store_explicit(&w->last_valid,1,memory_order_relaxed);
    }
    atomic_store_explicit(&w->busy,0,memory_order_release);
}
static void sotc_pointer_probe(const uint64_t regs[16],uint64_t site,uint32_t tag) {
    if(tag>=sotc_pointer_count || atomic_load_explicit(&sotc_trace_frozen,memory_order_acquire))return;
    const int saved=errno;const SotcPointerSite selected=sotc_pointer_sites[tag];
    uint64_t source=0,header=0,field=0,value=0,status=0;
    if(sotc_pointer_add(regs[selected.reg],selected.offset,&source)) {
        status|=1;
        if(selected.field_only) {
            /* A readable field can contain an unreadable pointer. Retain it
             * without chasing it, including when it was already bad on entry. */
            field=source;status|=4;
            if(field<=UINT64_MAX-7 && sotc_fault_read(&value,field,1)==1)status|=2|8;
        } else if(sotc_fault_read(&header,source,1)==1) {
            status|=2;
            int valid=sotc_pointer_add(header,selected.inner,&field);
            if(valid && selected.indexed) {
                const uint64_t index=regs[selected.index_reg];
                if(index>UINT64_MAX/selected.scale || field>UINT64_MAX-index*selected.scale)valid=0;
                else field+=index*selected.scale;
            }
            if(valid && field<=UINT64_MAX-7) {
                status|=4;
                if(sotc_fault_read(&value,field,1)==1)status|=8;
            }
        }
    }
    if(selected.field_only)status|=512;
    if(selected.policy==1)status|=1024;
    if(selected.policy==2)status|=2048;
    if(selected.indexed)status|=4096;
    status|=16384; /* Per-event watch-admission evidence is available. */
    uint64_t data[SOTC_JOURNAL_DATA_WORDS]={site,regs[selected.reg],source,header,field,value,status|(UINT64_C(1)<<16),
        selected.indexed?regs[selected.index_reg]:0,selected.indexed?selected.scale:0,selected.indexed?selected.index_reg:0,(uintptr_t)pthread_self()};
    const uint64_t ns=sotc_pointer_now();uint64_t previous[3]={0};
    if((status&15)==15 && field<(UINT64_C(1)<<40) && selected.policy!=2) {
        SotcPointerWatch *w=sotc_pointer_find(regs[selected.reg],source,header,field,selected.field_only,selected.policy);
        if(w)data[6]|=8192;
        else if(selected.policy==1)sotc_pointer_stat(192+tag);
        sotc_pointer_history(w,field,value,ns,previous);
    }
    sotc_pointer_record(tag,ns,data,previous);
    errno=saved;
}
static void sotc_pointer_copy_slot(SotcPointerWatch *w,uintptr_t address,size_t size,unsigned actor,uintptr_t input,
                                   unsigned persistent,uint64_t key,SotcGuestFailureState *state,int *have_state) {
        const uint64_t stamp=atomic_load_explicit(&w->stamp,memory_order_acquire);
        if(!stamp)return;
        if(atomic_load_explicit(&w->busy,memory_order_acquire)) {sotc_pointer_stat(182);return;}
        const uint64_t object=atomic_load_explicit(&w->object,memory_order_relaxed);
        const uint64_t source=atomic_load_explicit(&w->source,memory_order_relaxed);
        const uint64_t header=atomic_load_explicit(&w->header,memory_order_relaxed);
        const uint64_t field_only=atomic_load_explicit(&w->field_only,memory_order_relaxed);
        const uint64_t field=atomic_load_explicit(&w->field,memory_order_relaxed);
        atomic_thread_fence(memory_order_acquire);
        if(atomic_load_explicit(&w->stamp,memory_order_acquire)!=stamp ||
           atomic_load_explicit(&w->busy,memory_order_acquire)) {sotc_pointer_stat(182);return;}
        if(key!=UINT64_MAX && (field>>3)!=key)return;
        if(field>address ? field-address>=size : address-field>=8)return;
        if(!*have_state) {bootstrap_cpu_failure_state(state);*have_state=1;}
        /* Source/header relation is from registration, not reread at copy time. */
        uint64_t site=state->rip,value=0,status=5|8192|16384|(field_only?512:0)|(persistent?1024:0),caller=0;
        if(state->available && sotc_fault_read(&caller,state->regs[4],1)==1 &&
           caller>=sotc_pointer_image && caller-sotc_pointer_image<sotc_pointer_size) {site=caller;status|=256;}
        if(sotc_fault_read(&value,field,1)==1)status|=8;
        uint64_t data[SOTC_JOURNAL_DATA_WORDS]={site,object,source,header,field,value,status|((uint64_t)actor<<16),input,address,size,(uintptr_t)pthread_self()};
        const uint64_t ns=sotc_pointer_now();uint64_t previous[3]={0};
        if(status&8)sotc_pointer_history(w,field,value,ns,previous);
        sotc_pointer_record(sotc_pointer_count+((actor==3 || actor==5)?1:0),ns,data,previous);
}
/* Main's strong observer is called around the actual imported memcpy/memset.
 * Direct JIT/interpreter stores, GPU writes and other aliases are NOT hooked. */
static void sotc_pointer_copy(uintptr_t address,size_t size,unsigned actor,uintptr_t input) {
    if(!sotc_pointer_count || !size || address>UINTPTR_MAX-(size-1) || sotc_pointer_entered ||
        atomic_load_explicit(&sotc_trace_frozen,memory_order_acquire))return;
    const int saved=errno;sotc_pointer_entered=1;
    SotcGuestFailureState state={0};int have_state=0;
    for(unsigned i=0;i<SOTC_POINTER_WATCHES;++i)
        sotc_pointer_copy_slot(&sotc_pointer_watches[i],address,size,actor,input,0,UINT64_MAX,&state,&have_state);
    if(atomic_load_explicit(&sotc_pointer_persistent_used,memory_order_acquire)) {
        /* Conservative page filter: false positives only. Small copies inspect
         * bounded hash buckets; large copies scan the fixed table. Keys include
         * unaligned field starts overlapping the preceding seven bytes. */
        const uint64_t low=address>=7?address-7:0,high=address+size-1;
        int maybe=(high>>12)-(low>>12)>64;
        for(uint64_t page=low>>12;!maybe && page<=high>>12;++page) {
            const unsigned bit=sotc_pointer_page_bit(page<<12);
            maybe=(atomic_load_explicit(sotc_pointer_pages+bit/64,memory_order_acquire)>>(bit%64))&1;
        }
        if(maybe && size<=512) {
            for(uint64_t key=low>>3;key<=high>>3;++key)
                for(unsigned n=0;n<SOTC_POINTER_HASH_TRIES;++n)
                    sotc_pointer_copy_slot(&sotc_pointer_persistent[(sotc_pointer_hash(key<<3)+n)&(SOTC_POINTER_PERSISTENT-1)],
                        address,size,actor,input,1,key,&state,&have_state);
        } else if(maybe) {
            for(unsigned i=0;i<SOTC_POINTER_PERSISTENT;++i)
                sotc_pointer_copy_slot(&sotc_pointer_persistent[i],address,size,actor,input,1,UINT64_MAX,&state,&have_state);
        }
    }
    sotc_pointer_entered=0;errno=saved;
}
#endif
