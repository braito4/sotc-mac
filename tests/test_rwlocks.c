/* Original synthetic guest calls; no game input. The actual native registry
 * and pthread backend supply ownership, shared reads and exclusive writes. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

enum { INIT, DESTROY, READ, WRITE, TRY_READ, TRY_WRITE, UNLOCK, OPERATIONS };
static uintptr_t bridges[OPERATIONS];
static int32_t op(int which, void **lock) {
    const uint64_t args[] = {(uintptr_t)lock, 0, 0};
    return (int32_t)bbcpu_call(bridges[which], which == INIT ? 3 : 1, args);
}
typedef struct {
    void *lock;
    pthread_mutex_t control;
    pthread_cond_t changed;
    int reader_ready, release_reader, writer_ready, writer_held, release_writer, value;
} Fixture;
static void wait_for(Fixture *f, int *flag) {
    while (!*flag) assert(pthread_cond_wait(&f->changed, &f->control) == 0);
}
static void signal_flag(Fixture *f, int *flag) {
    *flag = 1;
    assert(pthread_cond_broadcast(&f->changed) == 0);
}
static void *reader(void *opaque) {
    Fixture *f = opaque;
    assert(op(READ, &f->lock) == 0);
    assert(pthread_mutex_lock(&f->control) == 0);
    signal_flag(f, &f->reader_ready);
    wait_for(f, &f->release_reader);
    assert(!f->value);
    assert(pthread_mutex_unlock(&f->control) == 0);
    assert(op(UNLOCK, &f->lock) == 0);
    return NULL;
}
static void *writer(void *opaque) {
    Fixture *f = opaque;
    assert((uint32_t)op(UNLOCK, &f->lock) == 0x80020001); /* Not a holder. */
    assert((uint32_t)op(TRY_WRITE, &f->lock) == 0x80020010);
    assert(pthread_mutex_lock(&f->control) == 0);
    signal_flag(f, &f->writer_ready);
    assert(pthread_mutex_unlock(&f->control) == 0);
    assert(op(WRITE, &f->lock) == 0);
    assert(pthread_mutex_lock(&f->control) == 0);
    f->value = 1;
    signal_flag(f, &f->writer_held);
    wait_for(f, &f->release_writer);
    assert(pthread_mutex_unlock(&f->control) == 0);
    assert(op(UNLOCK, &f->lock) == 0);
    return NULL;
}
void bootstrap_test_rwlocks(void) {
    const char *names[] = {"6ULAa0fq4jA#0#F", "BB+kb08Tl9A#0#F", "Ox9i0c7L5w0#0#F",
        "mqdNorrB+gI#0#F", "XD3mDeybCnk#0#F", "bIHoZCTomsI#0#F", "+L98PIbGttk#0#F"};
    unsigned char *code = runtime_low_map(16384, PROT_READ | PROT_WRITE);
    assert(code);
    for (int i = 0; i < OPERATIONS; ++i) {
        const uintptr_t target = bootstrap_resolve(names[i], 0);
        assert(target && !bootstrap_resolve(names[i], 1));
        unsigned char *p = code + i * 16;
        p[0] = 0x48; p[1] = 0xb8; memcpy(p + 2, &target, 8); p[10] = 0xff; p[11] = 0xe0;
        bridges[i] = (uintptr_t)p;
    }
    bbcpu_add_guest_code((uintptr_t)code, OPERATIONS * 16);
    assert(!bootstrap_resolve("6ULAa0fq4jA#p#F", 0));
    assert(!bootstrap_resolve("6ULAa0fq4jA#1#u", 0));
    assert(!bootstrap_resolve("iPtZRWICjrM#0#F", 0));
    assert(!bootstrap_resolve("adh--6nIqTk#0#F", 0));
    assert((uint32_t)op(INIT, NULL) == 0x80020016);
    void *lock = (void *)(uintptr_t)0xdead;
    assert(op(INIT, &lock) == 0);
    assert(op(READ, &lock) == 0 && op(READ, &lock) == 0);
    assert((uint32_t)op(TRY_WRITE, &lock) == 0x80020010);
    assert((uint32_t)op(WRITE, &lock) == 0x8002000b);
    assert((uint32_t)op(DESTROY, &lock) == 0x80020010);
    assert(op(UNLOCK, &lock) == 0 && op(UNLOCK, &lock) == 0);
    assert((uint32_t)op(UNLOCK, &lock) == 0x80020001);
    assert(op(WRITE, &lock) == 0);
    assert((uint32_t)op(TRY_READ, &lock) == 0x80020010);
    assert((uint32_t)op(READ, &lock) == 0x8002000b);
    assert(op(UNLOCK, &lock) == 0 && op(DESTROY, &lock) == 0 && (uintptr_t)lock == 1);
    assert((uint32_t)op(READ, &lock) == 0x80020016);
    assert((uint32_t)op(DESTROY, &lock) == 0x80020016);
    lock = NULL;
    assert(op(DESTROY, &lock) == 0);
    assert(op(READ, &lock) == 0 && op(UNLOCK, &lock) == 0 && op(DESTROY, &lock) == 0);

    Fixture f = {.control = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    assert(op(INIT, &f.lock) == 0 && op(READ, &f.lock) == 0);
    pthread_t r, w;
    alarm(10); /* A deadlock must fail instead of hanging the diagnostic. */
    assert(pthread_create(&r, NULL, reader, &f) == 0);
    assert(pthread_mutex_lock(&f.control) == 0);
    wait_for(&f, &f.reader_ready);
    assert(pthread_mutex_unlock(&f.control) == 0);
    assert(pthread_create(&w, NULL, writer, &f) == 0);
    assert(pthread_mutex_lock(&f.control) == 0);
    wait_for(&f, &f.writer_ready);
    assert(!f.value && !f.writer_held);
    assert(pthread_mutex_unlock(&f.control) == 0);
    assert(op(UNLOCK, &f.lock) == 0); /* The second reader still owns the lock. */
    assert((uint32_t)op(TRY_WRITE, &f.lock) == 0x80020010);
    assert((uint32_t)op(DESTROY, &f.lock) == 0x80020010);
    assert(pthread_mutex_lock(&f.control) == 0);
    assert(!f.value && !f.writer_held);
    signal_flag(&f, &f.release_reader);
    wait_for(&f, &f.writer_held);
    assert(f.value == 1);
    assert(pthread_mutex_unlock(&f.control) == 0);
    assert((uint32_t)op(TRY_READ, &f.lock) == 0x80020010);
    assert((uint32_t)op(TRY_WRITE, &f.lock) == 0x80020010);
    assert((uint32_t)op(UNLOCK, &f.lock) == 0x80020001);
    assert(pthread_mutex_lock(&f.control) == 0);
    signal_flag(&f, &f.release_writer);
    assert(pthread_mutex_unlock(&f.control) == 0);
    assert(pthread_join(r, NULL) == 0 && pthread_join(w, NULL) == 0);
    alarm(0);
    assert(op(READ, &f.lock) == 0 && f.value == 1);
    assert(op(UNLOCK, &f.lock) == 0 && op(DESTROY, &f.lock) == 0);
    assert(pthread_cond_destroy(&f.changed) == 0 && pthread_mutex_destroy(&f.control) == 0);
    puts("PASS: x86-to-ARM64 rwlock lifecycle/errors, shared readers, exclusive writer, contention, ownership and busy-destroy rejection");
}
