/* Diagnostic BBPROBE2 reader: stop at the first unimplemented PS4 import.
 * Default: all imports stop. --bootstrap: narrow verified startup contracts only.
 * Format and entry convention: AjncSystem/ShadowOfTheColossus-PC, revision
 * 2250f328feafab953aac85e758e7bddb2e80e891, scripts/prepare.py and src/probe.c.
 * CPU API: bloodborne-native/bloodborne-recomp, f749c155c2ff3bb932795de85c9c8fe9c4aa59ad.
 * Upstream foundations: deadinside28/bbport and shadPS4. No game code is embedded.
 */
#include "bbcpu.h"
#include "bootstrap_contracts.h"
#include "cpu_fault_diagnostics.h"
#include "cpu_field_trace.h"
#include "cpu_pointer_trace.h"
#include "shader_backing_trace.h"
#include "cpu_interp_region.h"
#include "startup_no_intro.h"
extern void sotc_cpu_region_dump(void);
#define SOTC_DIAGNOSTIC_CONTROL_DUMP(fd) do { sotc_cpu_field_trace_freeze(); sotc_cpu_region_dump(); sotc_cpu_field_trace_dump_limited((fd),128); } while (0)
#include "diagnostic_control.h"
#undef SOTC_DIAGNOSTIC_CONTROL_DUMP
#include "host_fault_diagnostics.h"
#include "platform.h"
#include "runtime.h"
#include <inttypes.h>
#include <stdatomic.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ucontext.h>

/* This metadata lookup is lock-free; never take the VMA registry lock from a
 * signal handler (the interrupted thread may already own it). */
extern unsigned runtime_memory_trap_reasons(uintptr_t address);
/* Private diagnostic overlay: 2 is generated JIT code, 4 is one active
 * interpreter instruction, and 0 means no exact guest context. */
extern int sotc_cpu_fault_context_kind(void *context);
extern void sotc_cpu_region_dump(void);
static size_t fault_hex(char *out, const char *label, uint64_t value) {
    size_t n=0;
    while (*label) out[n++]=*label++;
    int started=0;
    for (int shift=60; shift>=0; shift-=4) {
        const unsigned digit=(unsigned)((value>>shift)&15);
        if (digit || started || !shift) { out[n++]="0123456789abcdef"[digit]; started=1; }
    }
    return n;
}

