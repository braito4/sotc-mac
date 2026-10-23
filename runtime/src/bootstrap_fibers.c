/* ARM64/bbcpu adaptation of AjncSystem/ShadowOfTheColossus-PC
 * 2250f328feafab953aac85e758e7bddb2e80e891/src/runtime_fiber.c,
 * itself based on shadPS4's libSceFiber HLE (GPL-2.0-or-later).
 * Foundations: deadinside28/bbport and shadPS4.
 * Save guest architectural state at the host-call boundary, then leave the
 * translator through a private dispatcher. No x86 or host-stack assembly is
 * executed. Suspended fibers retain their guest stack and can resume later.
 */
#include "bootstrap_contracts.h"
#include "bootstrap_cpu_context.h"
#include "runtime.h"
#include "fiber_failure_diagnostics.h"
#include <setjmp.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Standalone contract binaries have no history owner. The diagnostic entry
 * supplies a strong override which freezes/reports its existing atomic ring. */
__attribute__((weak)) void sotc_cpu_field_trace_on_fiber_failure(const SotcFiberFailure *evidence) {
    if (sotc_fiber_failure_prepare(STDERR_FILENO))
        sotc_fiber_failure_write(STDERR_FILENO,evidence);
}

#define E_NULL ((int32_t)UINT32_C(0x80590001))
#define E_ALIGN ((int32_t)UINT32_C(0x80590002))
#define E_RANGE ((int32_t)UINT32_C(0x80590003))
#define E_INVALID ((int32_t)UINT32_C(0x80590004))
#define E_PERMISSION ((int32_t)UINT32_C(0x80590005))
#define E_STATE ((int32_t)UINT32_C(0x80590006))
#define MAGIC0 UINT32_C(0xdef1649c)
#define MAGIC1 UINT32_C(0xb37592a0)
#define OPT_MAGIC UINT32_C(0xbb40e64d)
#define STACK_MAGIC UINT64_C(0x7149f2ca7149f2ca)
#define STACK_UNUSED UINT64_C(0xdeadbeefdeadbeef)
enum { RUNNING = 1, IDLE = 2, TERMINATED = 3, CHECK_STACK = 0x10, RESET_FPU = 0x100 };
typedef struct SavedFiber {
    BbCpu cpu;
    uint64_t stop, *resume_arg;
    int started;
} SavedFiber;
typedef struct Fiber {
    uint32_t magic_start;
    _Atomic uint32_t state;
    uintptr_t entry;
    uint64_t initialize_arg;
    void *stack;
    uint64_t stack_size;
    char name[32];
    SavedFiber *context;
    uint32_t flags;
    void *context_start, *context_end;
    uint32_t magic_end;
} Fiber;
typedef struct {
    uint64_t size;
    uintptr_t entry;
    uint64_t initialize_arg;
    void *stack;
    uint64_t stack_size;
    char name[32];
    uint64_t margin;
    uint8_t pad[48];
} FiberInfo;
_Static_assert(sizeof(Fiber) == 112 && offsetof(Fiber, context) == 72, "reference SceFiber layout");
_Static_assert(sizeof(FiberInfo) == 128, "SceFiberInfo layout");
typedef struct Frame {
    jmp_buf dispatch;
    BbCpu caller;
    Fiber *running, *target;
    uint64_t run_arg, returned_arg;
} Frame;
static _Thread_local Frame *active;
static _Atomic unsigned created, runs, switches;
int bootstrap_fibers_active(void) { return active != NULL; }

