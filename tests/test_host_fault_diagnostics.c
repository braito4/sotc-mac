/* Original native signal fixture: no game, guest translation or GPU. */
#include "host_fault_diagnostics.h"
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>

static void fatal(int sig,siginfo_t *info,void *context) {
    (void)info;
    sotc_host_fault_ucontext(STDERR_FILENO,context);
    _Exit(128+sig);
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--fault")) {
        sotc_host_image_inventory();fflush(stdout);
        struct sigaction action={.sa_sigaction=fatal,.sa_flags=SA_SIGINFO};
        sigemptyset(&action.sa_mask);
        assert(!sigaction(SIGSEGV,&action,NULL));
        assert(!sigaction(SIGBUS,&action,NULL));
        void (*volatile jump)(void)=(void (*)(void))(uintptr_t)0x1000;
        jump();return 1;
    }
    SotcHostFaultRegs r={0};
    for(unsigned i=0;i<29;++i) r.x[i]=0x100+i;
    void *guard=mmap(NULL,16384,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);
    assert(guard!=MAP_FAILED);
    r.pc=UINT64_MAX;r.lr=0x123456;r.fp=(uintptr_t)guard;r.sp=(uintptr_t)guard;
    sotc_host_fault_context(STDOUT_FILENO,&r); // Does not dereference the guarded pointers.
    assert(!munmap(guard,16384));
    puts("PASS: bounded native snapshot without pointed-to reads");
}