static char (*imports)[128];
static uint64_t import_count;
static int bootstrap;
static _Atomic int entered_game;
static atomic_flag stopping = ATOMIC_FLAG_INIT;
static atomic_flag fault_reporting = ATOMIC_FLAG_INIT;
static uint64_t fault_image_size;
void sotc_cpu_on_instruction_failure(const SotcGuestFailureState *state,unsigned mnemonic) {
    static atomic_flag reporting=ATOMIC_FLAG_INIT;
    if(atomic_flag_test_and_set_explicit(&reporting,memory_order_relaxed))return;
    sotc_cpu_field_trace_freeze();
    sotc_cpu_region_dump();
    sotc_event_journal_failure(&sotc_trace_journal,3,state);
    fprintf(stderr,"CPU instruction failure: guest_rip=%#llx mnemonic=%u thread=%#llx; pre-exit snapshot, original exit retained\n",
        (unsigned long long)state->rip,mnemonic,(unsigned long long)state->thread);
    BbGuestRegs regs;memcpy(&regs,state->regs,sizeof(state->regs));regs.rip=state->rip;
    fprintf(stderr,"  rax=%#llx rcx=%#llx rdx=%#llx rbx=%#llx rsp=%#llx rbp=%#llx rsi=%#llx rdi=%#llx\n",
        (unsigned long long)state->regs[0],(unsigned long long)state->regs[1],(unsigned long long)state->regs[2],(unsigned long long)state->regs[3],
        (unsigned long long)state->regs[4],(unsigned long long)state->regs[5],(unsigned long long)state->regs[6],(unsigned long long)state->regs[7]);
    fprintf(stderr,"  r8=%#llx r9=%#llx r10=%#llx r11=%#llx r12=%#llx r13=%#llx r14=%#llx r15=%#llx\n",
        (unsigned long long)state->regs[8],(unsigned long long)state->regs[9],(unsigned long long)state->regs[10],(unsigned long long)state->regs[11],
        (unsigned long long)state->regs[12],(unsigned long long)state->regs[13],(unsigned long long)state->regs[14],(unsigned long long)state->regs[15]);
    sotc_cpu_fault_context(STDERR_FILENO,&regs,bb_image_base,fault_image_size);
    sotc_cpu_field_trace_dump_limited(STDERR_FILENO,128);
}
void sotc_cpu_pointer_copy_write(uintptr_t address,size_t size,unsigned actor,uintptr_t input) {
    sotc_pointer_copy(address,size,actor,input);
}
void sotc_shader_prefix_watch(uint64_t address,uint64_t packet,uint64_t capture) {
    const int saved=errno;sotc_shader_prefix_register(address,packet,capture);
    sotc_shader_backing_prefix_registered(address,packet,capture);errno=saved;
}
void sotc_cpu_shader_write_observe(uintptr_t address,uint64_t low,uint64_t high,
        uint64_t size,uint64_t rip,unsigned route) {
    const int saved=errno;
    sotc_shader_backing_cpu_write(address,low,high,size,rip,route);
    errno=saved;
}
void sotc_shader_gpu_write(uint64_t address,const void* data,uint64_t size,unsigned source,uint64_t sequence,uintptr_t caller) {
    if(source<11)sotc_shader_backing_observe(address,data,size,3+source,0,caller,sequence);
}
void sotc_shader_writable_binding(uint64_t address,uint64_t size,uint64_t shader,uint64_t slot,uint64_t declared) {
    sotc_shader_backing_binding(address,size,shader,slot,declared,0);
}
void sotc_shader_paged_writable_binding(uint64_t address,uint64_t size,uint64_t shader,uint64_t slot,uint64_t declared) {
    sotc_shader_backing_binding(address,size,shader,slot,declared,1);
}
void sotc_shader_image_writable_binding(uint64_t address,uint64_t size,uint64_t shader,uint64_t slot,uint64_t declared) {
    sotc_shader_backing_binding(address,size,shader,slot,declared,2);
}
void sotc_shader_target_writable_binding(uint64_t address,uint64_t size,uint64_t shader,uint64_t slot,uint64_t declared) {
    sotc_shader_backing_binding(address,size,shader,slot,declared,3);
}
void sotc_shader_resolved_image_writable_binding(uint64_t address,uint64_t size,uint64_t shader,uint64_t slot,uint64_t declared) {
    sotc_shader_backing_binding(address,size,shader,slot,declared,4);
}
int sotc_shader_prefix_watch_snapshot(uint64_t address,uint64_t out[8]) {
    return out?sotc_shader_prefix_snapshot(address,out):4;
}
void sotc_cpu_field_trace_on_stack_failure(const SotcGuestFailureState *state) {
    sotc_cpu_field_trace_freeze();
    sotc_event_journal_failure(&sotc_trace_journal,1,state);
    const int ready=sotc_fiber_failure_prepare(STDERR_FILENO);
    static const char reason[]="STOP: guest stack protector detected corruption\n";
    (void)write(STDERR_FILENO,reason,sizeof(reason)-1);
    if (!ready) return;
    char line[512];
    const int n=snprintf(line,sizeof(line),
        "Guest stack protector failure: available=0x%llx guest_rip=0x%llx rsp=0x%llx rbp=0x%llx fs_base=0x%llx stack_top=0x%llx thread=0x%llx; raw import state, not validated caller or canary\n",
        (unsigned long long)state->available,(unsigned long long)state->rip,
        (unsigned long long)state->regs[4],(unsigned long long)state->regs[5],
        (unsigned long long)state->fs_base,(unsigned long long)state->stack_top,
        (unsigned long long)state->thread);
    if (n>0 && (size_t)n<sizeof(line)) (void)write(STDERR_FILENO,line,(size_t)n);
    if (state->available) {
        BbGuestRegs regs;memcpy(&regs,state->regs,sizeof(state->regs));regs.rip=state->rip;
        sotc_cpu_fault_context(STDERR_FILENO,&regs,bb_image_base,fault_image_size);
    }
    sotc_cpu_field_trace_dump(STDERR_FILENO);
}
void sotc_cpu_field_trace_on_fiber_failure(const SotcFiberFailure *evidence) {
    sotc_cpu_field_trace_fiber_failure(STDERR_FILENO,evidence);
}
/* The renderer uses _Exit for assertions, so atexit cannot retain this history.
 * This optional exported hook only snapshots existing atomic records. */
void sotc_cpu_field_trace_on_gpu_assert(void) {
    static atomic_flag reporting = ATOMIC_FLAG_INIT;
    if (!atomic_flag_test_and_set_explicit(&reporting, memory_order_relaxed)) {
        sotc_cpu_field_trace_freeze();
        sotc_cpu_region_dump();
        sotc_cpu_field_trace_dump(STDERR_FILENO);
    }
}
typedef struct { uint64_t at, size, flags; } Segment;
typedef struct { uint64_t at, kind, value, addend; } Reloc;
typedef struct {
    uint64_t base, size, init, tls_at, tls_memsz, tls_filesz, tls_module;
    int ngs2_provider;
} Module;

