/* Synthetic lifecycle and guest-callback tests; no game input. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "platform.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

void *runtime_low_map(size_t size, int prot);
void bootstrap_test_clocks(void);
void bootstrap_test_files(void);
void bootstrap_test_rwlocks(void);
static int sequence[8], count;
static void note(void *arg) { sequence[count++] = *(int *)arg; }
static void plain(void) { sequence[count++] = 9; }
static void recursive(void *arg) { note(arg); bootstrap_finalize(NULL); }
static void test_console(void) {
    int64_t (*write_guest)(int, const void *, uint64_t) = (void *)bootstrap_resolve("FxVZqBAA7ks#0#F", 0);
    int32_t *(*guest_error)(void) = (void *)bootstrap_resolve("9BcDykPmo1I#0#F", 0);
    assert(write_guest && guest_error && guest_error() == runtime_errno());
    assert(!bootstrap_resolve("FxVZqBAA7ks#1#u", 0));
    assert(!bootstrap_resolve("FxVZqBAA7ks#0#F", 1));
    unsigned char *bridge = runtime_low_map(16384, PROT_READ | PROT_WRITE);
    assert(bridge);
    /* Synthetic x86 tail call to the typed host write function. */
    bridge[0] = 0x48; bridge[1] = 0xb8;
    const uintptr_t target = (uintptr_t)write_guest;
    memcpy(bridge + 2, &target, sizeof(target));
    bridge[10] = 0xff; bridge[11] = 0xe0;
    bbcpu_add_guest_code((uintptr_t)bridge, 16384);
    const unsigned char message[] = {'a', 0, 'b', '\n'};
    assert(write_guest(0, message, sizeof(message)) == -1 && *guest_error() == 9);
    const uint64_t invalid_args[] = {0, (uintptr_t)message, sizeof(message)};
    assert(bbcpu_call((uintptr_t)bridge, 3, invalid_args) == UINT64_MAX && *guest_error() == 9);
    assert(write_guest(1, NULL, 1) == -1 && *guest_error() == 14);
    assert(write_guest(1, message, UINT64_MAX) == -1 && *guest_error() == 22);
    *guest_error() = 17;
    assert(write_guest(1, NULL, 0) == 0 && *guest_error() == 17);
    for (int fd = 1; fd <= 2; ++fd) {
        fflush(NULL);
        int saved = dup(fd), pipe_fds[2];
        assert(saved > 2 && pipe(pipe_fds) == 0);
        /* Even an open host descriptor is not a guest console descriptor. */
        assert(write_guest(pipe_fds[1], message, sizeof(message)) == -1 && *guest_error() == 9);
        assert(dup2(pipe_fds[1], fd) == fd && close(pipe_fds[1]) == 0);
        const uint64_t args[] = {(uint64_t)fd, (uintptr_t)message, sizeof(message)};
        assert(bbcpu_call((uintptr_t)bridge, 3, args) == sizeof(message));
        assert(dup2(saved, fd) == fd);
        unsigned char received[sizeof(message)];
        assert(read(pipe_fds[0], received, sizeof(received)) == (ssize_t)sizeof(received));
        assert(!memcmp(message, received, sizeof(message)));
        assert(close(pipe_fds[0]) == 0);
        assert(close(fd) == 0);
        assert(write_guest(fd, message, sizeof(message)) == -1 && *guest_error() == 9);
        assert(dup2(saved, fd) == fd && close(saved) == 0);
    }
    puts("PASS: x86-to-ARM64 console writes, stdout/stderr bytes including NUL, descriptor isolation, errors and guest errno");
}
typedef struct {
    uintptr_t start, end; uint64_t offset; int32_t protection, memory_type;
    uint32_t flags; char name[32];
} QueryInfo;
_Static_assert(sizeof(QueryInfo) == 72, "PS4 virtual query layout");
/* Exercise the real upstream VMA table; reserving address space needs no
 * shared-memory object. Direct-memory alias checks additionally need the
 * normal Terminal's POSIX shared-memory capability. No game input is used. */
