/* Checked lifecycle adapter around unchanged bloodborne-native runtime_thread.c
 * 3170535f59cadbe5ce9c38e6bb961ed5fd215194 (GPL-2.0-or-later).
 * Uses its native pthreads, low guest stacks, TCBs, attributes and counters.
 * Foundations: deadinside28/bbport, bloodborne-native and shadPS4.
 */
#include "bootstrap_contracts.h"
#include "bootstrap_cpu_context.h"
#include "runtime.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int spawn_with_context(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
static void attach_module_tls(void *);
#define pthread_create spawn_with_context
#define bb_guest_tls_set attach_module_tls
#define runtime_thread_resolve sotc_upstream_thread_resolve
#include "../upstream/src/runtime_thread.c"
#undef runtime_thread_resolve
#undef bb_guest_tls_set
#undef pthread_create

typedef struct {
    void *(*start)(void *);
    void *argument;
    uint32_t mxcsr;
    uint16_t control, status;
} Start;
static void *start_with_context(void *opaque) {
    const Start start = *(Start *)opaque;
    free(opaque);
    bootstrap_cpu_set_fpu(start.mxcsr, start.control, start.status);
    return start.start(start.argument);
}
static int spawn_with_context(pthread_t *out, const pthread_attr_t *attr, void *(*start)(void *), void *argument) {
    Start *context = malloc(sizeof(*context));
    if (!context) return ENOMEM;
    BbCpu cpu; bootstrap_cpu_save(&cpu);
    *context = (Start){start, argument, cpu.mxcsr, cpu.fcw, cpu.fsw};
    const int error = pthread_create(out, attr, start_with_context, context);
    if (error) free(context);
    return error;
}
static void attach_module_tls(void *tcb) {
    bb_guest_tls_set(tcb);
    bootstrap_tls_attach(tcb, tls_offset());
}
/* The diagnostic exposes no pthread keys. A key call remains unresolved;
 * there are no hidden key destructors to skip at this lifecycle boundary. */
void runtime_thread_keys_cleanup(void) {
    bootstrap_cpu_thread_finished();
    bootstrap_tls_cleanup();
}
/* Serialize creation and lifecycle claims, never the blocking join itself.
 * This prevents a second join/detach from racing a join already in progress. */
static pthread_mutex_t lifecycle_lock = PTHREAD_MUTEX_INITIALIZER;
size_t bootstrap_threads_live(void) {
    size_t count = 0;
    pthread_mutex_lock(&lock);
    for (GuestThread *t = threads; t; t = t->next) if (!t->host_owned && !t->finished) ++count;
    pthread_mutex_unlock(&lock);
    return count;
}
static ABI int32_t checked_create(GuestThread **out, ThreadAttr **attr, GuestEntry entry, void *argument, const char *name) {
    if (!out || !entry || !bbcpu_is_guest((uintptr_t)entry)) return ERR(22);
    bootstrap_tls_freeze();
    pthread_mutex_lock(&lifecycle_lock);
    const int32_t result = thread_create(out, attr, entry, argument, name);
    pthread_mutex_unlock(&lifecycle_lock);
    return result;
}
static ABI int32_t checked_join(GuestThread *handle, void **result) {
    pthread_mutex_lock(&lifecycle_lock);
    GuestThread *t = find_thread(handle);
    int32_t error = 0;
    if (!t || t->host_owned) error = ERR(3);
    else if (t == current) error = ERR(11);
    else if (t->detached || t->joined) error = ERR(22);
    else t->joined = 1;
    pthread_mutex_unlock(&lifecycle_lock);
    if (error) return error;
    void *ignored = NULL;
    const int native_error = pthread_join(t->host, &ignored);
    if (native_error) return ERR(runtime_guest_errno(native_error));
    if (result) *result = t->result;
    pthread_mutex_lock(&lock); ++joined_count; pthread_mutex_unlock(&lock);
    return 0;
}
static ABI int32_t checked_detach(GuestThread *handle) {
    pthread_mutex_lock(&lifecycle_lock);
    GuestThread *t = find_thread(handle);
    int32_t error = 0;
    if (!t || t->host_owned) error = ERR(3);
    else if (t->detached || t->joined) error = ERR(22);
    else {
        const int native_error = pthread_detach(t->host);
        if (native_error) error = ERR(runtime_guest_errno(native_error));
        else t->detached = 1;
    }
    pthread_mutex_unlock(&lifecycle_lock);
    return error;
}
static ABI _Noreturn void checked_exit(void *value) {
    BbTrapState trap;
    if (bootstrap_fibers_active() || bbcpu_trap_state(&trap)) {
        fputs("STOP: pthread_exit through a live fiber/trap frame is outside this diagnostic\n", stderr); _Exit(23);
    }
    thread_exit(value);
}
static ABI int32_t checked_posix_create(GuestThread **t, ThreadAttr **a, GuestEntry e, void *arg) {
    return posix(checked_create(t, a, e, arg, "posix"));
}
static ABI int32_t checked_posix_create_name(GuestThread **t, ThreadAttr **a, GuestEntry e, void *arg, const char *name) {
    return posix(checked_create(t, a, e, arg, name));
}
static ABI int32_t checked_posix_join(GuestThread *t, void **result) { return posix(checked_join(t, result)); }
uintptr_t runtime_thread_resolve(const char *name) {
    static const struct { const char *name; void *fn; } overrides[] = {
        {"6UgtwV+0zb4#p#J", checked_create}, {"onNY9Byn-W8#p#J", checked_join},
        {"4qGrR6eoP9Y#p#J", checked_detach}, {"3kg7rT0NQIs#p#J", checked_exit},
        {"OxhIB8LB-PQ#I#J", checked_posix_create}, {"Jmi+9w9u0E4#I#J", checked_posix_create_name},
        {"h9CcP3J0oVM#I#J", checked_posix_join}, {"FJrT5LuUBAU#I#J", checked_exit},
    };
    for (size_t i = 0; i < sizeof(overrides) / sizeof(*overrides); ++i)
        if (!strcmp(name, overrides[i].name)) return (uintptr_t)overrides[i].fn;
    return sotc_upstream_thread_resolve(name);
}