static _Noreturn void fail(const char *message) {
    fprintf(stderr, "ERROR: %s\n", message);
    exit(1);
}
static uint64_t read64(FILE *f) {
    unsigned char b[8];
    if (fread(b, 1, sizeof(b), f) != sizeof(b)) fail("truncated input");
    uint64_t n = 0;
    for (int i = 7; i >= 0; --i) n = (n << 8) | b[i];
    return n;
}
static int mapped(const Segment *s, uint64_t n, uint64_t at, uint64_t size) {
    for (uint64_t i = 0; i < n; ++i)
        if (at >= s[i].at && at - s[i].at <= s[i].size && size <= s[i].size - (at - s[i].at)) return 1;
    return 0;
}
static _Noreturn void stop_import(uint64_t index, uint64_t argument) {
    /* Only the first thread reports a terminating boundary. Other threads
     * cannot interleave a second partial dump before this one exits. */
    if (atomic_flag_test_and_set_explicit(&stopping, memory_order_relaxed))
        for (;;) pause();
    if (index >= import_count) fail("bad import index");
    printf("STOP_AT_IMPORT: index=%" PRIu64 " name=%s\n", index, imports[index]);
    printf("Guest first argument: %#" PRIx64 "\n", argument);
    printf("Guest thread: %s\n", runtime_thread_name());
    const uint64_t caller = bbcpu_import_return();
    if (caller >= bb_image_base) printf("Guest caller offset: %#" PRIx64 "\n", caller - bb_image_base);
    bbcpu_report();
    if (bootstrap) bootstrap_report();
    puts(entered_game ? "Game entry executed through bbcpu; initialization intentionally stopped."
                      : "Linked module initialization stopped before game entry.");
    fflush(NULL);
    _Exit(20);
}
static _Noreturn void guest_exit(void) { fail("guest finalizer reached"); }
static void timed_out(int sig) {
    (void)sig;
    const char message[] = "STOP_TIMEOUT: diagnostic budget exhausted\n";
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    _Exit(21);
}
static void fault(int sig, siginfo_t *info, void *context) {
    if (sig == SIGBUS && bbcpu_handle_alignment_fault(context)) return;
    if (BB_IS_ACCESS_FAULT(sig) && bootstrap_gpu_handle_fault(context, info->si_addr)) return;
    if (BB_IS_ACCESS_FAULT(sig) && runtime_fault_recover) {
        sigjmp_buf *recover = runtime_fault_recover;
        runtime_fault_recover = NULL;
        sigset_t unblock;
        sigemptyset(&unblock);
        sigaddset(&unblock, sig);
        pthread_sigmask(SIG_UNBLOCK, &unblock, NULL);
        siglongjmp(*recover, 1);
    }
    uint64_t minimum_pc=0,minimum_lr=0;
#if defined(__APPLE__) && defined(__aarch64__)
    const ucontext_t *minimum_context=context;
    minimum_pc=minimum_context->uc_mcontext->__ss.__pc;
    minimum_lr=minimum_context->uc_mcontext->__ss.__lr;
#endif
    sotc_cpu_field_trace_fatal_minimum(STDERR_FILENO,(unsigned)sig,
        (uintptr_t)info->si_addr,minimum_pc,minimum_lr);
    /* A second fatal fault must not recurse into diagnostics or wait for a
     * damaged reporter. Preserve termination and explicitly mark truncation. */
    if (atomic_flag_test_and_set_explicit(&fault_reporting,memory_order_relaxed)) {
        const char repeated[]="CPU fault context: concurrent or nested fatal fault; report truncated\n";
        (void)write(STDERR_FILENO,repeated,sizeof(repeated)-1);
        _Exit(128+sig);
    }
    sotc_cpu_field_trace_freeze();
    sotc_cpu_region_dump();
    /* Publish registers before rich mapping/provider diagnostics or guest reads.
     * No recorder mutex/loader/file initialization is entered from this signal. */
    BbGuestRegs regs;
    const unsigned guest_context_kind=(unsigned)sotc_cpu_fault_context_kind(context);
    const int have_guest_regs=guest_context_kind && bbcpu_fault_guest_regs(context,&regs);
    if (have_guest_regs) {
        SotcGuestFailureState state={.available=1,.rip=regs.rip,.thread=(uintptr_t)pthread_self()};
        memcpy(state.regs,&regs,sizeof(state.regs));
        sotc_event_journal_failure(&sotc_trace_journal,guest_context_kind,&state);
    }
    /* Signal-safe header: report the failure type/address even when it is
     * outside registered guest code. Recovered GPU/speculative faults return
     * above and never emit a fatal diagnostic. */
    char header[96];
    const char *label = sig == SIGBUS ? "STOP_EXECUTION_FAULT: SIGBUS address=0x"
                      : sig == SIGILL ? "STOP_EXECUTION_FAULT: SIGILL address=0x"
                      : "STOP_EXECUTION_FAULT: SIGSEGV address=0x";
    size_t length = 0;
    while (*label) header[length++] = *label++;
    const uintptr_t address = (uintptr_t)info->si_addr;
    int started = 0;
    for (int shift = (int)(sizeof(address)*8)-4; shift >= 0; shift -= 4) {
        const unsigned digit = (unsigned)((address >> shift) & 15);
        if (digit || started || !shift) { header[length++] = "0123456789abcdef"[digit]; started = 1; }
    }
    header[length++] = '\n';
    (void)write(STDERR_FILENO, header, length);
    /* Keep these fields available in interpreter mode too. ESR distinguishes
     * alignment/access faults; VM protection distinguishes a GPU write trap
     * from a missing/read-only mapping or a fault in shared backing storage.
     * Do not touch the faulting bytes or change their protection. */
    char detail[256];
    size_t used=fault_hex(detail,"  signal code=0x",(uint32_t)info->si_code);
#if defined(__APPLE__) && defined(__aarch64__)
    const ucontext_t *uc=context;
    used+=fault_hex(detail+used," esr=0x",uc->uc_mcontext->__es.__esr);
    used+=fault_hex(detail+used," far=0x",uc->uc_mcontext->__es.__far);
    used+=fault_hex(detail+used," host pc=0x",uc->uc_mcontext->__ss.__pc);
#endif
    used+=fault_hex(detail+used," gpu traps=0x",runtime_memory_trap_reasons(address));
    detail[used++]='\n';
    (void)write(STDERR_FILENO,detail,used);
#if defined(__APPLE__) && defined(__aarch64__)
    sotc_host_fault_ucontext(STDERR_FILENO,context);
#endif
    const char mapping_label[]="  host mapping: ";
    (void)write(STDERR_FILENO,mapping_label,sizeof(mapping_label)-1);
    if (bb_describe_mapping(address,detail,sizeof(detail)))
        (void)write(STDERR_FILENO,detail,strnlen(detail,sizeof(detail)));
    else {
        const char missing[]="unmapped or unavailable\n";
        (void)write(STDERR_FILENO,missing,sizeof(missing)-1);
    }
    char description[2048];
    if (bbcpu_describe_fault(context, description, sizeof(description)))
        (void)write(STDERR_FILENO, description, strnlen(description, sizeof(description)));
    if (have_guest_regs) {
        sotc_cpu_fault_context(STDERR_FILENO,&regs,bb_image_base,fault_image_size);
    } else {
        const char unavailable[]="CPU fault context: unavailable outside translated code\n";
        (void)write(STDERR_FILENO,unavailable,sizeof(unavailable)-1);
    }
    sotc_cpu_field_trace_dump(STDERR_FILENO);
    _Exit(128 + sig);
}

