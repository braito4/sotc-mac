/* Narrow startup contracts adapted from AjncSystem/ShadowOfTheColossus-PC
 * 2250f328feafab953aac85e758e7bddb2e80e891/src/runtime.c and the arm64 callback
 * bridge in bloodborne-native/bloodborne-runtime 3170535f59cadbe5ce9c38e6bb961ed5fd215194.
 * Foundations: deadinside28/bbport and shadPS4. This is a diagnostic subset,
 * not a complete libc or PS4 runtime. No game code is embedded.
 */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "platform.h"
#include "runtime.h"
#include <inttypes.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>

typedef struct { uintptr_t callback; void *argument, *dso; int with_argument, active; } Handler;
static Handler *handlers;
static size_t handler_count, handler_capacity;
static _Atomic size_t init_calls, plain_calls, cxa_calls;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t flags, stack_canary;
static _Atomic size_t allocation_calls;
static int linked_libc;
static void *process_param;
static void **heap_api;
/* Layout/contract from SotC's runtime_sotc.c: the linked libc owns and updates
 * its trace state through these stable tables, initially empty. */
typedef struct { uint64_t size; uint32_t flag, get_segment_info; uint64_t *mask, *table; } HeapTrace;
static uint64_t heap_trace_mask, heap_trace_table[64];
static void heap_get_trace_info(HeapTrace *info) {
    if (!info) abort();
    info->mask = &heap_trace_mask; info->table = heap_trace_table; info->get_segment_info = 0;
}
static _Atomic uint64_t wait_ns[4];
static _Atomic uint64_t console_calls, console_bytes;
/* Console subset of bloodborne-runtime's runtime_file.c POSIX write contract.
 * Only stdout/stderr exist in this diagnostic; arbitrary host descriptors are
 * never guest descriptors. Preserve the byte count, short writes and guest
 * errno rather than reporting a successful write without emitting anything. */
static int64_t console_write(int fd, const void *buffer, uint64_t size) {
    int error = 0;
    if (fd != STDOUT_FILENO && fd != STDERR_FILENO) error = 9; /* EBADF */
    else if (size > (uint64_t)SSIZE_MAX) error = 22; /* EINVAL */
    else if (!buffer && size) error = 14; /* EFAULT */
    if (error) { *runtime_errno() = error; return -1; }
    const ssize_t result = write(fd, buffer, (size_t)size);
    if (result < 0) {
        /* FreeBSD/PS4 errno, following runtime_kernel.c's host mapping. */
        switch (errno) {
        case EINTR: error = 4; break;
        case ENXIO: error = 6; break;
        case EBADF: error = 9; break;
        case EFAULT: error = 14; break;
        case EINVAL: error = 22; break;
        case EFBIG: error = 27; break;
        case ENOSPC: error = 28; break;
        case EPIPE: error = 32; break;
        case EAGAIN: error = 35; break;
        default: error = 5; break; /* EIO */
        }
        *runtime_errno() = error;
        return -1;
    }
    atomic_fetch_add_explicit(&console_calls, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&console_bytes, (uint64_t)result, memory_order_relaxed);
    return result;
}
static const struct { const char *nid, *symbol; } native_import_names[] = {
#include "import_names.inc"
};
const char *runtime_symbol(const char *nid) {
    for (size_t i = 0; i < sizeof(native_import_names) / sizeof(*native_import_names); ++i)
        if (!strcmp(nid, native_import_names[i].nid)) return native_import_names[i].symbol;
    return NULL;
}
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) {
    const char *symbol = runtime_symbol(nid);
    if (!symbol) return 0;
    for (size_t i = 0; i < count; ++i)
        if (!strcmp(symbol, table[i].name)) return (uintptr_t)table[i].function;
    return 0;
}
void runtime_guest_call_sites(uint64_t out[3]) {
    out[0] = bbcpu_import_return(); out[1] = out[2] = 0;
}
uintptr_t runtime_mutex_resolve(const char *name);
void runtime_mutex_report(void);
void runtime_wait_note(int kind, uint64_t ns) {
    if (kind >= 0 && kind < 4) atomic_fetch_add_explicit(&wait_ns[kind], ns, memory_order_relaxed);
}
void bootstrap_set_linked_libc(int enabled) { linked_libc = enabled; }
void bootstrap_set_procparam(void *param) { process_param = param; }
static void *get_procparam(void) { return process_param; }
/* Diagnostic console profile: base PS4, matching the 5056 MiB direct-memory
 * budget of the upstream memory backend; PS4 Pro mode is not selected. */
