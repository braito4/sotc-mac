/* Original bounded backing-write overlap probes. Requires cpu_pointer_trace.h.
 * Protected prefix reads and guest-state queries occur outside the VMA lock.
 * No retention, permissions, success result or queue ordering is changed. */
#ifndef SOTC_SHADER_BACKING_TRACE_H
#define SOTC_SHADER_BACKING_TRACE_H
extern void sotc_memory_set_backing_write_observer(
    void (*observer)(uintptr_t,const void*,uint64_t,unsigned,int,uintptr_t));
static SotcEventJournal sotc_shader_backing_journal;
static _Thread_local unsigned sotc_shader_backing_entered;
enum { SOTC_SHADER_BINDING_HISTORY=4096,SOTC_SHADER_BINDING_MATCHES=8 };
typedef struct {
    _Atomic unsigned busy;
    _Atomic uint64_t stamp,address,size,shader,slot,declared,thread,ns,route;
} SotcShaderBindingHistory;
typedef struct { uint64_t stamp,address,size,shader,slot,declared,thread,ns,route; }
    SotcShaderBindingValue;
static SotcShaderBindingHistory sotc_shader_binding_history[SOTC_SHADER_BINDING_HISTORY];
static _Atomic uint64_t sotc_shader_binding_sequence;
typedef char SotcShaderBindingHistoryBudget[
    (sizeof(sotc_shader_binding_history)<=393216)?1:-1];

/* Keep recent writable descriptor ranges even when no shader prefix has been
 * registered yet. This is numeric write intent only: it neither proves a GPU
 * store nor retains guest memory. Publication is bounded and never waits. */
static void sotc_shader_backing_binding_history(uintptr_t address,uint64_t size,
        uint64_t shader,uint64_t slot,uint64_t declared,unsigned route) {
    if(!sotc_shader_prefix_enabled||!sotc_shader_backing_journal.map||!size||
       address>UINTPTR_MAX-(size-1)||route>4||
       atomic_load_explicit(&sotc_trace_frozen,memory_order_acquire))return;
    const uint64_t ticket=atomic_fetch_add_explicit(&sotc_shader_binding_sequence,1,
        memory_order_relaxed)+1;
    SotcShaderBindingHistory *record=&sotc_shader_binding_history[
        (ticket-1)%SOTC_SHADER_BINDING_HISTORY];
    unsigned expected=0;
    if(!atomic_compare_exchange_strong_explicit(&record->busy,&expected,1,
        memory_order_acquire,memory_order_relaxed)) {
        __atomic_fetch_add(sotc_shader_backing_journal.map+280,1,__ATOMIC_RELAXED);return;
    }
    const uint64_t previous=atomic_load_explicit(&record->stamp,memory_order_relaxed);
    if(previous>ticket) {
        atomic_store_explicit(&record->busy,0,memory_order_release);
        __atomic_fetch_add(sotc_shader_backing_journal.map+280,1,__ATOMIC_RELAXED);return;
    }
    atomic_store_explicit(&record->stamp,0,memory_order_release);
    atomic_store_explicit(&record->address,address,memory_order_relaxed);
    atomic_store_explicit(&record->size,size,memory_order_relaxed);
    atomic_store_explicit(&record->shader,shader,memory_order_relaxed);
    atomic_store_explicit(&record->slot,slot,memory_order_relaxed);
    atomic_store_explicit(&record->declared,declared,memory_order_relaxed);
    atomic_store_explicit(&record->thread,(uintptr_t)pthread_self(),memory_order_relaxed);
    atomic_store_explicit(&record->ns,sotc_pointer_now(),memory_order_relaxed);
    atomic_store_explicit(&record->route,route,memory_order_relaxed);
    atomic_store_explicit(&record->stamp,ticket,memory_order_release);
    atomic_store_explicit(&record->busy,0,memory_order_release);
    __atomic_fetch_add(sotc_shader_backing_journal.map+279,1,__ATOMIC_RELAXED);
    if(previous)__atomic_fetch_add(sotc_shader_backing_journal.map+281,1,__ATOMIC_RELAXED);
}

/* Join bindings that happened shortly before a prefix became observable. The
 * summary remains explicit even when no range matched, so absence is not
 * confused with an unscanned history window. */
