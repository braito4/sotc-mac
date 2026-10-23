/* Original bounded terminating fiber evidence. No guest layout or pointer chasing. */
#ifndef SOTC_FIBER_FAILURE_DIAGNOSTICS_H
#define SOTC_FIBER_FAILURE_DIAGNOSTICS_H
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    uint64_t fiber, stack, size, context, context_start, context_end;
    uint64_t entry, rip, expected, observed, state, flags, regs[16];
    unsigned char name[32];
} SotcFiberFailure;

/* Termination only. Skip unsupported output, never wait/retry on a full pipe.
 * Descriptor suppression avoids SIGPIPE from other diagnostic writers on Darwin. */
static inline int sotc_fiber_failure_prepare(int fd) {
    struct stat info;
    const int flags=fcntl(fd,F_GETFL);
    sigset_t blocked;sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);
#ifdef F_SETNOSIGPIPE
    (void)fcntl(fd,F_SETNOSIGPIPE,1);
#endif
    return !pthread_sigmask(SIG_BLOCK,&blocked,NULL) && flags>=0 && !fstat(fd,&info) &&
        (S_ISFIFO(info.st_mode) || S_ISSOCK(info.st_mode)) &&
        ((flags&O_NONBLOCK) || !fcntl(fd,F_SETFL,flags|O_NONBLOCK));
}
static inline int sotc_fiber_failure_line(int fd,const char *line,int count,size_t size) {
    return count>0 && (size_t)count<size && write(fd,line,(size_t)count)==count;
}
static inline void sotc_fiber_failure_write(int fd,const SotcFiberFailure *f) {
    char line[512];
    _Static_assert(sizeof(line)<=PIPE_BUF,"Each fiber evidence write must fit one pipe write");
    const unsigned bounds_valid=f->size>=8 && f->stack<=UINT64_MAX-f->size;
    const uint64_t end=bounds_valid ? f->stack+f->size : 0;
    const unsigned inside=bounds_valid && f->regs[4]>=f->stack+8 && f->regs[4]<end;
    int n=snprintf(line,sizeof(line),
        "STOP: fiber guest stack overflow\n"
        "Fiber stack failure: fiber=0x%" PRIx64 " stack=0x%" PRIx64 " size=0x%" PRIx64
        " rsp=0x%" PRIx64 " stack_end=0x%" PRIx64 " bounds_valid=0x%x rsp_in_bounds=0x%x"
        " observed=0x%" PRIx64 " expected=0x%" PRIx64 " state=0x%" PRIx64 " flags=0x%" PRIx64 "\n",
        f->fiber,f->stack,f->size,f->regs[4],end,bounds_valid,inside,
        f->observed,f->expected,f->state,f->flags);
    if (!sotc_fiber_failure_line(fd,line,n,sizeof(line))) return;
    char name[65];static const char hex[]="0123456789abcdef";
    for(unsigned i=0;i<32;++i) {name[i*2]=hex[f->name[i]>>4];name[i*2+1]=hex[f->name[i]&15];}
    name[64]=0;
    n=snprintf(line,sizeof(line),
        "Fiber stack identity: fiber=0x%" PRIx64 " entry=0x%" PRIx64 " guest_rip=0x%" PRIx64
        " context=0x%" PRIx64 " context_start=0x%" PRIx64 " context_end=0x%" PRIx64
        " name_hex=%s; numeric bounds only, marker mismatch is not proof of stack exhaustion\n",
        f->fiber,f->entry,f->rip,f->context,f->context_start,f->context_end,name);
    if (!sotc_fiber_failure_line(fd,line,n,sizeof(line))) return;
    static const char *const names[]={"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
                                      "r8","r9","r10","r11","r12","r13","r14","r15"};
    for(unsigned start=0;start<16;start+=8) {
        size_t used=0;
        for(unsigned i=start;i<start+8;++i) {
            n=snprintf(line+used,sizeof(line)-used,"%s%s=0x%" PRIx64,i==start?"  ":" ",names[i],f->regs[i]);
            if (n<0 || (size_t)n>=sizeof(line)-used) return;
            used+=(size_t)n;
        }
        line[used++]='\n';
        if (write(fd,line,used)!=(ssize_t)used) return;
    }
}

#endif
