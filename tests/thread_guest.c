/* Compiled to x86 ELF only for bbcpu; never executed through Rosetta. */
#include "thread_fixture.h"
typedef int (*Lock)(void **);
typedef int (*Wait)(void **, void **);
void *thread_worker(ThreadFixture *f) {
    __asm__ volatile("movq %%fs:0, %0" : "=r"(f->tcb));
    __asm__ volatile("movq %%fs:-16, %0" : "=r"(f->initial_static));
    __asm__ volatile("movq %%rsp, %0" : "=r"(f->stack_pointer));
    __asm__ volatile("stmxcsr %0" : "=m"(f->inherited_mxcsr));
    f->self = (TWord)((void *(*)(void))f->self_fn)();
    f->self_join_error = (unsigned int)((int (*)(void *, void **))f->join_fn)((void *)f->self, (void **)0);
    TWord index[] = {2, 0};
    TWord *tls = ((void *(*)(TWord *))f->tls_fn)(index);
    f->tls = (TWord)tls; f->initial_module = *tls;
    f->dtv = ((TWord *)f->tcb)[1];
    f->dtv_valid = ((TWord *)f->dtv)[3] == (TWord)tls;
    f->errors |= tls[1] != 0 || tls[7] != 0;
    *tls = f->tag;
    index[0] = 3;
    TWord *other_tls = ((void *(*)(TWord *))f->tls_fn)(index);
    f->other_tls = (TWord)other_tls;
    f->errors |= *(unsigned int *)(void *)other_tls != 0x13572468U || other_tls[1] != 0;
    *other_tls = f->tag + 200;
    int *error = ((int *(*)(void))f->errno_fn)();
    f->initial_errno = (unsigned int)*error;
    *error = (int)f->tag;
    TWord own_static = f->tag + 100;
    __asm__ volatile("movq %0, %%fs:-16" :: "r"(own_static) : "memory");
    f->errors |= ((Lock)f->lock_fn)(&f->shared->mutex) != 0;
    ++f->shared->ready;
    f->errors |= ((Lock)f->signal_fn)(&f->shared->condition) != 0;
    while (!f->shared->release)
        f->errors |= ((Wait)f->wait_fn)(&f->shared->condition, &f->shared->mutex) != 0;
    f->errors |= *tls != f->tag || *other_tls != f->tag + 200 || *error != (int)f->tag;
    __asm__ volatile("movq %%fs:-16, %0" : "=r"(f->final_static));
    f->errors |= ((Lock)f->unlock_fn)(&f->shared->mutex) != 0;
    if (f->path) {
        for (unsigned int trial = 0; trial < 32; ++trial) {
            int fd = ((int (*)(const char *, int, int))f->open_fn)(f->path, 0, 0);
            unsigned char buffer[8];
            unsigned char stat[120];
            f->errors |= fd < 3;
            f->errors |= ((long long (*)(int, void *, TWord))f->read_fn)(fd, buffer, 4) != 4;
            f->errors |= buffer[0] != 'a' || buffer[1] != 0 || buffer[2] != 'b' || buffer[3] != 'c';
            f->errors |= ((long long (*)(int, void *, TWord, long long))f->pread_fn)(fd, buffer, 2, 2) != 2;
            f->errors |= buffer[0] != 'b' || buffer[1] != 'c';
            f->errors |= ((int (*)(const char *, void *))f->stat_fn)(f->path, stat) != 0;
            f->errors |= *(TWord *)(void *)(stat + 72) != 4;
            f->errors |= ((int (*)(int))f->close_fn)(fd) != 0;
            ++f->reads;
        }
    }
    if (f->explicit_exit) {
        ((void (*)(void *))f->exit_fn)((void *)(f->tag + 0x1000));
        __asm__ volatile("ud2");
    }
    return (void *)(f->tag + 0x2000);
}