static void sotc_shader_backing_prefix_registered(uint64_t address,uint64_t packet,
        uint64_t capture) {
    if(!sotc_shader_prefix_enabled||!sotc_shader_backing_journal.map||!address||!capture||
       atomic_load_explicit(&sotc_trace_frozen,memory_order_acquire))return;
    const uint64_t registered_ns=sotc_pointer_now();
    SotcShaderBindingValue matches[SOTC_SHADER_BINDING_MATCHES]={0};
    unsigned count=0,stable=0,unstable=0;uint64_t oldest=0,newest=0;
    for(unsigned i=0;i<SOTC_SHADER_BINDING_HISTORY;++i) {
        SotcShaderBindingHistory *record=&sotc_shader_binding_history[i];
        if(atomic_load_explicit(&record->busy,memory_order_acquire)){++unstable;continue;}
        const uint64_t stamp=atomic_load_explicit(&record->stamp,memory_order_acquire);
        if(!stamp)continue;
        SotcShaderBindingValue value={stamp,
            atomic_load_explicit(&record->address,memory_order_relaxed),
            atomic_load_explicit(&record->size,memory_order_relaxed),
            atomic_load_explicit(&record->shader,memory_order_relaxed),
            atomic_load_explicit(&record->slot,memory_order_relaxed),
            atomic_load_explicit(&record->declared,memory_order_relaxed),
            atomic_load_explicit(&record->thread,memory_order_relaxed),
            atomic_load_explicit(&record->ns,memory_order_relaxed),
            atomic_load_explicit(&record->route,memory_order_relaxed)};
        atomic_thread_fence(memory_order_acquire);
        if(atomic_load_explicit(&record->busy,memory_order_acquire)||
           atomic_load_explicit(&record->stamp,memory_order_acquire)!=stamp){++unstable;continue;}
        const uint64_t ns=value.ns;
        if(!ns||ns>registered_ns)continue;
        ++stable;if(!oldest||ns<oldest)oldest=ns;if(ns>newest)newest=ns;
        const uint64_t begin=value.address;
        const uint64_t size=value.size;
        if(address<begin||address-begin>=size)continue;
        unsigned selected=count;
        if(count<SOTC_SHADER_BINDING_MATCHES)++count;
        else {
            selected=0;
            for(unsigned n=1;n<count;++n)
                if(matches[n].stamp<matches[selected].stamp)selected=n;
            if(stamp<=matches[selected].stamp)continue;
        }
        matches[selected]=value;
    }
    const uint64_t summary[SOTC_JOURNAL_DATA_WORDS]={address,packet,capture,0,32768,
        oldest,newest?newest:1,count,(uintptr_t)pthread_self(),stable,
        atomic_load_explicit(&sotc_shader_binding_sequence,memory_order_relaxed)};
    sotc_event_journal_record(&sotc_shader_backing_journal,0,registered_ns,summary);
    __atomic_fetch_add(sotc_shader_backing_journal.map+282,1,__ATOMIC_RELAXED);
    if(unstable)__atomic_fetch_add(sotc_shader_backing_journal.map+283,unstable,__ATOMIC_RELAXED);
    for(unsigned i=0;i<count;++i) {
        const unsigned route=(unsigned)matches[i].route;
        const uint64_t data[SOTC_JOURNAL_DATA_WORDS]={address,packet,capture,0,
            UINT64_C(32)|UINT64_C(32768)|((uint64_t)route<<6),
            matches[i].address,matches[i].size,matches[i].shader,matches[i].thread,
            matches[i].slot,matches[i].declared};
        sotc_event_journal_record(&sotc_shader_backing_journal,0,
            matches[i].ns,data);
        __atomic_fetch_add(sotc_shader_backing_journal.map+284,1,__ATOMIC_RELAXED);
    }
}
static void sotc_shader_backing_slot(unsigned i,uintptr_t address,const void* input,uint64_t size,
        unsigned after,int success,uintptr_t caller,uint64_t sequence) {
    SotcPointerWatch *w=sotc_shader_prefix_watches+i;
    const uint64_t stamp=atomic_load_explicit(&w->stamp,memory_order_acquire);
    if(!stamp||atomic_load_explicit(&w->busy,memory_order_acquire))return;
    uint64_t field=atomic_load_explicit(&w->field,memory_order_relaxed);
    if(field>address?field-address>=size:address-field>=8)return;
    uint64_t packet=atomic_load_explicit(&w->source,memory_order_relaxed);
    uint64_t capture=atomic_load_explicit(&w->header,memory_order_relaxed);
    if(atomic_load_explicit(&w->stamp,memory_order_acquire)!=stamp||
       atomic_load_explicit(&w->busy,memory_order_acquire))return;
    const unsigned notification=after==2,gpu_intent=after>=3&&after<=13,binding=after>=14&&after<=18;
    const unsigned binding_route=binding?after-14:0;
    unsigned channel=after==1;
    uint64_t head=0,status=binding?(32|((uint64_t)binding_route<<6)):gpu_intent?(16|((uint64_t)(after-2)<<8)):notification?8:after?(success?2:4):0;
    if(sotc_fault_read(&head,field,1)==1)status|=1;
    if(binding&&(status&1)&&(uint32_t)head!=UINT32_C(0xbeeb03ff))channel=1;
    SotcGuestFailureState state={0};bootstrap_cpu_failure_state(&state);
    uint64_t data[SOTC_JOURNAL_DATA_WORDS]={field,packet,capture,head,status,address,size,
        (uintptr_t)input,(uintptr_t)pthread_self(),(gpu_intent||binding)?sequence:state.available?state.rip:0,caller};
    sotc_event_journal_record(&sotc_shader_backing_journal,channel,sotc_pointer_now(),data);
    __atomic_fetch_add(sotc_shader_backing_journal.map+(binding?(binding_route==4?290:binding_route==3?288:binding_route==2?286:binding_route?277:275):gpu_intent?259:notification?258:256+after),1,__ATOMIC_RELAXED);
}
/* Successful CPU stores emitted by the checked ARM64 JIT overlay. The event
 * records the translated instruction and source value without rereading input.
 * This covers scalar/vector direct stores only; it is not a complete write log. */