enum { STACK_SIZE = 8 * 1024 * 1024 };
typedef struct {
    int linked;
    unsigned budget;
    unsigned char *image;
    uint64_t procparam, main_tls[4], module_count, entry;
    Module *modules;
    void *stack;
    uint64_t args[2];
} GuestRun;
static void *run_guest(void *opaque) {
    const GuestRun *run = opaque;
    alarm(run->budget); /* Zero cancels the duration alarm; parent cancellation remains independent. */
    if (run->linked && bootstrap) {
        runtime_thread_attach_main();
        for (uint64_t i = 0; i < run->module_count; ++i) {
            const uint64_t init_args[3] = {0};
            printf("Starting linked module %" PRIu64 " initializer\n", i);
            const int result = (int)bbcpu_call(bb_image_base + run->modules[i].init, 3, init_args);
            printf("Linked module %" PRIu64 " initializer returned %d\n", i, result);
            if (result) fail("module initializer failed");
            if (run->modules[i].ngs2_provider) bootstrap_modules_publish_ngs2();
        }
    }
    entered_game = 1;
    bbcpu_call_on_stack(bb_image_base + run->entry, 2, run->args, run->stack, STACK_SIZE - 64);
    fail("entry unexpectedly returned");
}
int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    SotcCpuInterpRegion experiment_region;
    if(sotc_cpu_region_parse(getenv("SOTC_CPU_INTERPRET_RANGE"),&experiment_region)) {
        fputs("Invalid SOTC_CPU_INTERPRET_RANGE: expected one bounded 0xbegin:0xend interval\n",stderr);
        return 24;
    }
    if(experiment_region.enabled && getenv("BB_JIT") && !strcmp(getenv("BB_JIT"),"0")) {
        fputs("Interpreter range comparison requires BB_JIT=1\n",stderr);
        return 24;
    }
    if (sotc_cpu_fault_fields_init(getenv("SOTC_CPU_FAULT_FIELDS"))) {
        fputs("Invalid SOTC_CPU_FAULT_FIELDS: expected reg:offset[:words], up to 16 fields and 1..8 words each\n",stderr);
        return 24;
    }
    if (sotc_cpu_field_trace_init(getenv("SOTC_CPU_FIELD_TRACE"))) {
        fputs("Invalid SOTC_CPU_FIELD_TRACE: expected site:reg:offset[:on-change[:match-reg:match-image-offset]], up to 24 reviewed block-entry sites\n",stderr);
        return 24;
    }
    if (sotc_pointer_init(getenv("SOTC_CPU_POINTER_TRACE"))) {
        fputs("Invalid SOTC_CPU_POINTER_TRACE: expected site:reg:offset:inner, up to twenty-six reviewed entries\n",stderr);
        return 24;
    }
    if (sotc_diagnostic_control_start()) return 28;
    if (sotc_trace_site_count)
        sotc_event_journal_init(&sotc_trace_journal,"cpu-events.bin",sotc_trace_site_count,128);
    if (sotc_pointer_count)
        sotc_event_journal_init(&sotc_pointer_journal,"pointer-events.bin",sotc_pointer_count+2,128);
    if(sotc_pointer_journal.map) {
        sotc_pointer_journal.map[179]=1;
        sotc_pointer_journal.map[180]=SOTC_POINTER_PERSISTENT;
        sotc_pointer_journal.map[181]=sizeof(sotc_pointer_persistent)+sizeof(sotc_pointer_pages);
        sotc_pointer_journal.map[183]=1; /* Per-probe persistent admission refusals at 192+. */
        sotc_pointer_journal.map[184]=SOTC_POINTER_GROUPS;
        sotc_pointer_journal.map[185]=SOTC_POINTER_GROUP_SLOTS;
        sotc_pointer_journal.map[186]=1; /* Independent bank counters at 224+. */
    }
    sotc_shader_prefix_init();
    sotc_shader_backing_init();
    /* Isolated prerequisites: no boot image, game mount or CPU initialization. */
    if (argc == 3 && !strcmp(argv[1], "--gpu-load-check"))
        return bootstrap_gpu_load_check(argv[2]);
    if (argc == 3 && !strcmp(argv[1], "--renderer-check"))
        return bootstrap_gpu_renderer_check(argv[2]);
    const char *game_root = NULL;
    const char *gpu_library = NULL;
    if (argc == 7 && !strcmp(argv[5], "--graphics-library")) {
        gpu_library = argv[6];
        argc = 5;
    }
    if ((argc == 3 || argc == 5) && !strcmp(argv[2], "--bootstrap")) {
        bootstrap = 1;
        if (argc == 5) {
            if (strcmp(argv[3], "--game-root")) fail("expected --game-root");
            game_root = argv[4];
        }
    } else if (argc != 2) fail("usage: probe_entry_arm64 IMAGE [--bootstrap [--game-root DIR]]");
    if (gpu_library && (!bootstrap || !game_root)) fail("graphics requires linked bootstrap and a read-only game root");
    unsigned budget = gpu_library ? 0 : 30;
    const char *configured_budget = getenv("SOTC_DIAGNOSTIC_SECONDS");
    if (configured_budget) {
        const size_t length = strlen(configured_budget);
        if (!length || length > 4) fail("SOTC_DIAGNOSTIC_SECONDS must be 0..1800 (0 means no duration limit)");
        budget = 0;
        for (size_t i = 0; i < length; ++i) {
            if (configured_budget[i] < '0' || configured_budget[i] > '9')
                fail("SOTC_DIAGNOSTIC_SECONDS must be 0..1800 (0 means no duration limit)");
            budget = budget * 10 + (unsigned)(configured_budget[i] - '0');
        }
        if (budget > 1800) fail("SOTC_DIAGNOSTIC_SECONDS must be 0..1800 (0 means no duration limit)");
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) fail("cannot open input");
    char magic[8];
    if (fread(magic, 1, 8, f) != 8) fail("truncated header");
    const int linked = !memcmp(magic, "BBPROBE5", 8);
    if (!linked && memcmp(magic, "BBPROBE2", 8)) fail("expected original BBPROBE2 or arm64-linked BBPROBE5 input");
    const uint64_t size = read64(f), entry = read64(f), ns = read64(f), nr = read64(f);
    import_count = read64(f);
    const uint64_t capabilities = read64(f);
    if (!size || size > 512 * 1024 * 1024 || entry >= size || !ns || ns > 64 ||
        nr > 1000000 || import_count > 100000 || capabilities > 1) fail("input limits exceeded");
    uint64_t procparam = 0, main_tls[4] = {0}, module_count = 0;
    Module modules[8] = {0};
    uint64_t *bindings = calloc(import_count ? (size_t)import_count : 1, sizeof(*bindings));
    uint64_t *binding_kinds = calloc(import_count ? (size_t)import_count : 1, sizeof(*binding_kinds));
    if (!bindings || !binding_kinds) fail("binding allocation failed");
    if (linked) {
        procparam = read64(f);
        for (unsigned i = 0; i < 4; ++i) main_tls[i] = read64(f);
        module_count = read64(f);
        if (!module_count || module_count > 8 || main_tls[1] > main_tls[2] || main_tls[2] > 1024 * 1024 ||
            main_tls[0] > size || main_tls[1] > size - main_tls[0] || main_tls[3] > 4096 ||
            (main_tls[3] & (main_tls[3] - 1))) fail("invalid linked header");
        for (uint64_t i = 0; i < module_count; ++i) {
            Module *m = &modules[i];
            m->base = read64(f); m->size = read64(f); m->init = read64(f); m->tls_at = read64(f);
            m->tls_memsz = read64(f); m->tls_filesz = read64(f); m->tls_module = read64(f);
            if (m->base >= size || !m->size || m->size > size - m->base || m->init < m->base ||
                m->init - m->base >= m->size || m->tls_module > 7 || m->tls_filesz > m->tls_memsz ||
                m->tls_memsz > 1024 * 1024 || (m->tls_module && (m->tls_module < 2 || m->tls_at >= size ||
                m->tls_memsz > size - m->tls_at))) fail("invalid linked module");
        }
        const uint64_t nb = read64(f);
        if (nb > import_count) fail("invalid binding count");
        for (uint64_t i = 0; i < nb; ++i) {
            const uint64_t index = read64(f), address = read64(f), kind = read64(f);
            int inside = 0;
            for (uint64_t j = 0; j < module_count; ++j)
                if (address >= modules[j].base && address - modules[j].base < modules[j].size) inside = 1;
            if (index >= import_count || bindings[index] || !inside || (kind != 1 && kind != 2)) fail("invalid binding");
            bindings[index] = address; binding_kinds[index] = kind;
        }
    }
    if (gpu_library && !linked) fail("graphics requires BBPROBE5 linked input");
    Segment *segments = calloc((size_t)ns, sizeof(*segments));
    Reloc *relocs = calloc(nr ? (size_t)nr : 1, sizeof(*relocs));
    imports = calloc(import_count ? (size_t)import_count : 1, sizeof(*imports));
    if (!segments || !relocs || !imports) fail("allocation failed");
    int executable_entry = 0;
    for (uint64_t i = 0; i < ns; ++i) {
        Segment *s = &segments[i];
        s->at = read64(f); s->size = read64(f); s->flags = read64(f);
        if (s->at > size || s->size > size - s->at || s->flags > 7) fail("invalid segment");
        if ((s->flags & 1) && entry >= s->at && entry - s->at < s->size) executable_entry = 1;
    }
    if (!executable_entry) fail("entry is not executable");
    if(experiment_region.enabled) {
        int executable_range=0;
        for(uint64_t i=0;i<ns;++i)
            if((segments[i].flags&1) && experiment_region.begin>=segments[i].at &&
               experiment_region.end<=segments[i].at+segments[i].size)executable_range=1;
        if(!executable_range)fail("CPU interpreter range is outside an executable image segment");
    }
    if (linked && (!mapped(segments, ns, procparam, 64) || !mapped(segments, ns, main_tls[0], main_tls[1])))
        fail("unmapped process parameter or main TLS");
    for (uint64_t i = 0; i < module_count; ++i)
        if (!mapped(segments, ns, modules[i].init, 1) || (modules[i].tls_module &&
            !mapped(segments, ns, modules[i].tls_at, modules[i].tls_memsz))) fail("unmapped module metadata");
    if (fread(imports, sizeof(*imports), (size_t)import_count, f) != import_count) fail("truncated imports");
    for (uint64_t i = 0; i < import_count; ++i)
        if (!memchr(imports[i], 0, sizeof(*imports))) fail("unterminated import name");
    for (uint64_t i = 0; i < nr; ++i) {
        Reloc *r = &relocs[i];
        r->at = read64(f); r->kind = read64(f); r->value = read64(f); r->addend = read64(f);
        if (r->kind > 2 || !mapped(segments, ns, r->at, 8) ||
            (r->kind && r->value >= import_count) || (!r->kind && r->value >= size) ||
            (r->kind != 2 && r->addend) || r->addend >= 16384) fail("invalid relocation");
    }
    unsigned char *image = runtime_low_map((size_t)size, PROT_READ | PROT_WRITE);
    unsigned char *traps = runtime_low_map((size_t)(import_count + 1) * 32, PROT_READ | PROT_WRITE);
    unsigned char *data = runtime_low_map((size_t)(import_count + 1) * 16384, PROT_NONE);
    if (!image || !traps || !data) fail("mapping failed");
    if (fread(image, 1, (size_t)size, f) != size || fgetc(f) != EOF) fail("incorrect image size");
    fclose(f);
    const char *startup_site=getenv("SOTC_SKIP_INTRO_SITE");
    if(startup_site&&*startup_site) {
        char *end=NULL;unsigned long long site=strtoull(startup_site,&end,0);
        if(!end||end==startup_site||*end||startup_site[0]=='-'||site>=size)
            fail("invalid no-intro command-line site");
        unsigned char *startup_text=runtime_low_map(16384,PROT_READ|PROT_WRITE);
        if(!startup_text||sotc_startup_no_intro(image,(size_t)size,(size_t)site,startup_text,16384)||
           mprotect(startup_text,16384,PROT_READ))fail("no-intro command-line validation failed");
        puts("Engine startup option: +nointro (original initialization retained)");
    }
    if (bootstrap) {
        if (!(capabilities & 1)) fail("bootstrap requires verified libc capability");
        bootstrap_initialize(capabilities);
        bootstrap_set_linked_libc(linked);
        if (game_root && bootstrap_mount_game_root(game_root)) fail("cannot mount read-only game root");
    }
    bb_image_base = (uintptr_t)image;
    fault_image_size = size;
    for (unsigned i=0;i<sotc_trace_site_count;++i) {
        const uint64_t at=sotc_trace_sites[i].site;
        int executable=0;
        if (at>=size || at>UINT64_MAX-bb_image_base) fail("CPU field trace site outside image");
        if (sotc_trace_sites[i].has_match &&
            (sotc_trace_sites[i].match_offset>=size ||
             sotc_trace_sites[i].match_offset>UINT64_MAX-bb_image_base))
            fail("CPU field trace match outside image");
        for (uint64_t j=0;j<ns;++j)
            if ((segments[j].flags&1) && at>=segments[j].at &&
                at-segments[j].at<segments[j].size) executable=1;
        if (!executable) fail("CPU field trace site is not executable");
    }
    sotc_cpu_field_trace_install(bb_image_base);
    sotc_pointer_image=bb_image_base;sotc_pointer_size=size;
    for(unsigned i=0;i<sotc_pointer_count;++i) {
        const uint64_t at=sotc_pointer_sites[i].site;int executable=0;
        if(at>=size || at>UINT64_MAX-bb_image_base)fail("CPU pointer trace site outside image");
        for(uint64_t j=0;j<ns;++j)if((segments[j].flags&1) && at>=segments[j].at &&
            at-segments[j].at<segments[j].size)executable=1;
        if(!executable)fail("CPU pointer trace site is not executable");
        bbcpu_set_probe(bb_image_base+at,sotc_pointer_probe,i);
    }
    if(sotc_pointer_count)printf("CPU pointer trace: %u configured pointer probes; imported memcpy/memset overlap observers; not exhaustive writers\n",sotc_pointer_count);
    if (sotc_trace_site_count)
        printf("CPU field trace: %u configured block-entry probes, %u retained observations; private diagnostic only\n",
               sotc_trace_site_count,SOTC_TRACE_RECORDS);
    /* Publish all guest TLS templates before the GPU can create host workers. */
    if (linked && bootstrap) {
        bootstrap_set_procparam(image + procparam);
        runtime_set_main_tls(image + main_tls[0], main_tls[1], main_tls[2], main_tls[3]);
        for (uint64_t i = 0; i < module_count; ++i) {
            const Module *m = &modules[i];
            if (m->tls_module) bootstrap_set_module_tls(m->tls_module, image + m->tls_at, m->tls_filesz, m->tls_memsz);
        }
    }

    if (gpu_library) {
        struct sigaction gpu_action = {.sa_sigaction = fault, .sa_flags = SA_SIGINFO};
        sigemptyset(&gpu_action.sa_mask);
        sigaction(SIGBUS, &gpu_action, NULL); sigaction(SIGSEGV, &gpu_action, NULL); sigaction(SIGILL, &gpu_action, NULL);
        uint64_t sdk;
        memcpy(&sdk, image + procparam + 16, sizeof(sdk));
        if (bootstrap_gpu_start(gpu_library, (uint32_t)sdk)) fail("graphics provider initialization failed");
#if defined(__APPLE__) && defined(__aarch64__)
        sotc_host_image_inventory();
#endif
    }

    for (uint64_t i = 0; i < import_count; ++i) {
        unsigned char *t = traps + i * 32;
        const uint32_t index = (uint32_t)i;
        const uintptr_t handler = (uintptr_t)stop_import;
        /* Synthetic x86 SysV bridge: preserve arg0 in rsi; pass import id in edi. */
        const unsigned char prefix[] = {0x48, 0x89, 0xfe, 0xbf};
        memcpy(t, prefix, sizeof(prefix)); memcpy(t + 4, &index, 4);
        t[8] = 0x48; t[9] = 0xb8; memcpy(t + 10, &handler, 8);
        t[18] = 0xff; t[19] = 0xe0;
    }
    for (uint64_t i = 0; i < nr; ++i) {
        const Reloc *r = &relocs[i];
        uintptr_t value = r->kind == 1 ? (uintptr_t)(traps + r->value * 32)
            : r->kind == 2 ? (uintptr_t)(data + r->value * 16384 + r->addend)
            : bb_image_base + r->value;
        if (bootstrap && r->kind) {
            const uintptr_t resolved = bootstrap_resolve(imports[r->value], r->kind == 2);
            if (resolved) value = resolved + r->addend;
            else if (linked && bindings[r->value]) {
                const uint64_t address = bindings[r->value];
                if (binding_kinds[r->value] != r->kind || !mapped(segments, ns, address, r->addend + 1))
                    fail("native binding kind or range mismatch");
                if (r->kind == 1) {
                    int executable = 0;
                    for (uint64_t j = 0; j < ns; ++j)
                        if ((segments[j].flags & 1) && address >= segments[j].at &&
                            address - segments[j].at < segments[j].size) executable = 1;
                    if (!executable) fail("native function is not executable");
                }
                value = bb_image_base + address + r->addend;
            }
        }
        memcpy(image + r->at, &value, 8);
    }
    for (uint64_t i = 0; i < ns; ++i)
        if (segments[i].flags & 1) bbcpu_add_guest_code(bb_image_base + segments[i].at, (size_t)segments[i].size);
    /* NGS2 becomes a loadable static provider only when both representative
     * exports bind to executable guest code in the same linked module. An
     * import name, host override or failed initializer cannot enable it. */
    if (linked && bootstrap) {
        uint64_t create = 0, render = 0;
        for (uint64_t i=0; i<import_count; ++i) if (binding_kinds[i] == 1 && bindings[i] &&
                !bootstrap_resolve(imports[i], 0) && bbcpu_is_guest(bb_image_base + bindings[i])) {
            if (!strcmp(imports[i], "mPYgU4oYpuY#z#t")) create = bindings[i];
            if (!strcmp(imports[i], "i0VnXM-C9fc#z#t")) render = bindings[i];
        }
        if (create && render) {
            unsigned matches = 0;
            for (uint64_t i=0; i<module_count; ++i) {
                Module *m = &modules[i];
                if (create >= m->base && create-m->base < m->size &&
                    render >= m->base && render-m->base < m->size) {
                    m->ngs2_provider = 1; ++matches;
                }
            }
            if (matches != 1) fail("NGS2 exports require a single linked module");
        }
    }
    bbcpu_add_guest_code((uintptr_t)traps, (size_t)(import_count + 1) * 32);
    struct sigaction action = {.sa_sigaction = fault, .sa_flags = SA_SIGINFO};
    sigemptyset(&action.sa_mask);
    sigaction(SIGBUS, &action, NULL); sigaction(SIGSEGV, &action, NULL); sigaction(SIGILL, &action, NULL);
    signal(SIGALRM, timed_out);
