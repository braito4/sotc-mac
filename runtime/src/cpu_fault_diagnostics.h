/* Terminating diagnostics adapted from bloodborne-runtime b2ff9cd0 probe.c's
 * fault_reads/guest-stack diagnostics. Foundations: bloodborne-native, bbport
 * and shadPS4. No game layouts, addresses, hooks or executable bytes.
 *
 * Best-effort private evidence, not a synchronized snapshot or a call stack.
 * Only the fatal path reads memory, through the platform's protected reader.
 * No allocation, VMA lock, GPU callback or permission change is introduced.
 */
#ifndef SOTC_CPU_FAULT_DIAGNOSTICS_H
#define SOTC_CPU_FAULT_DIAGNOSTICS_H
#include "bbcpu.h"
#include "platform.h"
#include <stdint.h>
#include <unistd.h>

extern unsigned runtime_memory_trap_reasons(uintptr_t address);

static size_t sotc_fault_text(char *out, const char *text) {
    size_t n=0;
    while (*text) out[n++]=*text++;
    return n;
}
static size_t sotc_fault_hex(char *out, uint64_t value) {
    size_t n=0; int started=0;
    out[n++]='0'; out[n++]='x';
    for (int shift=60;shift>=0;shift-=4) {
        unsigned digit=(unsigned)((value>>shift)&15);
        if (digit || started || !shift) {
            out[n++]="0123456789abcdef"[digit]; started=1;
        }
    }
    return n;
}
static int sotc_fault_guest_range(uint64_t at, size_t size) {
    return size && ((at>=BB_LOW_MIN && at<BB_LOW_MAX && size<=BB_LOW_MAX-at) ||
                    (at>=BB_USER_MIN && at<BB_USER_MAX && size<=BB_USER_MAX-at));
}
/* Stop before a tracked GPU page; even a read must not resolve guest contents
 * or wait for the renderer from its terminating signal handler. */
static size_t sotc_fault_read(uint64_t *out, uint64_t at, size_t words) {
    size_t got=0;
    for (;got<words;++got) {
        if (got>(UINT64_MAX-at)/8) break;
        const uint64_t address=at+got*8;
        if (!sotc_fault_guest_range(address,8) ||
            runtime_memory_trap_reasons(address) ||
            runtime_memory_trap_reasons(address+7) ||
            bb_read_memory(out+got,(uintptr_t)address,8)!=8) break;
    }
    return got;
}
/* All callers use fixed labels and at most 16 words: <512 bytes per line. */
static void sotc_fault_words(int fd, const char *label, uint64_t at, size_t words) {
    uint64_t values[16];
    const size_t got=sotc_fault_read(values,at,words);
    char line[512];size_t n=sotc_fault_text(line,"  CPU fault memory ");
    n+=sotc_fault_text(line+n,label);n+=sotc_fault_text(line+n," address=");
    n+=sotc_fault_hex(line+n,at);n+=sotc_fault_text(line+n," words=");
    n+=sotc_fault_hex(line+n,got);
    for (size_t i=0;i<got;++i) {line[n++]=' ';n+=sotc_fault_hex(line+n,values[i]);}
    if (got<words) n+=sotc_fault_text(line+n," stopped_unreadable_or_guarded");
    line[n++]='\n';(void)write(fd,line,n);
}
static void sotc_cpu_fault_context(int fd,const BbGuestRegs *regs,
                                   uint64_t image_base,uint64_t image_size) {
    static const char *const names[16]={"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
                                        "r8","r9","r10","r11","r12","r13","r14","r15"};
    static const char header[]="CPU fault context: private, best-effort; words and frame candidates are not validated objects/callers\n";
    (void)write(fd,header,sizeof(header)-1);
    /* Four words from each distinct register base; captures metadata without
     * following arbitrary object pointers or assuming an allocator layout. */
    for (unsigned i=0;i<16;++i) {
        if (i==4 || i==5) continue;
        int duplicate=0;
        for (unsigned j=0;j<i;++j) if (j!=4 && j!=5 && regs->r[i]==regs->r[j]) duplicate=1;
        if (!duplicate) sotc_fault_words(fd,names[i],regs->r[i],4);
    }
    sotc_fault_words(fd,"rsp",regs->r[4],16);
    uint64_t frame=regs->r[5];
    for (unsigned depth=0;depth<8;++depth) {
        uint64_t pair[2];
        if ((frame&7) || sotc_fault_read(pair,frame,2)!=2) break;
        char line[192];size_t n=sotc_fault_text(line,"  CPU fault frame candidate fp=");
        n+=sotc_fault_hex(line+n,frame);n+=sotc_fault_text(line+n," return=");
        n+=sotc_fault_hex(line+n,pair[1]);
        if (pair[1]>=image_base && pair[1]-image_base<image_size) {
            n+=sotc_fault_text(line+n," image_offset=");n+=sotc_fault_hex(line+n,pair[1]-image_base);
        }
        line[n++]='\n';(void)write(fd,line,n);
        /* Bounded preceding frame storage may retain earlier allocation
         * parameters. It is raw memory, never labeled as validated arguments. */
        if(frame>=64) sotc_fault_words(fd,"frame_locals_candidate",frame-64,8);
        /* Cycles, backward pointers and implausibly distant frames stop here.
         * A missing frame pointer is expected in optimized/tail-called code. */
        if (pair[0]<=frame || pair[0]-frame>(1u<<20)) break;
        frame=pair[0];
    }
    static const char end[]="CPU fault context: end (bounded, no recovery attempted)\n";
    (void)write(fd,end,sizeof(end)-1);
}
#endif