static void sotc_shader_backing_cpu_write_slot(unsigned i,uintptr_t address,uint64_t size,
        uint64_t low,uint64_t high,uint64_t rip,unsigned route) {
    SotcPointerWatch *w=sotc_shader_prefix_watches+i;
    const uint64_t stamp=atomic_load_explicit(&w->stamp,memory_order_acquire);
    if(!stamp||atomic_load_explicit(&w->busy,memory_order_acquire))return;
    const uint64_t field=atomic_load_explicit(&w->field,memory_order_relaxed);
    if(field>address?field-address>=size:address-field>=8)return;
    const uint64_t packet=atomic_load_explicit(&w->source,memory_order_relaxed);
    const uint64_t capture=atomic_load_explicit(&w->header,memory_order_relaxed);
    if(atomic_load_explicit(&w->stamp,memory_order_acquire)!=stamp||
       atomic_load_explicit(&w->busy,memory_order_acquire))return;
    uint64_t head=0,status=(uint64_t)route<<32;
    if(sotc_fault_read(&head,field,1)==1)status|=1;
    const uint64_t data[SOTC_JOURNAL_DATA_WORDS]={field,packet,capture,head,status,
        address,size,low,high,rip,(uintptr_t)pthread_self()};
    sotc_event_journal_record(&sotc_shader_backing_journal,0,sotc_pointer_now(),data);
    __atomic_fetch_add(sotc_shader_backing_journal.map+(route==1?291:292),1,__ATOMIC_RELAXED);
}
void sotc_shader_backing_cpu_write(uintptr_t address,uint64_t low,uint64_t high,
        uint64_t size,uint64_t rip,unsigned route) {
    if(!sotc_shader_prefix_enabled||!sotc_shader_backing_journal.map||!size||size>16||
       (route!=1&&route!=2)||address>UINTPTR_MAX-(size-1)||
       atomic_load_explicit(&sotc_trace_frozen,memory_order_acquire))return;
    /* Eligible successful scalar/vector JIT stores, including non-overlaps.
     * Keep these separate from map[291:292], which count retained overlaps. */
    __atomic_fetch_add(sotc_shader_backing_journal.map+(route==1?293:294),1,__ATOMIC_RELAXED);
    const uint64_t last=address+size-1;
    const uint64_t first=address>=7?address-7:0;
    int maybe=(last>>12)-(first>>12)>64;
    for(uint64_t page=first>>12;!maybe&&page<=last>>12;++page) {
        const unsigned bit=sotc_pointer_page_bit(page<<12);
        maybe=(atomic_load_explicit(sotc_shader_prefix_pages+bit/64,memory_order_acquire)>>(bit%64))&1;
    }
    if(!maybe) return;
    __atomic_fetch_add(sotc_shader_backing_journal.map+(route==1?295:296),1,__ATOMIC_RELAXED);
    int bucket_candidate=0;
    for(uint64_t key=first>>8;key<=last>>8;++key) {
        const unsigned begin=sotc_shader_prefix_bucket(key<<8)*SOTC_SHADER_PREFIX_WAYS;
        for(unsigned i=begin;i<begin+SOTC_SHADER_PREFIX_WAYS;++i)
            if((atomic_load_explicit(&sotc_shader_prefix_watches[i].field,memory_order_relaxed)>>8)==key) {
                bucket_candidate=1;
                sotc_shader_backing_cpu_write_slot(i,address,size,low,high,rip,route);
            }
    }
    if(bucket_candidate)
        __atomic_fetch_add(sotc_shader_backing_journal.map+(route==1?297:298),1,__ATOMIC_RELAXED);
}
static void sotc_shader_backing_observe(uintptr_t address,const void* source,uint64_t size,
        unsigned after,int success,uintptr_t caller,uint64_t sequence) {
    if(!sotc_shader_prefix_enabled||!sotc_shader_backing_journal.map||!size||after>18||
       address>UINTPTR_MAX-(size-1)||sotc_shader_backing_entered||
       atomic_load_explicit(&sotc_trace_frozen,memory_order_acquire))return;
    const int saved=errno;sotc_shader_backing_entered=1;
    __atomic_fetch_add(sotc_shader_backing_journal.map+(after==18?289:after==17?287:after==16?285:after==15?276:260+after),1,__ATOMIC_RELAXED);
    const uint64_t low=address>=7?address-7:0,high=address+size-1;
    int maybe=(high>>12)-(low>>12)>64;
    for(uint64_t page=low>>12;!maybe&&page<=high>>12;++page){
        const unsigned bit=sotc_pointer_page_bit(page<<12);
        maybe=(atomic_load_explicit(sotc_shader_prefix_pages+bit/64,memory_order_acquire)>>(bit%64))&1;
    }
    if(maybe&&size<=512) {
        for(uint64_t key=low>>8;key<=high>>8;++key) {
            const unsigned begin=sotc_shader_prefix_bucket(key<<8)*SOTC_SHADER_PREFIX_WAYS;
            for(unsigned i=begin;i<begin+SOTC_SHADER_PREFIX_WAYS;++i)
                if(atomic_load_explicit(&sotc_shader_prefix_watches[i].field,memory_order_relaxed)>>8==key)
                    sotc_shader_backing_slot(i,address,source,size,after,success,caller,sequence);
        }
    }else if(maybe)for(unsigned i=0;i<SOTC_SHADER_PREFIX_WATCHES;++i)
        sotc_shader_backing_slot(i,address,source,size,after,success,caller,sequence);
    sotc_shader_backing_entered=0;errno=saved;
}
static void sotc_shader_backing_write(uintptr_t address,const void* source,uint64_t size,
        unsigned after,int success,uintptr_t caller) {
    sotc_shader_backing_observe(address,source,size,after,success,caller,0);
}
static void sotc_shader_backing_binding(uintptr_t address,uint64_t size,uint64_t shader,
        uint64_t slot,uint64_t declared,unsigned route) {
    sotc_shader_backing_binding_history(address,size,shader,slot,declared,route);
    sotc_shader_backing_observe(address,(const void*)(uintptr_t)shader,size,
        14+route,0,(uintptr_t)declared,slot);
}
static void sotc_shader_backing_init(void) {
    if(!sotc_shader_prefix_enabled)return;
    sotc_event_journal_init(&sotc_shader_backing_journal,"shader-backing-events.bin",2,512);
    if(sotc_shader_backing_journal.map){
        sotc_shader_backing_journal.map[9]=13;sotc_shader_backing_journal.map[15]=8;
        sotc_shader_backing_journal.map[278]=SOTC_SHADER_BINDING_HISTORY;
        sotc_memory_set_backing_write_observer(sotc_shader_backing_write);
    }
}
#endif