static int valid(const Fiber *f) { return f->magic_start == MAGIC0 && f->magic_end == MAGIC1; }
static void prepare_stack(Fiber *f) {
    f->context_start = f->stack;
    f->context_end = (unsigned char *)f->stack + f->stack_size;
    *(uint64_t *)f->stack = STACK_MAGIC;
    if (f->flags & CHECK_STACK)
        for (uint64_t *p = (uint64_t *)f->stack + 1; p < (uint64_t *)f->context_end; ++p) *p = STACK_UNUSED;
    /* No GPU page tracker is installed in this CPU-only diagnostic. */
}
static int32_t initialize(Fiber *f, const char *name, uintptr_t entry, uint64_t arg,
                          void *stack, uint64_t size, const uint32_t *opt, uint32_t flags, uint32_t version) {
    if (!f || !name || !entry) return E_NULL;
    if (((uintptr_t)f & 7) || ((uintptr_t)stack & 15) || (opt && ((uintptr_t)opt & 7))) return E_ALIGN;
    if (size && size < 512) return E_RANGE;
    if ((size & 15) || (!stack) != (!size) || (opt && *opt != OPT_MAGIC) || !bbcpu_is_guest(entry)) return E_INVALID;
    SavedFiber *saved = calloc(1, sizeof(*saved));
    if (!saved) { fputs("STOP: cannot allocate fiber CPU context\n", stderr); _Exit(23); }
    memset(f, 0, sizeof(*f));
    f->entry = entry; f->initialize_arg = arg; f->stack = stack; f->stack_size = size;
    strncpy(f->name, name, 31); f->context = saved;
    f->flags = flags | (version >= 0x03500000 ? RESET_FPU : 0);
    f->magic_start = MAGIC0; f->magic_end = MAGIC1;
    if (stack) prepare_stack(f);
    atomic_store(&f->state, IDLE); atomic_fetch_add(&created, 1);
    return 0;
}
static int32_t fiber_initialize(Fiber *f, const char *name, uintptr_t entry, uint64_t arg,
                                void *stack, uint64_t size, const uint32_t *opt, uint32_t version) {
    return initialize(f, name, entry, arg, stack, size, opt, 0, version);
}
static int32_t fiber_initialize_internal(Fiber *f, const char *name, uintptr_t entry, uint64_t arg,
                                         void *stack, uint64_t size, const uint32_t *opt, uint32_t flags, uint32_t version) {
    return initialize(f, name, entry, arg, stack, size, opt, flags, version);
}
static int32_t options_initialize(uint32_t *opt) {
    if (!opt) return E_NULL;
    if ((uintptr_t)opt & 7) return E_ALIGN;
    *opt = OPT_MAGIC; return 0;
}
static int32_t finalize(Fiber *f) {
    if (!f) return E_NULL;
    if ((uintptr_t)f & 7) return E_ALIGN;
    if (!valid(f)) return E_INVALID;
    uint32_t expected = IDLE;
    if (!atomic_compare_exchange_strong(&f->state, &expected, TERMINATED)) return E_STATE;
    free(f->context); f->context = NULL;
    return 0;
}
static int32_t attach(Fiber *f, void *stack, uint64_t size) {
    if (size && size < 512) return E_RANGE;
    if (!stack || !size || (size & 15) || f->stack) return E_INVALID;
    f->stack = stack; f->stack_size = size; prepare_stack(f); return 0;
}
static int32_t save_running(uint64_t *resume_arg) {
    Fiber *f = active->running;
    BbCpu current;
    bootstrap_cpu_save(&current);
    /* A nested host->guest callback has live host frames which this guest-only
     * dispatcher cannot suspend. Refuse it instead of dropping those frames. */
    if (current.depth != active->caller.depth + 1) return E_PERMISSION;
    if (f->stack) {
        const uint64_t observed=*(uint64_t *)f->stack;
        if (observed != STACK_MAGIC) {
            SotcFiberFailure evidence={.fiber=(uintptr_t)f,.stack=(uintptr_t)f->stack,
                .size=f->stack_size,.context=(uintptr_t)f->context,
                .context_start=(uintptr_t)f->context_start,.context_end=(uintptr_t)f->context_end,
                .entry=f->entry,.rip=current.rip,.expected=STACK_MAGIC,.observed=observed,
                .state=atomic_load(&f->state),.flags=f->flags};
            memcpy(evidence.regs,current.r,sizeof(evidence.regs));
            memcpy(evidence.name,f->name,sizeof(evidence.name));
            sotc_cpu_field_trace_on_fiber_failure(&evidence);
            _Exit(23);
        }
        if (!bootstrap_cpu_suspend(&f->context->cpu)) return E_PERMISSION;
        f->context->resume_arg = resume_arg;
    } else {
        f->context->started = 0; /* Contextless fibers restart on the next run. */
    }
    return 0;
}
static _Noreturn void dispatch(Frame *frame) {
    Fiber *f = frame->target;
    frame->running = f;
    SavedFiber *saved = f->context;
    if (saved->started) {
        if (saved->resume_arg) *saved->resume_arg = frame->run_arg;
        saved->cpu.depth = frame->caller.depth + 1;
        bootstrap_cpu_resume(&saved->cpu, saved->stop);
    } else {
        saved->started = 1;
        void *stack = f->stack;
        uint64_t size = f->stack_size;
        if (!stack) {
            if (frame->caller.r[RSP] < 256) { fputs("STOP: contextless fiber has no guest caller stack\n", stderr); _Exit(23); }
            stack = (void *)((frame->caller.r[RSP] - 128) & ~UINT64_C(15));
        }
        bootstrap_cpu_start(f->entry, f->initialize_arg, frame->run_arg, stack, size,
                            frame->caller.depth, (f->flags & RESET_FPU) != 0, &saved->stop);
    }
    fputs("STOP: fiber entry returned instead of yielding\n", stderr); _Exit(23);
}
static int32_t run_impl(Fiber *f, void *stack, uint64_t size, uint64_t arg, uint64_t *returned) {
    if (!f) return E_NULL;
    if (((uintptr_t)f & 7) || ((uintptr_t)stack & 15)) return E_ALIGN;
    if (!valid(f)) return E_INVALID;
    BbTrapState trap;
    if (active || bbcpu_recomp_functions() || bbcpu_trap_state(&trap)) return E_PERMISSION;
    uint32_t expected = IDLE;
    if (!atomic_compare_exchange_strong(&f->state, &expected, RUNNING)) return E_STATE;
    if (stack || size) {
        const int32_t r = attach(f, stack, size);
        if (r) { atomic_store(&f->state, IDLE); return r; }
    }
    Frame *frame = calloc(1, sizeof(*frame));
    if (!frame) { atomic_store(&f->state, IDLE); return E_STATE; }
    bootstrap_cpu_save(&frame->caller);
    frame->target = f; frame->run_arg = arg;
    active = frame; atomic_fetch_add(&runs, 1);
    switch (setjmp(frame->dispatch)) {
    case 2: break;
    default: dispatch(frame);
    }
    if (returned) *returned = frame->returned_arg;
    bootstrap_cpu_restore(&frame->caller);
    active = NULL; free(frame);
    return 0;
}
static int32_t switch_impl(Fiber *f, void *stack, uint64_t size, uint64_t arg, uint64_t *on_resume) {
    if (!f) return E_NULL;
    if (((uintptr_t)f & 7) || ((uintptr_t)stack & 15)) return E_ALIGN;
    if (!valid(f)) return E_INVALID;
    if (!active) return E_PERMISSION;
    uint32_t expected = IDLE;
    if (!atomic_compare_exchange_strong(&f->state, &expected, RUNNING)) return E_STATE;
    if (stack || size) {
        const int32_t r = attach(f, stack, size);
        if (r) { atomic_store(&f->state, IDLE); return r; }
    }
    const int32_t result = save_running(on_resume);
    if (result) { atomic_store(&f->state, IDLE); return result; }
    Fiber *previous = active->running;
    active->target = f; active->run_arg = arg;
    atomic_fetch_add(&switches, 1);
    atomic_store(&previous->state, IDLE);
    longjmp(active->dispatch, 1);
}
static int32_t run(Fiber *f, uint64_t arg, uint64_t *returned) { return run_impl(f, NULL, 0, arg, returned); }
static int32_t switch_to(Fiber *f, uint64_t arg, uint64_t *on_resume) { return switch_impl(f, NULL, 0, arg, on_resume); }
static int32_t return_to_thread(uint64_t arg, uint64_t *on_resume) {
    if (!active) return E_PERMISSION;
    const int32_t result = save_running(on_resume);
    if (result) return result;
    active->returned_arg = arg;
    atomic_store(&active->running->state, IDLE);
    longjmp(active->dispatch, 2);
}
static int32_t get_self(Fiber **out) {
    if (!out) return E_NULL;
    if (!active) return E_PERMISSION;
    *out = active->running; return 0;
}
static int32_t get_info(Fiber *f, FiberInfo *out) {
    if (!f || !out) return E_NULL;
    if (((uintptr_t)f & 7) || ((uintptr_t)out & 7)) return E_ALIGN;
    if (!valid(f) || out->size != sizeof(*out)) return E_INVALID;
    out->entry = f->entry; out->initialize_arg = f->initialize_arg;
    out->stack = f->stack; out->stack_size = f->stack_size;
    memcpy(out->name, f->name, sizeof(out->name)); out->margin = UINT64_MAX;
    if ((f->flags & CHECK_STACK) && f->stack && *(uint64_t *)f->stack == STACK_MAGIC) {
        uint64_t *p = (uint64_t *)f->stack + 1;
        while (p < (uint64_t *)f->context_end && *p == STACK_UNUSED) ++p;
        out->margin = (uint64_t)((unsigned char *)p - ((unsigned char *)f->stack + 8));
    }
    return 0;
}
static int32_t rename_fiber(Fiber *f, const char *name) {
    if (!f || !name) return E_NULL;
    if ((uintptr_t)f & 7) return E_ALIGN;
    if (!valid(f)) return E_INVALID;
    memset(f->name, 0, sizeof(f->name)); strncpy(f->name, name, 31); return 0;
}
static int32_t frame_pointer(uint64_t *out) {
    if (!out) return E_NULL;
    if (!active) return E_PERMISSION;
    *out = active->caller.r[RBP]; return 0;
}
uintptr_t bootstrap_fibers_resolve(const char *name) {
    static const struct { const char *nid; void *function; } exports[] = {
        {"hVYD7Ou2pCQ#s#C", fiber_initialize}, {"7+OJIpko9RY#s#C", fiber_initialize_internal},
        {"asjUJJ+aa8s#s#C", options_initialize}, {"JeNX5F-NzQU#s#C", finalize},
        {"a0LLrZWac0M#s#C", run}, {"PFT2S-tJ7Uk#s#C", switch_to},
        {"B0ZX2hx9DMw#s#C", return_to_thread}, {"p+zLIOg27zU#s#C", get_self},
        {"avfGJ94g36Q#s#C", run_impl}, {"ZqhZFuzKT6U#s#C", switch_impl},
        {"uq2Y5BFz0PE#s#C", get_info}, {"JzyT91ucGDc#s#C", rename_fiber},
        {"0dy4JtMUcMQ#s#C", frame_pointer},
    };
    for (size_t i = 0; i < sizeof(exports) / sizeof(*exports); ++i)
        if (!strcmp(name, exports[i].nid)) return (uintptr_t)exports[i].function;
    return 0;
}
void bootstrap_fibers_report(void) {
    printf("Bootstrap: fibers created=%u, runs=%u, switches=%u\n", atomic_load(&created), atomic_load(&runs), atomic_load(&switches));
}