static void test_memory(int direct_memory) {
    int32_t (*reserve)(void **, uint64_t, int, uint64_t) = (void *)bootstrap_resolve("7oxv3PPCumo#0#F", 0);
    int32_t (*unmap)(void *, uint64_t) = (void *)bootstrap_resolve("cQke9UuBQOk#0#F", 0);
    int32_t (*query)(const void *, int, QueryInfo *, uint64_t) = (void *)bootstrap_resolve("rVjRvHJ0X6c#0#F", 0);
    uint64_t (*ring_address)(void) = (void *)bootstrap_resolve("ln33zjBrfjk#A#B", 0);
    assert(reserve && unmap && query && ring_address);
    assert(!bootstrap_resolve("ln33zjBrfjk#0#F", 0));
    const uint64_t system_size = UINT64_C(1) << 32, page = 16384;
    void *occupied = (void *)BB_USER_MIN;
    assert(reserve(&occupied, system_size, 0x10, system_size) == 0);
    uint64_t ring = ring_address();
    assert(ring == ring_address() && ring >= BB_USER_MIN && ring < BB_USER_MAX);
    assert(ring < (UINT64_C(1) << 40) && !(ring % page));
    QueryInfo info;
    assert(query((void *)ring, 0, &info, sizeof(info)) == 0);
    assert(info.end - info.start == system_size && !info.flags);
    assert(info.start >= (uintptr_t)occupied + system_size && info.protection == 0);
    void *collision = (void *)ring;
    assert(reserve(&collision, page, 0x10, 0) == (int32_t)UINT32_C(0x8002000c));
    void *outside = (void *)UINT64_C(0x7f0000000);
    assert(reserve(&outside, page, 0x10, 0) == (int32_t)UINT32_C(0x80020016));
    void *following = NULL;
    assert(reserve(&following, page, 0, 0) == 0 && (uintptr_t)following >= info.end);
    assert(unmap(following, page) == 0 && unmap(occupied, system_size) == 0);
    puts("PASS: stable 40-bit tessellation address, collision-aware system reservation and fixed-map bounds");
    if (!direct_memory) return;
    int32_t (*allocate)(int64_t, int64_t, uint64_t, uint64_t, int, int64_t *) =
        (void *)bootstrap_resolve("rTXw65xmLIA#0#F", 0);
    int32_t (*map)(void **, uint64_t, int, int, int64_t, uint64_t) =
        (void *)bootstrap_resolve("L-Q3LEjIbgA#0#F", 0);
    int32_t (*raw_map)(void **, uint64_t, int, int, int64_t, uint64_t) =
        (void *)runtime_memory_resolve("L-Q3LEjIbgA#p#J");
    int32_t (*release)(uint64_t, uint64_t) = (void *)bootstrap_resolve("MBuItvba6z8#0#F", 0);
    int32_t (*available)(int64_t, int64_t, uint64_t, int64_t *, uint64_t *) =
        (void *)bootstrap_resolve("C0f7TJcbfac#0#F", 0);
    int32_t (*flexible_available)(uint64_t *) = (void *)bootstrap_resolve("aNz11fnnzi4#0#F", 0);
    int32_t (*map_flexible)(void **, uint64_t, int, int) = (void *)bootstrap_resolve("IWIBBdTHit4#0#F", 0);
    assert(allocate && map && raw_map && release && available && flexible_available && map_flexible);
    int64_t physical = -1;
    const uint64_t bytes = 131072;
    const int64_t budget = INT64_C(5056) << 20;
    int64_t free_start = -1;
    uint64_t free_bytes = 0;
    assert(available(0, budget, 0, &free_start, &free_bytes) == 0 && free_start == 0 && free_bytes == (uint64_t)budget);
    assert(allocate(0, INT64_C(5056) << 20, bytes, page, 3, &physical) == 0);
    assert(physical == 0);
    assert(available(0, budget, 0, &free_start, &free_bytes) == 0 && free_start == (int64_t)bytes && free_bytes == (uint64_t)budget - bytes);
    assert(available(0, bytes, 0, &free_start, &free_bytes) == (int32_t)UINT32_C(0x8002000c));
    void *mapped = (void *)ring, *alias = NULL;
    assert(map(&mapped, bytes, 0x33, 0x10, physical, page) == 0 && (uintptr_t)mapped == ring);
    assert(map(&alias, bytes, 0x33, 0, physical, page) == 0 && alias != mapped);
    assert((uintptr_t)alias >= BB_USER_MIN && (uintptr_t)alias < BB_USER_MAX);
    volatile unsigned char *a = mapped, *b = alias;
    a[0] = 0x5a; a[bytes - 1] = 0xc3;
    assert(b[0] == 0x5a && b[bytes - 1] == 0xc3);
    b[17] = 0xa7; assert(a[17] == 0xa7);
    assert(query(mapped, 0, &info, sizeof(info)) == 0 && info.offset == (uint64_t)physical);
    assert(info.flags == 18 && info.protection == 0x33);
    collision = mapped;
    assert(raw_map(&collision, bytes, 0x33, 0x90, physical, page) == (int32_t)UINT32_C(0x8002000c));
    assert(raw_map(&outside, bytes, 0x33, 0x10, physical, page) == (int32_t)UINT32_C(0x80020016));
    assert(a[0] == 0x5a && b[17] == 0xa7);
    assert(release((uint64_t)physical, bytes) == 0);
    assert(query(mapped, 0, &info, sizeof(info)) == (int32_t)UINT32_C(0x8002000d));
    assert(query(alias, 0, &info, sizeof(info)) == (int32_t)UINT32_C(0x8002000d));
    assert(available(0, budget, 0, &free_start, &free_bytes) == 0 && free_start == 0 && free_bytes == (uint64_t)budget);
    puts("PASS: relocated fixed direct map, shared physical aliases, overwrite rejection and release");
    uint64_t flex_before, flex_during, flex_after;
    void *flex = NULL;
    assert(flexible_available(&flex_before) == 0);
    assert(map_flexible(&flex, page, 3, 0) == 0);
    assert(flexible_available(&flex_during) == 0 && flex_during == flex_before - page);
    assert(unmap(flex, page) == 0);
    assert(flexible_available(&flex_after) == 0 && flex_after == flex_before);
    puts("PASS: availability queries track real direct/flexible allocation and release");
}
int main(int argc, char **argv) {
    assert(argc == 1 || (argc == 2 && !strcmp(argv[1], "--direct-memory")));
    const char *init = "bzQExy189ZI#1#u";
    bootstrap_initialize(0);
    assert(!bootstrap_resolve(init, 0));
    bootstrap_initialize(1);
    assert(bootstrap_resolve(init, 0));
    assert(!bootstrap_resolve(init, 1));
    assert(!bootstrap_resolve("bzQExy189ZI#q#q", 0));
    uint64_t *canary = (void *)bootstrap_resolve("f7uOxY9mM1U#0#F", 1);
    assert(canary && *canary && !(*canary & 255));
    void *(*allocate)(size_t) = (void *)bootstrap_resolve("gQX+4GDQjpM#1#u", 0);
    void *(*resize)(void *, size_t) = (void *)bootstrap_resolve("Y7aJ1uydPMo#1#u", 0);
    void (*release)(void *) = (void *)bootstrap_resolve("tIhsqj0qsFE#1#u", 0);
    unsigned char *memory = allocate(32);
    assert(memory && (uintptr_t)memory < (UINT64_C(1) << 40) && !((uintptr_t)memory & 15));
    memset(memory, 0x5a, 32);
    unsigned char *larger = resize(memory, 128);
    assert(larger);
    for (unsigned i = 0; i < 32; ++i) assert(larger[i] == 0x5a);
    release(larger);
    int32_t (*attr_init)(void **) = (void *)bootstrap_resolve("dQHWEsJtoE4#p#F", 0);
    int32_t (*attr_type)(void **, int) = (void *)bootstrap_resolve("mDmgMOGVUqg#p#F", 0);
    int32_t (*attr_destroy)(void **) = (void *)bootstrap_resolve("HF7lK46xzjY#p#F", 0);
    int32_t (*attr_protocol)(void **, int) = (void *)bootstrap_resolve("5txKfcMUAok#p#F", 0);
    int32_t (*mutex_init)(void **, void **) = (void *)bootstrap_resolve("ttHNfU+qDBU#p#F", 0);
    int32_t (*mutex_lock)(void **) = (void *)bootstrap_resolve("7H0iTOciTLo#p#F", 0);
    int32_t (*mutex_unlock)(void **) = (void *)bootstrap_resolve("2Z+PpY6CaJg#p#F", 0);
    int32_t (*mutex_destroy)(void **) = (void *)bootstrap_resolve("ltCfaGr2JGE#p#F", 0);
    assert(attr_init && attr_type && attr_destroy && mutex_init && mutex_lock && mutex_unlock && mutex_destroy);
    assert(!bootstrap_resolve("dQHWEsJtoE4#1#u", 0));
    assert(attr_init(NULL) == 22); /* POSIX returns guest errno, not a kernel error. */
    void *attr = NULL, *mutex = NULL;
    assert(attr_init(&attr) == 0 && attr_type(&attr, 2) == 0);
    assert(attr_protocol(&attr, 0) == 0 && attr_protocol(&attr, -1) == 22);
    assert(mutex_init(&mutex, &attr) == 0 && attr_destroy(&attr) == 0 && !attr);
    assert(mutex_lock(&mutex) == 0 && mutex_lock(&mutex) == 0);
    assert(mutex_unlock(&mutex) == 0 && mutex_unlock(&mutex) == 0);
    assert(mutex_destroy(&mutex) == 0 && mutex_destroy(&mutex) == 22);
    int (*atexit_)(void (*)(void)) = (void *)bootstrap_resolve("8G2LB+A3rzg#1#u", 0);
    int (*cxa)(void (*)(void *), void *, void *) = (void *)bootstrap_resolve("tsvEmnenz48#1#u", 0);
    assert(atexit_(NULL) == -1 && cxa(NULL, NULL, NULL) == -1);
    int first = 1, second = 2, third = 3, a, b;
    assert(cxa(note, &first, &a) == 0);
    assert(cxa(note, &second, &b) == 0);
    assert(atexit_(plain) == 0);
    assert(!count); /* Registration must not invoke a destructor. */
    bootstrap_finalize(&a);
    assert(count == 1 && sequence[0] == 1);
    bootstrap_finalize(&a);
    assert(count == 1);
    assert(cxa(recursive, &third, &a) == 0);
    bootstrap_finalize(NULL);
    assert(count == 4 && sequence[1] == 3 && sequence[2] == 9 && sequence[3] == 2);
    bootstrap_finalize(NULL);
    assert(count == 4);
    unsigned char *guest = runtime_low_map(16384, PROT_READ | PROT_WRITE);
    assert(guest);
    /* mov qword [rdi],42; ret -- synthetic x86-64 callback. */
    const unsigned char code[] = {0x48,0xc7,0x07,42,0,0,0,0xc3};
    memcpy(guest, code, sizeof(code));
    bbcpu_add_guest_code((uintptr_t)guest, 16384);
    uint64_t result = 0;
    assert(cxa((void *)guest, &result, &b) == 0);
    bootstrap_finalize(&b);
    assert(result == 42);
    bootstrap_finalize(NULL);
    puts("PASS: scoped resolution, capability gate, 40-bit heap/realloc, POSIX recursive mutex lifecycle/errors, destructor order/recursion and x86 guest callback");
    test_console();
    bootstrap_test_rwlocks();
    bootstrap_test_clocks();
    bootstrap_test_files();
    test_memory(argc == 2);
}
