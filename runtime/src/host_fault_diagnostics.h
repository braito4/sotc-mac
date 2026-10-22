/* Original bounded native fault evidence. No guest bytes, stack walking, symbol
 * lookup, allocation or recovery in the signal handler. Output stays private. */
#ifndef SOTC_HOST_FAULT_DIAGNOSTICS_H
#define SOTC_HOST_FAULT_DIAGNOSTICS_H
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

typedef struct {
    uint64_t x[29], fp, lr, sp, pc;
} SotcHostFaultRegs;

static inline size_t sotc_host_text(char *out,const char *text) {
    size_t n=0;while (*text) out[n++]=*text++;return n;
}
static inline size_t sotc_host_hex(char *out,uint64_t value) {
    size_t n=0;int started=0;out[n++]='0';out[n++]='x';
    for(int shift=60;shift>=0;shift-=4) {
        unsigned digit=(unsigned)((value>>shift)&15);
        if(digit || started || !shift) {out[n++]="0123456789abcdef"[digit];started=1;}
    }
    return n;
}
static inline void sotc_host_fault_context(int fd,const SotcHostFaultRegs *r) {
    static const char header[]="Host ARM64 context: private register snapshot; pointers and LR are candidates, not a validated call stack\n";
    (void)write(fd,header,sizeof(header)-1);
    for(unsigned first=0;first<29;first+=4) {
        char line[256];size_t n=sotc_host_text(line,"  host registers:");
        for(unsigned i=first;i<29 && i<first+4;++i) {
            line[n++]=' ';line[n++]='x';
            if(i>=10) line[n++]=(char)('0'+i/10);
            line[n++]=(char)('0'+i%10);line[n++]='=';
            n+=sotc_host_hex(line+n,r->x[i]);
        }
        line[n++]='\n';(void)write(fd,line,n);
    }
    char line[256];size_t n=sotc_host_text(line,"  host control: pc=");
    n+=sotc_host_hex(line+n,r->pc);n+=sotc_host_text(line+n," lr=");
    n+=sotc_host_hex(line+n,r->lr);n+=sotc_host_text(line+n," fp=");
    n+=sotc_host_hex(line+n,r->fp);n+=sotc_host_text(line+n," sp=");
    n+=sotc_host_hex(line+n,r->sp);line[n++]='\n';(void)write(fd,line,n);
    static const char end[]="Host ARM64 context: end; no pointed-to memory read, no recovery attempted\n";
    (void)write(fd,end,sizeof(end)-1);
}

#if defined(__APPLE__) && defined(__aarch64__)
#include <sys/ucontext.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static inline void sotc_host_fault_ucontext(int fd,const void *context) {
    const ucontext_t *uc=context;
    if(!uc || !uc->uc_mcontext) return;
    const arm_thread_state64_t *s=&uc->uc_mcontext->__ss;
    SotcHostFaultRegs r;
    for(unsigned i=0;i<29;++i) r.x[i]=s->__x[i];
    r.fp=s->__fp;r.lr=s->__lr;r.sp=s->__sp;r.pc=s->__pc;
    sotc_host_fault_context(fd,&r);
}

/* Startup only: dyld and stdio are never called by the new fatal reporter.
 * Print the loaded binary UUID and text range for later offline symbolization.
 * Symbols/code and full owner paths are not copied into a public report. */
static inline void sotc_host_image_inventory(void) {
    const uint32_t count=_dyld_image_count();
    const uint32_t limit=count<512?count:512;
    puts("Host images: private startup snapshot; later loads may be absent");
    for(uint32_t i=0;i<limit;++i) {
        const struct mach_header *header=_dyld_get_image_header(i);
        const char *name=_dyld_get_image_name(i);
        if(!header || header->magic!=MH_MAGIC_64 || !name) continue;
        const struct mach_header_64 *h=(const void *)header;
        const unsigned char *commands=(const unsigned char *)(h+1);
        size_t at=0;uint64_t text_start=0,text_size=0;
        char uuid[33]="unavailable";
        for(uint32_t j=0;j<h->ncmds && at<=h->sizeofcmds &&
            sizeof(struct load_command)<=h->sizeofcmds-at;++j) {
            const struct load_command *cmd=(const void *)(commands+at);
            if(cmd->cmdsize<sizeof(*cmd) || cmd->cmdsize>h->sizeofcmds-at) break;
            if(cmd->cmd==LC_UUID && cmd->cmdsize>=sizeof(struct uuid_command)) {
                const struct uuid_command *u=(const void *)cmd;
                for(unsigned k=0;k<16;++k) {
                    uuid[2*k]="0123456789abcdef"[u->uuid[k]>>4];
                    uuid[2*k+1]="0123456789abcdef"[u->uuid[k]&15];
                }
                uuid[32]=0;
            }
            if(cmd->cmd==LC_SEGMENT_64 && cmd->cmdsize>=sizeof(struct segment_command_64)) {
                const struct segment_command_64 *segment=(const void *)cmd;
                if(!strncmp(segment->segname,"__TEXT",16)) {
                    text_start=segment->vmaddr+(uint64_t)_dyld_get_image_vmaddr_slide(i);
                    text_size=segment->vmsize;
                }
            }
            at+=cmd->cmdsize;
        }
        const char *base=strrchr(name,'/');base=base?base+1:name;
        printf("Host image: load=0x%" PRIxPTR " text=0x%" PRIx64 " size=0x%" PRIx64 " uuid=%s name=%s\n",
               (uintptr_t)header,text_start,text_size,uuid,base);
    }
    printf("Host images: end recorded=%u available=%u\n",limit,count);
}
#endif
#endif
