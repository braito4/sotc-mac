/* Included after the actual memory backend. Diagnostic only: no write, trap,
 * protection change, GPU callback or ledger lock while holding the VMA lock. */
#ifndef SOTC_BUFFER_BINDING_AUDIT_RUNTIME_H
#define SOTC_BUFFER_BINDING_AUDIT_RUNTIME_H
#include <mach/mach.h>
#include <mach/mach_vm.h>
static pthread_once_t sotc_binding_audit_once=PTHREAD_ONCE_INIT;
static int sotc_binding_audit_on;
static unsigned sotc_binding_audit_reports;
static void sotc_binding_audit_init(void){const char *s=getenv("SOTC_BINDING_AUDIT");sotc_binding_audit_on=s&&!strcmp(s,"1");}
typedef struct {
    uint64_t guest,physical,bytes,vma_start,vma_end,vma_physical,begin_ns,end_ns;
    int kind,prot,type;unsigned guest_read,backing_read;
    unsigned char guest_data[64],backing_data[64];
} SotcBindingSample;
/* Called with NO renderer registry or runtime memory lock held. VM reads can
 * fail on protected guest aliases: report that, never unprotect or retry. */
void sotc_buffer_binding_audit_v1(unsigned sequence,unsigned slot,unsigned phase,
                                 uint64_t guest,uint64_t bytes) {
    const int saved=errno;pthread_once(&sotc_binding_audit_once,sotc_binding_audit_init);
    if(!sotc_binding_audit_on||!sequence||sequence>32||slot>255||phase>1||!sotc_page_span(guest,bytes)||bytes>(8ull<<20)){errno=saved;return;}
    if(__atomic_fetch_add(&sotc_binding_audit_reports,1,__ATOMIC_RELAXED)>=32){fprintf(stderr,"Binding audit: report budget exhausted\n");errno=saved;return;}
    SotcBindingSample samples[64]={0};unsigned count=0;uint64_t at=guest,end=guest+bytes;
    read_lock();const uint64_t generation=__atomic_load_n(&table_generation,__ATOMIC_ACQUIRE);
    const uint64_t begin=sotc_page_now();
    while(at<end&&count<64){
        const size_t i=vma_index(at);SotcBindingSample *s=&samples[count++];s->guest=at;s->begin_ns=sotc_page_now();
        if(i==vma_count||vmas[i].start>at||vmas[i].kind==KIND_RESERVED){
            uint64_t next=i==vma_count?end:vmas[i].start>at?vmas[i].start:vmas[i].end;
            s->bytes=(next<end?next:end)-at;at+=s->bytes;s->end_ns=sotc_page_now();continue;
        }
        const Vma v=vmas[i];s->kind=v.kind;s->prot=v.prot;s->type=v.type;
        s->vma_start=v.start;s->vma_end=v.end;s->vma_physical=v.phys;
        s->physical=v.phys+(at-v.start);
        const uint64_t bound=v.end<end?v.end:end;
        const uint64_t bucket_room=SOTC_PAGE_BUCKET_SIZE-(s->physical&(SOTC_PAGE_BUCKET_SIZE-1));
        const uint64_t step=bound-at<bucket_room?bound-at:bucket_room;
        s->bytes=step<64?step:64;
        if(backing_base&&sotc_page_span(s->physical,s->bytes)&&s->physical+s->bytes<=POOL_SIZE+FLEX_SPAN){
            for(unsigned j=0;j<s->bytes;++j)s->backing_data[j]=__atomic_load_n(backing_base+s->physical+j,__ATOMIC_RELAXED);
            s->backing_read=1;
        }
        mach_vm_size_t copied=0;
        s->guest_read=mach_vm_read_overwrite(mach_task_self(),(mach_vm_address_t)at,s->bytes,
            (mach_vm_address_t)(uintptr_t)s->guest_data,&copied)==KERN_SUCCESS&&copied==s->bytes;
        s->end_ns=sotc_page_now();at+=step;
    }
    const uint64_t finish=sotc_page_now();read_unlock();
    const char *dir=getenv("SOTC_SCATTER_GUARD_DIR");char path[4096];int fd=-1;
    if(dir&&dir[0]=='/') {int n=snprintf(path,sizeof(path),"%s/binding-audit-%u-slot-%u-%s.json",dir,sequence,slot,phase?"after":"before");if(n>0&&(size_t)n<sizeof(path))fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);}
    FILE *f=fd>=0?fdopen(fd,"w"):NULL;if(!f&&fd>=0)close(fd);int ok=0;
    if(f){
        fprintf(f,"{\"schema\":1,\"clock\":\"CLOCK_MONOTONIC\",\"sequence\":%u,\"slot\":%u,\"phase\":%u,\"guest\":%llu,\"bytes\":%llu,\"generation\":%llu,\"begin_ns\":%llu,\"end_ns\":%llu,\"truncated\":%s,\"samples\":[",sequence,slot,phase,(unsigned long long)guest,(unsigned long long)bytes,(unsigned long long)generation,(unsigned long long)begin,(unsigned long long)finish,at<end?"true":"false");
        for(unsigned i=0;i<count;++i){const SotcBindingSample *s=samples+i;
            fprintf(f,"%s{\"guest\":%llu,\"physical\":%llu,\"bytes\":%llu,\"vma_start\":%llu,\"vma_end\":%llu,\"vma_physical\":%llu,\"kind\":%d,\"prot\":%d,\"type\":%d,\"guest_read\":%u,\"backing_read\":%u,\"begin_ns\":%llu,\"end_ns\":%llu,\"guest_hex\":\"",i?",":"",(unsigned long long)s->guest,(unsigned long long)s->physical,(unsigned long long)s->bytes,(unsigned long long)s->vma_start,(unsigned long long)s->vma_end,(unsigned long long)s->vma_physical,s->kind,s->prot,s->type,s->guest_read,s->backing_read,(unsigned long long)s->begin_ns,(unsigned long long)s->end_ns);
            if(s->guest_read)for(unsigned j=0;j<s->bytes;++j)fprintf(f,"%02x",s->guest_data[j]);
            fputs("\",\"backing_hex\":\"",f);if(s->backing_read)for(unsigned j=0;j<s->bytes;++j)fprintf(f,"%02x",s->backing_data[j]);fputs("\"}",f);
        }
        fputs("],\"limits\":\"Sparse sequential CPU observations under VMA lock, not atomic or GPU-synchronized. Before is before producer encoding; after is after the existing producer completion wait. GPU-before snapshot is a different instant. Protected/unreadable guest aliases stay unreadable. Mapping generation is not allocation identity.\"}\n",f);
        ok=!ferror(f)&&fflush(f)==0&&fsync(fd)==0;if(fclose(f)!=0)ok=0;
    }
    fprintf(stderr,"Binding audit: sequence=%u slot=%u phase=%u samples=%u truncated=%u saved=%d\n",sequence,slot,phase,count,at<end,ok);errno=saved;
}
#endif
