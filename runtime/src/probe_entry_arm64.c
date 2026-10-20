/* Diagnostic BBPROBE2 reader: stop at the first unimplemented PS4 import.
 * Default: all imports stop. --bootstrap: narrow verified startup contracts only.
 * Format and entry convention: AjncSystem/ShadowOfTheColossus-PC, revision
 * 2250f328feafab953aac85e758e7bddb2e80e891, scripts/prepare.py and src/probe.c.
 * CPU API: bloodborne-native/bloodborne-recomp, f749c155c2ff3bb932795de85c9c8fe9c4aa59ad.
 * Upstream foundations: deadinside28/bbport and shadPS4. No game code is embedded.
 */
#include "bbcpu.h"
#include "bootstrap_contracts.h"
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

static char (*imports)[128];
static uint64_t import_count;
static int bootstrap;
static _Atomic int entered_game;
static atomic_flag stopping = ATOMIC_FLAG_INIT;
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
    char description[2048];
    if (bbcpu_describe_fault(context, description, sizeof(description)))
        (void)write(STDERR_FILENO, description, strnlen(description, sizeof(description)));
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
    alarm(run->budget);
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
    unsigned budget = 30;
    const char *configured_budget = getenv("SOTC_DIAGNOSTIC_SECONDS");
    if (configured_budget) {
        const size_t length = strlen(configured_budget);
        if (!length || length > 2) fail("SOTC_DIAGNOSTIC_SECONDS must be 1..60");
        budget = 0;
        for (size_t i = 0; i < length; ++i) {
            if (configured_budget[i] < '0' || configured_budget[i] > '9')
                fail("SOTC_DIAGNOSTIC_SECONDS must be 1..60");
            budget = budget * 10 + (unsigned)(configured_budget[i] - '0');
        }
        if (!budget || budget > 60) fail("SOTC_DIAGNOSTIC_SECONDS must be 1..60");
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
    if (bootstrap) {
        if (!(capabilities & 1)) fail("bootstrap requires verified libc capability");
        bootstrap_initialize(capabilities);
        bootstrap_set_linked_libc(linked);
        if (game_root && bootstrap_mount_game_root(game_root)) fail("cannot mount read-only game root");
    }
    bb_image_base = (uintptr_t)image;
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
    printf("Diagnostic watchdog: %u seconds\n", budget);
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
