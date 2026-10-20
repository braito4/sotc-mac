/* Our synthetic x86-64 fixture; contains no game code. Never executed by Rosetta. */
#include "fiber_fixture.h"
typedef int (*Return)(FWord, FWord *);
typedef int (*Switch)(void *, FWord, FWord *);
typedef int (*Self)(void **);

void fixture_a(FWord initialize_arg, FWord arg) {
    Fixture *f = (void *)initialize_arg;
    volatile FWord local = 0x123456789abcdef0ULL;
    register FWord canary __asm__("r12") = 0x76543210fedcba98ULL;
    void *self = 0;
    f->first = arg;
    f->errors |= ((Self)f->self_fn)(&self) != 0 || (FWord)self != f->self;
    __asm__ volatile("stmxcsr %0" : "=m"(f->initial_mxcsr));
    unsigned int mxcsr = 0x3f80;
    __asm__ volatile("ldmxcsr %0" :: "m"(mxcsr));
    __asm__ volatile("" : "+r"(canary));
    f->errors |= ((Return)f->return_fn)(11, &f->resumed) != 0;
    __asm__ volatile("" : "+r"(canary));
    f->canary = canary; f->local = local;
    __asm__ volatile("stmxcsr %0" : "=m"(f->mxcsr));
    __asm__ volatile("movq %%fs:0, %0" : "=r"(f->thread));
    f->errors |= ((Switch)f->switch_fn)((void *)f->other, 301, &f->switched) != 0;
    f->errors |= ((Return)f->return_fn)(501, &f->second) != 0;
    __builtin_trap();
}
void fixture_b(FWord initialize_arg, FWord arg) {
    Fixture *f = (void *)initialize_arg;
    volatile FWord local = 0xabcdef0123456789ULL;
    f->first = arg;
    unsigned int mxcsr = 0x5f80;
    __asm__ volatile("ldmxcsr %0" :: "m"(mxcsr));
    f->errors |= ((Switch)f->switch_fn)((void *)f->other, 401, &f->switched) != 0;
    f->local = local;
    __asm__ volatile("stmxcsr %0" : "=m"(f->mxcsr));
    f->errors |= ((Return)f->return_fn)(701, &f->resumed) != 0;
    __builtin_trap();
}
int fixture_initialize8(InitFixture *a) {
    typedef int (*Init)(void *, const char *, FWord, FWord, void *, FWord, const void *, unsigned int);
    return ((Init)a->target)((void *)a->fiber, (void *)a->name, a->entry, a->arg,
                            (void *)a->stack, a->size, (void *)a->opt, (unsigned int)a->version);
}
int fixture_initialize9(InitFixture *a) {
    typedef int (*Init)(void *, const char *, FWord, FWord, void *, FWord, const void *, unsigned int, unsigned int);
    return ((Init)a->target)((void *)a->fiber, (void *)a->name, a->entry, a->arg,
                            (void *)a->stack, a->size, (void *)a->opt, (unsigned int)a->flags, (unsigned int)a->version);
}
void fixture_contextless(FWord initialize_arg, FWord arg) {
    Fixture *f = (void *)initialize_arg;
    ++f->first;
    ((Return)f->return_fn)(arg + 1, 0);
    __builtin_trap();
}
int fixture_run(FWord function, void *fiber, FWord arg, FWord *returned) {
    return ((Switch)function)(fiber, arg, returned);
}
int fixture_nested(FWord return_fn) { return ((Return)return_fn)(0, 0); }
void fixture_permissions(FWord initialize_arg, FWord arg) {
    Fixture *f = (void *)initialize_arg;
    (void)arg;
    f->errors |= (unsigned int)((Switch)f->run_fn)((void *)f->self, 0, 0) != 0x80590005U;
    f->errors |= (unsigned int)((Switch)f->switch_fn)((void *)f->self, 0, 0) != 0x80590006U;
    f->errors |= (unsigned int)((int (*)(void *))f->other)((void *)f->self) != 0x80590006U;
    f->errors |= (unsigned int)((int (*)(FWord))f->nested_fn)(f->return_fn) != 0x80590005U;
    ((Return)f->return_fn)(1000, 0);
    __builtin_trap();
}