static int32_t is_neo_mode(void) { return 0; }
/* shadPS4 gnmdriver.cpp/address_space.h reserves a 4 GiB system range, with
 * its final 256 MiB used for tessellation. Its original PS4 address overlaps
 * macOS arm64 host mappings. Ask the native VMA backend for a free reservation
 * instead: the returned API pointer and the game's subsequent fixed map then
 * agree, and ordinary guest allocations skip the whole reserved range.
 * This relocates this API-owned range only; it provides no GPU and does not
 * translate the game's packed/static pointers or other fixed PS4 addresses. */
static uint64_t tessellation_ring_address(void) {
    static void *system_range;
    const uint64_t system_size = UINT64_C(1) << 32;
    pthread_mutex_lock(&lock);
    if (!system_range) {
        typedef int32_t (*Reserve)(void **, uint64_t, int, uint64_t);
        Reserve reserve = (Reserve)runtime_memory_resolve("7oxv3PPCumo#p#J");
        void *candidate = NULL;
        const int32_t result = reserve ? reserve(&candidate, system_size, 0, system_size)
                                       : (int32_t)UINT32_C(0x80020016);
        if (result) {
            pthread_mutex_unlock(&lock);
            printf("STOP_MEMORY_LAYOUT: system reservation size=%" PRIu64 " result=%#x\n",
                   system_size, (unsigned)result);
            bootstrap_report(); fflush(NULL); _Exit(25);
        }
        system_range = candidate;
        printf("Bootstrap: tessellation range relocated into reserved ARM64 guest memory\n");
    }
    const uint64_t address = (uintptr_t)system_range + system_size - (UINT64_C(256) << 20);
    pthread_mutex_unlock(&lock);
    return address;
}
static int32_t allocate_direct(int64_t low, int64_t high, uint64_t size, uint64_t alignment, int type, int64_t *out) {
    typedef int32_t (*Fn)(int64_t, int64_t, uint64_t, uint64_t, int, int64_t *);
    Fn fn = (Fn)runtime_memory_resolve("rTXw65xmLIA#p#J");
    errno = 0;
    const int32_t result = fn(low, high, size, alignment, type, out);
    const int host_errno = errno;
    if (result) {
        fprintf(stderr, "Bootstrap: direct allocation size=%" PRIu64 " failed %#x; host errno=%d (%s)\n",
                size, (unsigned)result, host_errno, strerror(host_errno));
        printf("STOP_HOST_RESOURCE: direct-memory allocation result=%#x errno=%d\n", (unsigned)result, host_errno);
        bootstrap_report(); fflush(NULL); _Exit(24);
    }
    return result;
}
static int32_t map_direct(void **out, uint64_t size, int prot, int flags_, int64_t physical, uint64_t alignment) {
    typedef int32_t (*Fn)(void **, uint64_t, int, int, int64_t, uint64_t);
    Fn fn = (Fn)runtime_memory_resolve("L-Q3LEjIbgA#p#J");
    const uintptr_t requested = out ? (uintptr_t)*out : 0;
    const int32_t result = fn(out, size, prot, flags_, physical, alignment);
    if (result) {
        fprintf(stderr, "Bootstrap: map request address=%#" PRIxPTR " size=%" PRIu64 " physical=%#" PRIx64 " result=%#x\n",
                requested, size, (uint64_t)physical, (unsigned)result);
        printf("STOP_MEMORY_LAYOUT: direct-map address=%#" PRIxPTR " size=%" PRIu64 " result=%#x\n",
               requested, size, (unsigned)result);
        bootstrap_report(); fflush(NULL); _Exit(25);
    }
    return result;
}
static void set_heap_api(void **api) { heap_api = api; }