#if defined(__APPLE__) && defined(__aarch64__)
    if (!gpu_library) sotc_host_image_inventory();
#endif
    setenv("BB_JIT_FAULT_MAP", "1", 1);
    void *stack = runtime_low_map(STACK_SIZE, PROT_READ | PROT_WRITE);
    if (!stack) fail("stack mapping failed");
    struct { uint64_t count; const char *args[2]; } params = {1, {"/app0/eboot.bin", NULL}};
    const uint64_t args[] = {(uintptr_t)&params, (uintptr_t)guest_exit};
    printf("Diagnostic only: %s; %s; %s; no save access.\n",
           bootstrap ? "narrow bootstrap contracts" : "all imports stop",
           game_root ? "read-only game-content mounts" : "no game-file API access",
           gpu_library ? "experimental graphics provider" : "no renderer");
    printf("Image size: %" PRIu64 "; entry offset: %#" PRIx64 "; relocations: %" PRIu64 "\n", size, entry, nr);
    puts("High mapping is temporary: this does not validate SotC's packed pointers.");
    if (budget) printf("Diagnostic watchdog: %u seconds\n", budget);
    else puts("Diagnostic duration limit: none");
    GuestRun run = {.linked = linked, .budget = budget, .image = image,
                    .procparam = procparam, .module_count = module_count,
                    .entry = entry, .modules = modules, .stack = stack,
                    .args = {args[0], args[1]}};
    memcpy(run.main_tls, main_tls, sizeof(main_tls));
    if (gpu_library) {
        pthread_t guest;
        if (pthread_create(&guest, NULL, run_guest, &run)) fail("cannot start guest main thread");
        bootstrap_gpu_window_loop();
        fail("GPU window loop unexpectedly returned");
    }
    run_guest(&run);
}
