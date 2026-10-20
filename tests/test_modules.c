/* Synthetic x86 calls to the module loader, absent optical metadata profile
 * and unavailable services. No game input or physical-disc placement assumed. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static uintptr_t guest_bridge(uintptr_t host) {
    static unsigned char *page; static size_t used;
    if (!page) { page = runtime_low_map(16384, PROT_READ | PROT_WRITE); assert(page); }
    unsigned char *p = page + used; used += 16;
    p[0] = 0x48; p[1] = 0xb8; memcpy(p + 2, &host, 8); p[10] = 0xff; p[11] = 0xe0;
    bbcpu_add_guest_code((uintptr_t)p, 12);
    return (uintptr_t)p;
}
static uint32_t load(uintptr_t fn, uint32_t id, int32_t bytes, const void *arguments,
                      uint64_t reserved, int32_t *initializer_result) {
    const uint64_t args[] = {id, (uint32_t)bytes, (uintptr_t)arguments, reserved, (uintptr_t)initializer_result};
    return (uint32_t)bbcpu_call(fn, 5, args);
}
static void check_display_safe_area(uintptr_t entry) {
    unsigned char buffer[148], expected[148];
    const float ratio = 1.0f;
    *runtime_errno() = 17;
    for (unsigned offset = 0; offset < 4; ++offset) {
        memset(buffer, 0xa5, sizeof(buffer)); memcpy(expected, buffer, sizeof(buffer));
        memcpy(expected + 8 + offset, &ratio, sizeof(ratio));
        const uint64_t args[] = {(uintptr_t)(buffer + 8 + offset), 1, UINT64_MAX, 3, 5, 7};
        for (unsigned repeat = 0; repeat < 2; ++repeat) {
            assert((uint32_t)bbcpu_call(entry, 6, args) == 0);
            assert(!memcmp(buffer, expected, sizeof(buffer)) && *runtime_errno() == 17);
        }
    }
    const uint64_t null_args[] = {0, (uintptr_t)buffer, UINT64_MAX, 3, 5, 7};
    assert((uint32_t)bbcpu_call(entry, 6, null_args) == 0x80a10003);
    assert(!memcmp(buffer, expected, sizeof(buffer)) && *runtime_errno() == 17);
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    unsigned char *guarded = mmap(NULL, page * 2, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(guarded != MAP_FAILED);
    memset(guarded, 0xa5, page);
    assert(mprotect(guarded + page, page, PROT_NONE) == 0);
    /* Place just the defined ratio before a protected page. This detects
     * both reads and writes of reserved fields, not only buffer overruns. */
    const uint64_t args[] = {(uintptr_t)(guarded + page - sizeof(ratio))};
    assert((uint32_t)bbcpu_call(entry, 1, args) == 0);
    assert(!memcmp(guarded + page - sizeof(ratio), &ratio, sizeof(ratio)));
    for (size_t i = 0; i < page - sizeof(ratio); ++i) assert(guarded[i] == 0xa5);
    assert(munmap(guarded, page * 2) == 0 && *runtime_errno() == 17);
}
static void test_display_safe_area(void) {
    const char *name = "1n37q1Bvc5Y#a#a";
    bootstrap_initialize(0); assert(!bootstrap_resolve(name, 0));
    bootstrap_initialize(1);
    const uintptr_t fn = bootstrap_resolve(name, 0); assert(fn);
    assert(!bootstrap_resolve(name, 1));
    assert(!bootstrap_resolve("1n37q1Bvc5Y#A#a", 0));
    assert(!bootstrap_resolve("1n37q1Bvc5Y#a#A", 0));
    check_display_safe_area(guest_bridge(fn));
    puts("PASS: logical safe-area guest ABI, float ratio, untouched reserved bytes, unaligned/protected-page outputs, null error, repeated calls, scopes and errno");
}
static uintptr_t guest_stack_argument(uintptr_t host, uint32_t seventh) {
    unsigned char *p = runtime_low_map(16384, PROT_READ | PROT_WRITE); assert(p);
    /* push imm32; movabs host,%rax; call *%rax; add $8,%rsp; ret.
     * Six register arguments pass through, with the seventh on the guest
     * stack and SysV call alignment preserved. No host stack forwarding is
     * required for an argument-independent error provider. */
    const unsigned char code[] = {0x68,0,0,0,0, 0x48,0xb8,0,0,0,0,0,0,0,0,
                                  0xff,0xd0, 0x48,0x83,0xc4,0x08, 0xc3};
    memcpy(p, code, sizeof(code)); memcpy(p + 1, &seventh, 4); memcpy(p + 7, &host, 8);
    bbcpu_add_guest_code((uintptr_t)p, sizeof(code)); return (uintptr_t)p;
}
static void check_gnm_failure(uintptr_t owner, uintptr_t resource) {
    const uintptr_t entries[] = {guest_bridge(owner), guest_stack_argument(resource, 0x76543210),
                                guest_stack_argument(resource, UINT32_MAX)};
    uint64_t buffer[] = {UINT64_C(0x123456789abcdef0), UINT64_MAX};
    const uint64_t initial[] = {buffer[0], buffer[1]};
    const uint64_t pointers[] = {0, 1, UINT64_MAX, (uintptr_t)buffer};
    *runtime_errno() = 17;
    for (unsigned repeat = 0; repeat < 2; ++repeat)
        for (unsigned e = 0; e < 3; ++e)
            for (unsigned p = 0; p < 4; ++p) {
                const uint64_t args[] = {pointers[p], pointers[p], pointers[p],
                                        UINT64_MAX, pointers[p], UINT64_MAX};
                assert((uint32_t)bbcpu_call(entries[e], 6, args) == 0x8eee00ff);
                assert(!memcmp(buffer, initial, sizeof(buffer)) && *runtime_errno() == 17);
            }
}
static void check_compute_queue_failure(uintptr_t entry) {
    _Alignas(256) uint64_t buffer[32] = {UINT64_C(0x123456789abcdef0), UINT64_MAX};
    unsigned char initial[sizeof(buffer)]; memcpy(initial, buffer, sizeof(buffer));
    const uint64_t pipes[] = {0, 6, 7, UINT32_MAX, UINT64_C(0x100000000)};
    const uint64_t queues[] = {0, 7, 8, UINT32_MAX, UINT64_C(0x100000000)};
    const uint64_t sizes[] = {0, 1, 2, 3, UINT32_C(0x80000000), UINT32_MAX, UINT64_C(0x100000001)};
    const uint64_t rings[] = {0, 1, (uintptr_t)buffer, UINT64_C(0xffffffffffffff00)};
    const uint64_t readers[] = {0, 1, (uintptr_t)buffer, UINT64_C(0xfffffffffffffffc)};
    *runtime_errno() = 17;
    for (unsigned p = 0; p < 5; ++p) for (unsigned q = 0; q < 5; ++q)
        for (unsigned z = 0; z < 7; ++z) for (unsigned r = 0; r < 4; ++r)
            for (unsigned a = 0; a < 4; ++a) {
                const uint32_t size = (uint32_t)sizes[z];
                const uint64_t args[] = {pipes[p], queues[q], rings[r], sizes[z], readers[a]};
                uint32_t expected = 0x80d170ff;
                if ((uint32_t)pipes[p] >= 7) expected = 0x80d17000;
                else if ((uint32_t)queues[q] >= 8) expected = 0x80d17001;
                else if (size && (size & (uint32_t)(0u - size)) != size) expected = 0x80d17002;
                else if (rings[r] % 256) expected = 0x80d17003;
                else if (readers[a] % 4) expected = 0x80d17004;
                assert((uint32_t)bbcpu_call(entry, 5, args) == expected);
                assert(!memcmp(buffer, initial, sizeof(buffer)) && *runtime_errno() == 17);
            }
}
static void test_unavailable_compute_queue(void) {
    const char *name = "29oKvKXzEZo#A#B";
    bootstrap_initialize(0); assert(!bootstrap_resolve(name, 0));
    bootstrap_initialize(1);
    const uintptr_t fn = bootstrap_resolve(name, 0); assert(fn);
    assert(!bootstrap_resolve(name, 1));
    assert(!bootstrap_resolve("29oKvKXzEZo#B#B", 0));
    assert(!bootstrap_resolve("29oKvKXzEZo#A#A", 0));
    assert(!bootstrap_resolve("ArSg-TGinhk#A#B", 0)); /* No queue to unmap. */
    const uintptr_t entry = guest_bridge(fn);
    check_compute_queue_failure(entry); check_compute_queue_failure(entry);
    puts("PASS: GNM compute queue guest ABI, retail validation order/widths and zero-size prefix, alignment, explicit backend failure, unchanged buffers/errno and repeated calls");
}
static void test_unavailable_gnm(void) {
    const char *names[] = {"ZFqKFl23aMc#A#B", "nvEwfYAImTs#A#B"};
    bootstrap_initialize(0);
    for (unsigned i = 0; i < 2; ++i) assert(!bootstrap_resolve(names[i], 0));
    bootstrap_initialize(1);
    for (unsigned i = 0; i < 2; ++i) assert(bootstrap_resolve(names[i], 0) && !bootstrap_resolve(names[i], 1));
    assert(!bootstrap_resolve("ZFqKFl23aMc#B#B", 0));
    assert(!bootstrap_resolve("ZFqKFl23aMc#A#A", 0));
    assert(!bootstrap_resolve("nvEwfYAImTs#B#B", 0));
    assert(!bootstrap_resolve("nvEwfYAImTs#A#A", 0));
    assert(!bootstrap_resolve("unknown#A#B", 0));
    check_gnm_failure(bootstrap_resolve(names[0], 0), bootstrap_resolve(names[1], 0));
    puts("PASS: retail GNM owner/resource registration errors, scoped capability, ignored register/seventh stack arguments, untouched outputs/errno and repeated calls");
}
static void test_unavailable_share(void) {
    const char *name = "j7DlalBzHh8#o#o";
    bootstrap_initialize(0); assert(!bootstrap_resolve(name, 0));
    bootstrap_initialize(1);
    const uintptr_t fn = bootstrap_resolve(name, 0); assert(fn);
    assert(!bootstrap_resolve(name, 1));
    assert(!bootstrap_resolve("j7DlalBzHh8#o#p", 0));
    assert(!bootstrap_resolve("j7DlalBzHh8#p#o", 0));
    assert(!bootstrap_resolve("j7DlalBzHh8#0#F", 0));
    assert(!bootstrap_resolve("8hZ2EEl2Tto#o#o", 0)); /* No share menu. */
    const uintptr_t entry = guest_bridge(fn);
    uint64_t buffer[] = {UINT64_C(0x123456789abcdef0), UINT64_MAX};
    const uint64_t before[] = {buffer[0], buffer[1]};
    const uint64_t modes[] = {0, 1, 2, UINT64_C(0x100000001), UINT64_MAX};
    const uint64_t sizes[] = {0, 0x1ffff, 0x20000, UINT64_C(1) << 32, UINT64_MAX};
    const uint64_t pointers[] = {0, 1, (uintptr_t)buffer};
    *runtime_errno() = 17;
    for (unsigned repeat = 0; repeat < 2; ++repeat)
        for (unsigned m = 0; m < sizeof(modes) / sizeof(*modes); ++m)
            for (unsigned z = 0; z < sizeof(sizes) / sizeof(*sizes); ++z)
                for (unsigned p = 0; p < sizeof(pointers) / sizeof(*pointers); ++p) {
                    /* The service allocation fails before the final thread
                     * arguments are inspected, including invalid pointers. */
                    const uint64_t args[] = {modes[m], sizes[z], UINT64_MAX,
                                             pointers[p], 3, 5};
                    const uint32_t expected = modes[m] != 1 || sizes[z] < 0x20000
                                              ? 0x81170003 : 0x81170002;
                    assert((uint32_t)bbcpu_call(entry, 6, args) == expected);
                    assert(!memcmp(buffer, before, sizeof(buffer)) && *runtime_errno() == 17);
                }
    puts("PASS: ShareUtility Ex2 guest ABI, 64-bit mode/size validation, early resource failure, untouched attributes/buffers/errno, repeated errors and scoped capability");
}
static void test_unavailable_network(void) {
    const char *name = "Nlev7Lg8k3A#Bc#D";
    bootstrap_initialize(0);
    assert(!bootstrap_resolve(name, 0));
    bootstrap_initialize(1);
    const uintptr_t fn = bootstrap_resolve(name, 0);
    assert(fn && !bootstrap_resolve(name, 1));
    assert(!bootstrap_resolve("Nlev7Lg8k3A#E#F", 0));
    assert(!bootstrap_resolve("Nlev7Lg8k3A#0#F", 0));
    assert(!bootstrap_resolve("Nlev7Lg8k3A#Bc#G", 0));
    /* A no-argument API must ignore stale registers, including invalid pointer
     * values. Encoded network errors must not overwrite kernel/libc errno. */
    uint64_t buffer[] = {UINT64_C(0x123456789abcdef0), UINT64_C(0x23456789abcdef01)};
    const uint64_t original[] = {buffer[0], buffer[1]};
    const uint64_t junk[] = {1, UINT64_MAX, (uintptr_t)buffer, 3, 5, 7};
    const uintptr_t entry = guest_bridge(fn);
    *runtime_errno() = 17;
    for (unsigned i = 0; i < 3; ++i) {
        assert((uint32_t)bbcpu_call(entry, 0, NULL) == UINT32_C(0x8041012d));
        assert((uint32_t)bbcpu_call(entry, 6, junk) == UINT32_C(0x8041012d));
    }
    assert(!memcmp(buffer, original, sizeof(buffer)) && *runtime_errno() == 17);
    /* No other network API, including an apparent successful termination or
     * a guest-visible network errno slot, is exposed by this error provider. */
    assert(!bootstrap_resolve("cTGkc6-TBlI#Bc#D", 0)); /* sceNetTerm */
    assert(!bootstrap_resolve("HQOwnfMGipQ#Bc#D", 0)); /* sceNetErrnoLoc */
    assert(!bootstrap_resolve("6MojQ8uFHEI#Bc#D", 0)); /* sceNetInitParam */
    puts("PASS: no-argument network-init guest ABI, repeated unsupported errors, capability/scopes, unchanged buffers/errno and unresolved network services");
}
static void test_unavailable_dialogs(void) {
    const char *common = "uoUpLGNkygk#c#c", *message = "lDqxaY1UbEo#g#g";
    bootstrap_initialize(0);
    assert(!bootstrap_resolve(common, 0) && !bootstrap_resolve(message, 0));
    bootstrap_initialize(1);
    const uintptr_t common_fn = bootstrap_resolve(common, 0), message_fn = bootstrap_resolve(message, 0);
    assert(common_fn && message_fn && !bootstrap_resolve(common, 1) && !bootstrap_resolve(message, 1));
    assert(!bootstrap_resolve("uoUpLGNkygk#0#F", 0) && !bootstrap_resolve("lDqxaY1UbEo#c#c", 0));
    /* Neither no-argument initializer may interpret stale argument registers
     * as a parameter block, acquire a module reference, or become initialized. */
    int32_t (*loaded)(uint32_t) = (void *)bootstrap_resolve("ynFKQ5bfGks#F#G", 0); assert(loaded);
    assert((uint32_t)loaded(0x80000018) == 0x805a1001 && (uint32_t)loaded(0xa4) == 0x805a1001);
    uint64_t buffer[] = {0x123456789abcdef0, 0x23456789abcdef01};
    const uint64_t initial[] = {buffer[0], buffer[1]};
    const uint64_t junk[] = {1, UINT64_MAX, (uintptr_t)buffer, 3, 5, 7};
    const uintptr_t init_common = guest_bridge(common_fn), init_message = guest_bridge(message_fn);
    *runtime_errno() = 17;
    assert((uint32_t)bbcpu_call(init_message, 6, junk) == 0x80b80001);
    for (unsigned i = 0; i < 2; ++i) {
        assert((uint32_t)bbcpu_call(init_common, 6, junk) == 0x80b8000f);
        assert((uint32_t)bbcpu_call(init_message, 6, junk) == 0x80b80001);
    }
    assert(!memcmp(buffer, initial, sizeof(buffer)) && *runtime_errno() == 17);
    assert((uint32_t)loaded(0x80000018) == 0x805a1001 && (uint32_t)loaded(0xa4) == 0x805a1001);
    assert(!bootstrap_resolve("BQ3tey0JmQM#c#c", 0)); /* Other dialog APIs stay unresolved. */
    puts("PASS: no-argument dialog guest ABI, explicit unavailability/dependency errors, capability/scopes, unchanged buffers/errno and module availability");
}
static void test_request_location(uintptr_t entry) {
    struct Output {
        uint64_t head, flags, middle1, first, middle2, count, tail;
    };
    const struct Output initial = {0x123456789abcdef0, 101, 0x213456789abcdef0,
                                  202, 0x313456789abcdef0, 303, 0x413456789abcdef0};
    struct Output out;
    uint64_t args[] = {(uintptr_t)"/app0/fixture.bin", 0, 1,
                      (uintptr_t)&out.flags, (uintptr_t)&out.first, (uintptr_t)&out.count};
    *runtime_errno() = 17;
    /* Required path/output pointers and signed range checks precede any write. */
    const unsigned null_arguments[] = {0, 3, 4, 5};
    for (unsigned i = 0; i < sizeof(null_arguments) / sizeof(*null_arguments); ++i) {
        const unsigned slot = null_arguments[i]; const uint64_t saved = args[slot];
        out = initial; args[slot] = 0;
        assert((uint32_t)bbcpu_call(entry, 6, args) == 0x81100001);
        assert(!memcmp(&out, &initial, sizeof(out))); args[slot] = saved;
    }
    const int64_t ranges[][2] = {{-1, 1}, {0, 0}, {0, -1}, {0, INT64_MIN}};
    for (unsigned i = 0; i < sizeof(ranges) / sizeof(*ranges); ++i) {
        out = initial; args[0] = 1; /* Must not read the path on invalid ranges. */
        args[1] = (uint64_t)ranges[i][0]; args[2] = (uint64_t)ranges[i][1];
        assert((uint32_t)bbcpu_call(entry, 6, args) == 0x81100001);
        assert(!memcmp(&out, &initial, sizeof(out)));
    }
    const char *paths[] = {"/app0/fixture.bin", "/app1/fixture.bin", "/hostapp/fixture.bin",
                          "relative.bin", "/app0", "/app10/fixture.bin", "/APP0/fixture.bin", ""};
    for (unsigned i = 0; i < sizeof(paths) / sizeof(*paths); ++i) {
        out = initial; args[0] = (uintptr_t)paths[i]; args[1] = 0; args[2] = 1;
        assert((uint32_t)bbcpu_call(entry, 6, args) == (i < 2 ? 0x81100003 : 0x81100002));
        struct Output expected = initial;
        expected.flags = expected.first = expected.count = UINT64_MAX;
        assert(!memcmp(&out, &expected, sizeof(out)));
    }
    out = initial; args[0] = (uintptr_t)paths[0]; args[1] = args[2] = INT64_MAX;
    args[4] = args[5] = args[3]; /* Aliased outputs still write exactly one qword. */
    assert((uint32_t)bbcpu_call(entry, 6, args) == 0x81100003);
    struct Output expected = initial; expected.flags = UINT64_MAX;
    assert(!memcmp(&out, &expected, sizeof(out)) && *runtime_errno() == 17);
}
int main(void) {
    test_display_safe_area();
    test_unavailable_gnm();
    test_unavailable_compute_queue();
    test_unavailable_share();
    test_unavailable_network();
    test_unavailable_dialogs();
    bootstrap_initialize(1);
    const char *name = "hHrGoGoNf+s#F#G";
    const uintptr_t fn = bootstrap_resolve(name, 0); assert(fn);
    assert(!bootstrap_resolve(name, 1) && !bootstrap_resolve("hHrGoGoNf+s#0#F", 0));
    const uintptr_t entry = guest_bridge(fn);
    int32_t (*loaded)(uint32_t) = (void *)bootstrap_resolve("ynFKQ5bfGks#F#G", 0);
    int32_t (*plain_load)(uint32_t) = (void *)bootstrap_resolve("39iV5E1HoCk#F#G", 0);
    int32_t (*unload)(uint16_t) = (void *)bootstrap_resolve("eR2bZFAAU0Q#F#G", 0);
    assert(loaded && plain_load && unload);
    struct { uint32_t head; int32_t result; uint32_t tail; } out = {0x12345678, 123, 0x87654321};
    assert(load(entry, 0, 0, NULL, 0, &out.result) == 0x805a1000);
    assert(load(entry, 0x80000000, 0, NULL, 0, &out.result) == 0x805a1000);
    assert(load(entry, 0xd7, 0, NULL, 0, &out.result) == 0x805a1001 && out.result == 123);
    assert(!bootstrap_resolve("lbQKqsERhtE#libSceDiscMap", 0));
    char root[] = "/tmp/sotc-discmap-XXXXXX"; assert(mkdtemp(root));
    char fixture[sizeof(root) + 32];
    assert(snprintf(fixture, sizeof(fixture), "%s/fixture.bin", root) < (int)sizeof(fixture));
    FILE *file = fopen(fixture, "wb"); assert(file);
    assert(fwrite("fixture", 1, 7, file) == 7 && fclose(file) == 0);
    assert(bootstrap_mount_game_root(root) == 0);
    assert(load(entry, 0xd7, 0, NULL, UINT64_C(1) << 40, &out.result) == 0x805a1000);
    assert(load(entry, 0x100d7, 0, NULL, 0, &out.result) == 0x805a1001);
    assert(load(entry, 0x80000006, 0, NULL, 0, &out.result) == 0x805a1001);
    assert(load(entry, 0xd7, -1, (void *)(uintptr_t)1, 0, &out.result) == 0x805a1001);
    assert((uint32_t)loaded(0xd7) == 0x805a1001 && out.result == 123);
    assert(load(entry, 0xd7, 0, NULL, 0, &out.result) == 0 && out.result == 0);
    assert(out.head == 0x12345678 && out.tail == 0x87654321 && loaded(0xd7) == 0);
    assert(plain_load(0xd7) == 0 && unload(0xd7) == 0 && loaded(0xd7) == 0);
    const uintptr_t location = bootstrap_resolve("fJgP+wqifno#libSceDiscMap", 0);
    assert(location && !bootstrap_resolve("fJgP+wqifno#0#F", 0));
    assert(!bootstrap_resolve("fJgP+wqifno#libSceDiscMap", 1));
    test_request_location(guest_bridge(location));
    const uintptr_t request = bootstrap_resolve("lbQKqsERhtE#libSceDiscMap", 0);
    const uintptr_t package = bootstrap_resolve("fl1eoDnwQ4s#libSceDiscMap", 0);
    const uintptr_t query = bootstrap_resolve("ioKMruft1ek#libSceDiscMap", 0);
    assert(request && package && query);
    assert(!bootstrap_resolve("lbQKqsERhtE#0#F", 0) && !bootstrap_resolve("lbQKqsERhtE#libSceDiscMap", 1));
    int32_t hdd = 101;
    uint64_t values[] = {UINT64_C(0x1122334455667788), UINT64_C(0x8877665544332211), UINT64_MAX - 1};
    const uint64_t original[] = {values[0], values[1], values[2]};
    const uint64_t request_args[] = {(uintptr_t)"/app0/not-a-disc.bin", 17, 4096, (uintptr_t)&hdd};
    assert((uint32_t)bbcpu_call(guest_bridge(request), 4, request_args) == 0x81100004 && hdd == 101);
    const uint64_t package_args[] = {0, (uintptr_t)&values[1], (uintptr_t)&values[2]};
    assert((uint32_t)bbcpu_call(guest_bridge(package), 3, package_args) == 0x81100004);
    const uint64_t query_args[] = {0, 0, 0, (uintptr_t)&values[0], (uintptr_t)&values[1], (uintptr_t)&values[2]};
    assert((uint32_t)bbcpu_call(guest_bridge(query), 6, query_args) == 0x81100004);
    assert(!memcmp(values, original, sizeof(values)));
    assert(unload(0xd7) == 0 && (uint32_t)loaded(0xd7) == 0x805a1001);
    assert((uint32_t)unload(0xd7) == 0x805a1001);
    out.result = 321;
    assert(load(entry, 0xad, 0, NULL, 0, &out.result) == 0x805a1001 && out.result == 321);
    bootstrap_unmount_game_root(); assert(unlink(fixture) == 0 && rmdir(root) == 0);
    assert(!bootstrap_resolve("fJgP+wqifno#libSceDiscMap", 0));
    puts("PASS: internal module-load ABI and references, DiscMap bitmap errors and six-argument request-location errors, 64-bit outputs, range/namespace validation and errno preservation");
    return 0;
}