void bootstrap_initialize(uint64_t capabilities) {
    flags = capabilities;
    arc4random_buf(&stack_canary, sizeof(stack_canary));
    stack_canary &= ~UINT64_C(255);
}
static void init_env(void) {
    /* The prepare.py capability proves that the supplied libc export is one RET. */
    ++init_calls;
}
static int register_handler(Handler value) {
    if (!value.callback) return -1;
    pthread_mutex_lock(&lock);
    if (handler_count == handler_capacity) {
        size_t capacity = handler_capacity ? handler_capacity * 2 : 64;
        Handler *next = capacity > 1024 * 1024 ? NULL : realloc(handlers, capacity * sizeof(*handlers));
        if (!next) { pthread_mutex_unlock(&lock); return -1; }
        handlers = next; handler_capacity = capacity;
    }
    value.active = 1;
    handlers[handler_count++] = value;
    pthread_mutex_unlock(&lock);
    return 0;
}
static int guest_atexit(void (*callback)(void)) {
    int result = register_handler((Handler){.callback = (uintptr_t)callback});
    if (!result) ++plain_calls;
    return result;
}
static int guest_cxa_atexit(void (*callback)(void *), void *argument, void *dso) {
    int result = register_handler((Handler){.callback = (uintptr_t)callback, .argument = argument,
                                           .dso = dso, .with_argument = 1});
    if (!result) ++cxa_calls;
    return result;
}
void bootstrap_finalize(void *dso) {
    for (;;) {
        pthread_mutex_lock(&lock);
        size_t i = handler_count;
        while (i && (!handlers[i - 1].active || (dso && handlers[i - 1].dso != dso))) --i;
        if (!i) { pthread_mutex_unlock(&lock); return; }
        Handler h = handlers[i - 1];
        handlers[i - 1].active = 0;
        pthread_mutex_unlock(&lock);
        if (bbcpu_is_guest(h.callback)) {
            const uint64_t argument = (uintptr_t)h.argument;
            bbcpu_call(h.callback, h.with_argument, &argument);
        } else if (h.with_argument) {
            ((void (*)(void *))h.callback)(h.argument);
        } else {
            ((void (*)(void))h.callback)();
        }
    }
}
static _Noreturn void stack_fail(void) {
    fputs("STOP: guest stack protector detected corruption\n", stderr);
    _Exit(22);
}
static void *guest_malloc(size_t size) { ++allocation_calls; return bb_low_malloc(size); }
static void *guest_realloc(void *p, size_t size) { ++allocation_calls; return bb_low_realloc(p, size); }
static void *guest_memalign(size_t alignment, size_t size) {
    ++allocation_calls;
    return bb_low_aligned_alloc(alignment, size);
}
static int32_t posix_attr_protocol(void **attr, int protocol) {
    if (!attr || !*attr || protocol < 0 || protocol > 2) return 22;
    /* Normal host mutexes provide PRIO_NONE. Priority inheritance/protection
     * would need a scheduler bridge and remain explicitly unsupported here. */
    if (protocol) {
        static int reported;
        if (!reported++) fprintf(stderr, "Bootstrap: pthread priority protocol %d is unsupported (ENOTSUP)\n", protocol);
        return 45; /* FreeBSD/PS4 ENOTSUP. */
    }
    return 0;
}
uintptr_t bootstrap_resolve(const char *name, int is_data) {
    if (!(flags & 1)) return 0;
    /* Scoped library identifiers observed in the validated CUSA08809 EU 1.01 input.
     * Unknown scopes remain unresolved instead of matching NIDs across libraries. */
    if (is_data) {
        if (!strcmp(name, "f7uOxY9mM1U#0#F")) return (uintptr_t)&stack_canary;
        static int32_t need_internal = 1;
        if (!strcmp(name, "ZT4ODD2Ts9o#libSceLibcInternal")) return (uintptr_t)&need_internal;
        return 0;
    }
    const uintptr_t clock = bootstrap_clocks_resolve(name);
    if (clock) return clock;
    const uintptr_t fiber = bootstrap_fibers_resolve(name);
    if (fiber) return fiber;
    const uintptr_t module = bootstrap_modules_resolve(name);
    if (module) return module;
    if (!strcmp(name, "bzQExy189ZI#1#u")) return (uintptr_t)init_env;
    if (!strcmp(name, "8G2LB+A3rzg#1#u")) return (uintptr_t)guest_atexit;
    if (!strcmp(name, "tsvEmnenz48#1#u")) return (uintptr_t)guest_cxa_atexit;
    if (!strcmp(name, "H2e8t5ScQGc#1#u")) return (uintptr_t)bootstrap_finalize;
    if (!strcmp(name, "Ou3iL1abvng#0#F")) return (uintptr_t)stack_fail;
    if (!strcmp(name, "959qrazPIrg#0#F")) return (uintptr_t)get_procparam;
    if (!strcmp(name, "WslcK1FQcGI#0#F")) return (uintptr_t)is_neo_mode;
    if (!strcmp(name, "ln33zjBrfjk#A#B")) return (uintptr_t)tessellation_ring_address;
    if (!strcmp(name, "rTXw65xmLIA#0#F")) return (uintptr_t)allocate_direct;
    if (!strcmp(name, "L-Q3LEjIbgA#0#F")) return (uintptr_t)map_direct;
    if (!strcmp(name, "FxVZqBAA7ks#0#F")) return (uintptr_t)console_write;
    if (!strcmp(name, "vNe1w4diLCs#0#F")) return (uintptr_t)bootstrap_tls_get_addr;
    if (!strcmp(name, "p5EcQeEeJAE#0#F")) return (uintptr_t)set_heap_api;
    if (!strcmp(name, "NWtTN10cJzE#libSceLibcInternalExt")) return (uintptr_t)heap_get_trace_info;
    /* Keep allocator families together. With linked libc, all guest heap calls
     * bind to that libc instead of mixing its blocks with the host allocator. */
    if (!linked_libc) {
        if (!strcmp(name, "gQX+4GDQjpM#1#u")) return (uintptr_t)guest_malloc;
        if (!strcmp(name, "tIhsqj0qsFE#1#u")) return (uintptr_t)bb_low_free;
        if (!strcmp(name, "Y7aJ1uydPMo#1#u")) return (uintptr_t)guest_realloc;
        if (!strcmp(name, "Ujf3KzMvRmI#1#u")) return (uintptr_t)guest_memalign;
    }
    if (!strcmp(name, "Q3VBxCXhUHs#1#u")) return (uintptr_t)memcpy;
    if (!strcmp(name, "8zTFvBIAIN8#1#u")) return (uintptr_t)memset;
    if (!strcmp(name, "j4ViWNHEgww#1#u")) return (uintptr_t)strlen;
    if (!strcmp(name, "5txKfcMUAok#p#F")) return (uintptr_t)posix_attr_protocol;
    /* Verified executable namespaces: SotC libScePosix#libkernel is #p#F;
     * its libkernel#libkernel is #0#F. Reuse the upstream typed mutex backend
     * under Bloodborne's corresponding namespaces, with no upstream edits. */
    if (strlen(name) == 15) {
        const char *scope = !strcmp(name + 11, "#p#F") ? "#I#J"
                          : !strcmp(name + 11, "#0#F") ? "#p#J" : NULL;
        if (scope) {
            char canonical[32];
            snprintf(canonical, sizeof(canonical), "%.11s%s", name, scope);
            const uintptr_t mutex = runtime_mutex_resolve(canonical);
            if (mutex) return mutex;
            /* Reuse only the checked non-timed kernel rwlock contracts. The
             * kernel relative-timeout NIDs have a different argument ABI from
             * the upstream backend's absolute-timespec functions. Keep those
             * unresolved until a dedicated relative-time adapter is tested. */
            static const char *rwlocks[] = {"6ULAa0fq4jA", "BB+kb08Tl9A", "Ox9i0c7L5w0",
                "mqdNorrB+gI", "XD3mDeybCnk", "bIHoZCTomsI", "+L98PIbGttk"};
            for (size_t i = 0; i < sizeof(rwlocks) / sizeof(*rwlocks); ++i)
                if (!strncmp(name, rwlocks[i], 11)) return runtime_rwlock_resolve(canonical);
            const uintptr_t thread = runtime_thread_resolve(canonical);
            if (thread) return thread;
            const uintptr_t memory = runtime_memory_resolve(canonical);
            if (memory) return memory;
            return bootstrap_files_resolve(canonical);
        }
    }
    return 0;
}
void bootstrap_report(void) {
    pthread_mutex_lock(&lock);
    printf("Bootstrap: _init_env=%zu, atexit=%zu, __cxa_atexit=%zu, handlers=%zu\n",
           init_calls, plain_calls, cxa_calls, handler_count);
    pthread_mutex_unlock(&lock);
    printf("Bootstrap: allocations=%zu (upstream arm64 heap below 1 TiB)\n", allocation_calls);
    runtime_mutex_report();
    runtime_rwlock_report();
    runtime_thread_report();
    runtime_memory_report();
    bootstrap_files_report();
    bootstrap_fibers_report();
    printf("Bootstrap: console writes=%" PRIu64 ", bytes=%" PRIu64 " (stdout/stderr only)\n",
           atomic_load_explicit(&console_calls, memory_order_relaxed),
           atomic_load_explicit(&console_bytes, memory_order_relaxed));
    printf("Bootstrap: application heap API registered=%s\n", heap_api ? "yes" : "no");
}
