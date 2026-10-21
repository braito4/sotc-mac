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
    if (!page) {
        page = runtime_low_map(16384, PROT_READ | PROT_WRITE); assert(page);
        /* All original synthetic trampolines share one guest-code page;
         * registering each stub separately exhausts the CPU range table. */
        bbcpu_add_guest_code((uintptr_t)page, 16384);
    }
    assert(used + 16 <= 16384);
    unsigned char *p = page + used; used += 16;
    p[0] = 0x48; p[1] = 0xb8; memcpy(p + 2, &host, 8); p[10] = 0xff; p[11] = 0xe0;
    return (uintptr_t)p;
}
static uint32_t load(uintptr_t fn, uint32_t id, int32_t bytes, const void *arguments,
                      uint64_t reserved, int32_t *initializer_result) {
    const uint64_t args[] = {id, (uint32_t)bytes, (uintptr_t)arguments, reserved, (uintptr_t)initializer_result};
    return (uint32_t)bbcpu_call(fn, 5, args);
}
static void test_video_flip_event(void) {
    const char *name = "HXzjK9yI30k#G#H";
    bootstrap_initialize(0); assert(!bootstrap_resolve(name, 0));
    bootstrap_initialize(1);
    const uintptr_t fn = bootstrap_resolve(name, 0); assert(fn && !bootstrap_resolve(name, 1));
    assert(!bootstrap_resolve("HXzjK9yI30k#H#H", 0));
    assert(!bootstrap_resolve("HXzjK9yI30k#G#G", 0));
    const uintptr_t entry = guest_bridge(fn);
    uintptr_t queue = 0;
    int32_t (*create)(uintptr_t *, const char *) = (void *)bootstrap_equeues_resolve("D0OdFMjp46I#0#F");
    int32_t (*destroy)(uintptr_t) = (void *)bootstrap_equeues_resolve("jpFjmgAC5AE#0#F");
    assert(create(&queue, "fixture") == 0);
    const uint64_t queues[] = {0, 1, UINT64_MAX, queue};
    const uint64_t handles[] = {0, 1, 3, 4, UINT32_MAX, UINT64_MAX, 0x80290018, UINT64_C(0xffffffff80290018)};
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    void *protected = mmap(NULL, page, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0); assert(protected != MAP_FAILED);
    const uint64_t pointers[] = {0, 1, UINT64_MAX, (uintptr_t)protected};
    *runtime_errno() = 17;
    for (unsigned q = 0; q < 4; ++q) for (unsigned h = 0; h < 8; ++h) for (unsigned p = 0; p < 4; ++p) {
        const uint64_t args[] = {queues[q], handles[h], pointers[p]};
        assert((uint32_t)bbcpu_call(entry, 3, args) == 0x8029000b && *runtime_errno() == 17);
    }
    /* Registration failure leaves the real user-event queue empty. */
    int32_t (*wait)(uintptr_t, void *, int32_t, int32_t *, const uint32_t *) =
        (void *)bootstrap_equeues_resolve("fzyMKs9kim0#0#F");
    unsigned char event[32], initial[32]; memset(event, 0xa5, sizeof(event)); memcpy(initial, event, sizeof(event));
    int32_t n = -1; const uint32_t poll = 0;
    assert(wait(queue, event, 1, &n, &poll) == (int32_t)0x8002003c && n == 0 && !memcmp(event, initial, sizeof(event)));
    assert(destroy(queue) == 0 && munmap(protected, page) == 0 && *runtime_errno() == 17);
    puts("PASS: headless flip-event guest ABI, handle-before-queue validation, real/invalid queues, opaque user data, unchanged empty queue, scopes and errno");
}
static void check_video_flip_rate(uintptr_t entry) {
    const uint64_t handles[] = {0, 1, 3, 4, UINT32_MAX, UINT64_MAX,
                                0x80290018, UINT64_C(0xffffffff80290018)};
    const uint64_t rates[] = {0, 1, 2, 3, UINT32_MAX, UINT64_MAX};
    *runtime_errno() = 17;
    for (unsigned round = 0; round < 2; ++round)
        for (unsigned h = 0; h < 8; ++h) for (unsigned r = 0; r < 6; ++r) {
            const uint64_t args[] = {handles[h], rates[r]};
            assert((uint32_t)bbcpu_call(entry, 2, args) == 0x8029000b && *runtime_errno() == 17);
        }
}
static void test_video_flip_rate(void) {
    const char *name = "CBiu4mCE1DA#G#H";
    bootstrap_initialize(0); assert(!bootstrap_resolve(name, 0));
    bootstrap_initialize(1);
    const uintptr_t fn = bootstrap_resolve(name, 0); assert(fn && !bootstrap_resolve(name, 1));
    assert(!bootstrap_resolve("CBiu4mCE1DA#H#H", 0));
    assert(!bootstrap_resolve("CBiu4mCE1DA#G#G", 0));
    check_video_flip_rate(guest_bridge(fn));
    puts("PASS: headless flip-rate guest ABI, invalid-handle precedence, repeated failures/rate extremes, scopes and errno");
}
static void check_video_adjust_color(uintptr_t entry) {
    const uint64_t handles[] = {0, 1, 2, 3, 4, UINT32_MAX, UINT64_MAX,
        UINT32_C(0x80290018), UINT64_C(0xffffffff80290018), UINT64_C(0x100000001)};
    const uint64_t sizes[] = {0, 11, 12, 16, 17, UINT32_MAX, UINT64_MAX};
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    unsigned char *unreadable = mmap(NULL, page, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(unreadable != MAP_FAILED);
    uint64_t buffer[] = {UINT64_C(0x123456789abcdef0), UINT64_MAX};
    const uint64_t initial[] = {buffer[0], buffer[1]};
    const uint64_t pointers[] = {0, 1, (uintptr_t)buffer, (uintptr_t)unreadable};
    *runtime_errno() = 17;
    for (unsigned repeat = 0; repeat < 2; ++repeat)
        for (unsigned h = 0; h < 10; ++h) for (unsigned z = 0; z < 7; ++z)
            for (unsigned p = 0; p < 4; ++p) {
                const uint64_t args[] = {handles[h], pointers[p], sizes[z], 3, 5, 7};
                assert((uint32_t)bbcpu_call(entry, 6, args) == 0x8029000b);
                assert(!memcmp(buffer, initial, sizeof(buffer)) && *runtime_errno() == 17);
            }
    assert(munmap(unreadable, page) == 0 && *runtime_errno() == 17);
}
static void test_video_adjust_color(void) {
    const char *name = "pv9CI5VC+R0#G#H";
    bootstrap_initialize(0); assert(!bootstrap_resolve(name, 0));
    bootstrap_initialize(1);
    const uintptr_t fn = bootstrap_resolve(name, 0); assert(fn && !bootstrap_resolve(name, 1));
    assert(!bootstrap_resolve("pv9CI5VC+R0#H#H", 0));
    assert(!bootstrap_resolve("pv9CI5VC+R0#G#G", 0));
    check_video_adjust_color(guest_bridge(fn));
    puts("PASS: headless color-adjust guest ABI, invalid-handle precedence including failed opens, 32-bit handle/size, untouched null/guarded settings, repeated errors, scopes and errno");
}
static void test_video_capabilities(void) {
    const char *name = "kGVLc3htQE8#G#H";
    bootstrap_initialize(0); assert(!bootstrap_resolve(name, 0));
    bootstrap_initialize(1);
    const uintptr_t fn = bootstrap_resolve(name, 0); assert(fn && !bootstrap_resolve(name, 1));
    assert(!bootstrap_resolve("kGVLc3htQE8#H#H", 0));
    assert(!bootstrap_resolve("kGVLc3htQE8#G#G", 0));
    /* The same invalid-handle matrix covers full-width size arguments and
     * protected/null outputs, before any actual capability-layout access. */
    check_video_adjust_color(guest_bridge(fn));
    puts("PASS: headless capability-query guest ABI, invalid-handle precedence, full-width sizes, untouched protected/null outputs and errno");
}
static uint32_t gamma_call(uintptr_t entry, void *output, uint64_t size, uint32_t bits) {
    uint64_t gpr[6] = {(uintptr_t)output, size, UINT64_MAX, 3, 5, 7}, result[2];
    uint8_t xmm[8][16], result_xmm[2][16]; memset(xmm, 0xa5, sizeof(xmm));
    memcpy(xmm[0], &bits, sizeof(bits));
    bbcpu_call_full(entry, gpr, (const uint8_t (*)[16])xmm, result, result_xmm);
    return (uint32_t)result[0];
}
static void check_video_gamma(uintptr_t entry) {
    const struct { uint32_t bits; int valid; } cases[] = {
        {0,0}, {0x80000000,0}, {0x3dcccccc,0}, {0x3dcccccd,1},
        {0x3f000000,1}, {0x3f800000,1}, {0x40000000,1}, {0x40000001,0},
        {0xbf800000,0}, {0x7f800000,0}, {0xff800000,0}, {0x00000001,0},
        {0x7fc12345,1}, {0xffc12345,1}, {0x7f812345,1}, {0xff812345,1}
    };
    const uint64_t sizes[] = {0, 4, 11, 12, 13, 15, 16, 17, UINT32_MAX,
                              UINT64_C(0x10000000c), UINT64_C(0x100000010)};
    unsigned char buffer[40], expected[40];
    *runtime_errno() = 17;
    for (unsigned repeat = 0; repeat < 2; ++repeat)
        for (unsigned c = 0; c < sizeof(cases) / sizeof(*cases); ++c)
            for (unsigned z = 0; z < sizeof(sizes) / sizeof(*sizes); ++z)
                for (unsigned unaligned = 0; unaligned < 2; ++unaligned) {
                    memset(buffer, 0xa5, sizeof(buffer)); memcpy(expected, buffer, sizeof(buffer));
                    const uint32_t size = (uint32_t)sizes[z];
                    const int valid = cases[c].valid && (size == 12 || size == 16);
                    if (valid) {
                        memset(expected + 8 + unaligned, 0, size);
                        for (unsigned channel = 0; channel < 3; ++channel)
                            memcpy(expected + 8 + unaligned + channel * 4, &cases[c].bits, 4);
                    }
                    assert(gamma_call(entry, buffer + 8 + unaligned, sizes[z], cases[c].bits)
                           == (valid ? 0 : 0x80290001));
                    assert(!memcmp(buffer, expected, sizeof(buffer)) && *runtime_errno() == 17);
                    assert(gamma_call(entry, NULL, sizes[z], cases[c].bits) == 0x80290001);
                }
    /* Invalid sizes/ranges must fail before reading or writing the pointer. */
    assert(gamma_call(entry, (void *)1, 13, 0x3f800000) == 0x80290001);
    assert(gamma_call(entry, (void *)1, 12, 0x40000001) == 0x80290001);
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    unsigned char *guarded = mmap(NULL, page * 2, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(guarded != MAP_FAILED && mprotect(guarded + page, page, PROT_NONE) == 0);
    for (unsigned size = 12; size <= 16; size += 4) {
        memset(guarded, 0xa5, page);
        assert(gamma_call(entry, guarded + page - size, size, 0x3f800000) == 0);
        for (unsigned channel = 0; channel < 3; ++channel) {
            uint32_t bits; memcpy(&bits, guarded + page - size + channel * 4, 4);
            assert(bits == 0x3f800000);
        }
        if (size == 16) { uint32_t reserved; memcpy(&reserved, guarded + page - 4, 4); assert(!reserved); }
        for (size_t i = 0; i < page - size; ++i) assert(guarded[i] == 0xa5);
    }
    assert(munmap(guarded, page * 2) == 0 && *runtime_errno() == 17);
}
static void test_video_gamma(void) {
    const char *name = "DYhhWbJSeRg#G#H";
    bootstrap_initialize(0); assert(!bootstrap_resolve(name, 0));
    bootstrap_initialize(1);
    const uintptr_t fn = bootstrap_resolve(name, 0); assert(fn && !bootstrap_resolve(name, 1));
    assert(!bootstrap_resolve("DYhhWbJSeRg#H#H", 0));
    assert(!bootstrap_resolve("DYhhWbJSeRg#G#G", 0));
    check_video_gamma(guest_bridge(fn));
    puts("PASS: retail gamma helper mixed integer/float guest ABI, 12/16-byte layouts, three channels, range/NaN behavior, exact guarded output, invalid pointers/size, scopes and errno");
}
static void test_unavailable_video(void) {
    const char *name = "Up36PTk687E#G#H";
    bootstrap_initialize(0); assert(!bootstrap_resolve(name, 0));
    bootstrap_initialize(1);
    const uintptr_t fn = bootstrap_resolve(name, 0); assert(fn);
    assert(!bootstrap_resolve(name, 1));
    assert(!bootstrap_resolve("Up36PTk687E#H#H", 0));
    assert(!bootstrap_resolve("Up36PTk687E#G#G", 0));
    assert(!bootstrap_resolve("Up36PTk687E#C#D", 0));
    const uintptr_t entry = guest_bridge(fn);
    uint64_t buffer[] = {UINT64_C(0x123456789abcdef0), UINT64_MAX};
    const uint64_t initial[] = {buffer[0], buffer[1]};
    const uint64_t users[] = {0, 1, 0xff, UINT32_MAX, UINT64_MAX};
    const uint64_t buses[] = {0, 1, 4, 8, UINT32_MAX, UINT64_C(0x100000000)};
    const uint64_t indices[] = {0, 1, UINT32_MAX, UINT64_C(0x100000000)};
    const uint64_t params[] = {0, 1, (uintptr_t)buffer};
    *runtime_errno() = 17;
    for (unsigned repeat = 0; repeat < 2; ++repeat)
        for (unsigned u = 0; u < 5; ++u) for (unsigned b = 0; b < 6; ++b)
            for (unsigned i = 0; i < 4; ++i) for (unsigned p = 0; p < 3; ++p) {
                const uint64_t args[] = {users[u], buses[b], indices[i], params[p], 3, 5};
                const uint32_t expected = (uint32_t)buses[b] != 0 ? 0x80290016
                                        : (uint32_t)indices[i] != 0 ? 0x80290001 : 0x80290018;
                assert((uint32_t)bbcpu_call(entry, 6, args) == expected);
                assert(!memcmp(buffer, initial, sizeof(buffer)) && *runtime_errno() == 17);
            }
    /* A failed open creates no port. Close, resolution and submit-flip
     * remain unresolved; buffer registration has a checked error provider. */
    assert(!bootstrap_resolve("uquVH4-Du78#G#H", 0));
    assert(!bootstrap_resolve("6kPnj51T62Y#G#H", 0));
    assert(!bootstrap_resolve("U46NwOiJpys#G#H", 0));
    puts("PASS: headless VideoOut open guest ABI, 32-bit arguments, unsupported modes/index errors, repeated no-device responses, ignored options and unchanged buffers/errno");
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
     * stack and SysV call alignment preserved. The existing CPU host bridge
     * forwards that slot as the seventh native integer argument. */
    const unsigned char code[] = {0x68,0,0,0,0, 0x48,0xb8,0,0,0,0,0,0,0,0,
                                  0xff,0xd0, 0x48,0x83,0xc4,0x08, 0xc3};
    memcpy(p, code, sizeof(code)); memcpy(p + 1, &seventh, 4); memcpy(p + 7, &host, 8);
    bbcpu_add_guest_code((uintptr_t)p, sizeof(code)); return (uintptr_t)p;
}
static void check_video_attribute(uintptr_t function) {
    const uint32_t pitches[] = {0, 1, 1920, UINT32_MAX, 0x87654321};
    const uint32_t fields[][5] = {
        {0x80000000, 0, 0, 1920, 1080}, {0x80002200, 1, 0, 3840, 2160},
        {0xc1060000, 0, 0, 1280, 720}, {0, 0, 0, 0, 0},
        {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX}
    };
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    unsigned char *guarded = mmap(NULL, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(guarded != MAP_FAILED && mprotect(guarded + page, page, PROT_NONE) == 0);
    *runtime_errno() = 17;
    for (unsigned p = 0; p < 5; ++p) {
        const uintptr_t entry = guest_stack_argument(function, pitches[p]);
        for (unsigned f = 0; f < 5; ++f) for (unsigned offset = 0; offset < 2; ++offset) {
            unsigned char bytes[56]; memset(bytes, 0xa5, sizeof(bytes));
            uint64_t args[6] = {(uintptr_t)(bytes + 8 + offset), 0, 0, 0, 0, 0};
            for (unsigned i = 0; i < 5; ++i) args[i + 1] = UINT64_C(0xdeadbeef00000000) | fields[f][i];
            bbcpu_call(entry, 6, args);
            unsigned char expected[56]; memset(expected, 0xa5, sizeof(expected));
            uint32_t words[10] = {0}; memcpy(words, fields[f], sizeof(fields[f])); words[5] = pitches[p];
            memcpy(expected + 8 + offset, words, sizeof(words));
            assert(!memcmp(bytes, expected, sizeof(bytes)) && *runtime_errno() == 17);
            args[0] = (uintptr_t)(guarded + page - sizeof(words));
            bbcpu_call(entry, 6, args);
            assert(!memcmp(guarded + page - sizeof(words), words, sizeof(words)));
        }
    }
    assert(munmap(guarded, page * 2) == 0 && *runtime_errno() == 17);
}
static void check_video_buffers(uintptr_t entry) {
    const uint64_t handles[] = {0, 1, 3, 4, UINT32_MAX, UINT64_MAX, 0x80290018};
    const uint64_t counts[] = {0, 1, 2, 16, 17, UINT32_MAX, UINT64_MAX};
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    void *protected = mmap(NULL, page, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0); assert(protected != MAP_FAILED);
    const uint64_t pointers[] = {0, 1, (uintptr_t)protected};
    *runtime_errno() = 17;
    for (unsigned h = 0; h < 7; ++h) for (unsigned c = 0; c < 7; ++c)
        for (unsigned a = 0; a < 3; ++a) for (unsigned b = 0; b < 3; ++b) {
            const uint64_t args[] = {handles[h], counts[c], pointers[a], counts[c], pointers[b]};
            const uint32_t expected = !a || !b ? 0x80290002 : 0x8029000b;
            assert((uint32_t)bbcpu_call(entry, 5, args) == expected && *runtime_errno() == 17);
        }
    assert(munmap(protected, page) == 0);
}
static void test_video_buffers(void) {
    const char *name = "w3BY+tAEiQY#G#H";
    bootstrap_initialize(0); assert(!bootstrap_resolve(name, 0));
    bootstrap_initialize(1);
    const uintptr_t fn = bootstrap_resolve(name, 0); assert(fn && !bootstrap_resolve(name, 1));
    assert(!bootstrap_resolve("w3BY+tAEiQY#H#H", 0) && !bootstrap_resolve("w3BY+tAEiQY#G#G", 0));
    check_video_buffers(guest_bridge(fn));
    puts("PASS: headless VideoOut buffer registration guest ABI, null-address precedence, invalid handles/counts, protected pointers, scopes and errno");
}
static void check_video_mode(uintptr_t entry) {
    const uint32_t sizes[] = {0, 1, 3, 4, 16, 32, 48, 64};
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    unsigned char *guarded = mmap(NULL, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(guarded != MAP_FAILED && mprotect(guarded + page, page, PROT_NONE) == 0);
    *runtime_errno() = 17;
    for (unsigned z = 0; z < sizeof(sizes) / sizeof(*sizes); ++z) for (unsigned offset = 0; offset < 2; ++offset) {
        unsigned char data[80], expected[80]; memset(data, 0xa5, sizeof(data)); memset(expected, 0xa5, sizeof(expected));
        memset(expected + 8 + offset, 0xff, sizes[z]); memcpy(expected + 8 + offset, &sizes[z], 4);
        const uint64_t args[] = {(uintptr_t)(data + 8 + offset), UINT64_C(0xdeadbeef00000000) | sizes[z]};
        bbcpu_call(entry, 2, args); assert(!memcmp(data, expected, sizeof(data)) && *runtime_errno() == 17);
        const size_t extent = sizes[z] < 4 ? 4 : sizes[z];
        const uint64_t guarded_args[] = {(uintptr_t)(guarded + page - extent), sizes[z]};
        bbcpu_call(entry, 2, guarded_args);
        assert(!memcmp(guarded + page - extent, expected + 8 + offset, extent));
    }
    assert(munmap(guarded, page * 2) == 0);
}
static void check_video_configure(uintptr_t entry) {
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    void *protected = mmap(NULL, page, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0); assert(protected != MAP_FAILED);
    const uint64_t handles[] = {0, 1, 4, UINT32_MAX, 0x80290018};
    const uint64_t pointers[] = {0, 1, (uintptr_t)protected};
    const uint64_t sizes[] = {0, 1, 32, UINT32_MAX, UINT64_MAX};
    const uint64_t reserved[] = {0, 1, UINT32_MAX, UINT64_C(0x100000000)};
    *runtime_errno() = 17;
    for (unsigned h = 0; h < 5; ++h) for (unsigned p = 0; p < 3; ++p)
        for (unsigned z = 0; z < 5; ++z) for (unsigned r = 0; r < 4; ++r) {
            const uint64_t args[] = {handles[h], reserved[r], pointers[p], pointers[p], sizes[z], sizes[z]};
            const uint32_t expected = (uint32_t)reserved[r] || !p ? 0x80290001 : 0x8029000b;
            assert((uint32_t)bbcpu_call(entry, 6, args) == expected && *runtime_errno() == 17);
        }
    assert(munmap(protected, page) == 0);
}
static void test_video_modes(void) {
    const char *names[] = {"pjkDsgxli6c#G#H", "N1bEoJ4SRw4#G#H"};
    for (unsigned i = 0; i < 2; ++i) {
        bootstrap_initialize(0); assert(!bootstrap_resolve(names[i], 0)); bootstrap_initialize(1);
        uintptr_t fn = bootstrap_resolve(names[i], 0); assert(fn && !bootstrap_resolve(names[i], 1));
        if (i) check_video_configure(guest_bridge(fn)); else check_video_mode(guest_bridge(fn));
    }
    assert(!bootstrap_resolve("pjkDsgxli6c#H#H", 0) && !bootstrap_resolve("N1bEoJ4SRw4#G#G", 0));
    puts("PASS: VideoOut any-mode data helper, exact guarded size/fill incl short sizes, 32-bit widths; headless configure precedence, protected pointers, scopes and errno");
}
static void test_video_attribute(void) {
    const char *name = "i6-sR91Wt-4#G#H";
    bootstrap_initialize(0); assert(!bootstrap_resolve(name, 0));
    bootstrap_initialize(1);
    const uintptr_t fn = bootstrap_resolve(name, 0); assert(fn && !bootstrap_resolve(name, 1));
    assert(!bootstrap_resolve("i6-sR91Wt-4#H#H", 0) && !bootstrap_resolve("i6-sR91Wt-4#G#G", 0));
    check_video_attribute(fn);
    puts("PASS: VideoOut 40-byte buffer attributes, seven-argument guest ABI, stack pitch, exact guarded/unaligned output, 32-bit widths, scopes and errno");
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
static void check_unavailable_savedata_search(uintptr_t entry) {
    const size_t page=(size_t)sysconf(_SC_PAGESIZE);
    void *protected=mmap(NULL,page,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);assert(protected!=MAP_FAILED);
    unsigned char condition[128],result[256],before[256];
    memset(condition,0xa5,sizeof(condition));memset(result,0x5a,sizeof(result));memcpy(before,result,sizeof(result));
    const uint64_t inputs[]={0,1,UINT64_MAX,(uintptr_t)condition,(uintptr_t)protected};
    const uint64_t outputs[]={0,1,UINT64_MAX,(uintptr_t)result,(uintptr_t)protected};
    *runtime_errno()=17;
    for(unsigned round=0;round<3;++round) for(unsigned i=0;i<5;++i) for(unsigned j=0;j<5;++j) {
        const uint64_t args[]={inputs[i],outputs[j],UINT64_MAX,1,3,5};
        assert((uint32_t)bbcpu_call(entry,6,args)==0x809f0001);
        assert(!memcmp(result,before,sizeof(result)) && *runtime_errno()==17);
        for(size_t k=0;k<sizeof(condition);++k) assert(condition[k]==0xa5);
    }
    assert(!munmap(protected,page));
}
static void test_unavailable_savedata_search(void) {
    const char *name="dyIhnXq-0SM#u#d";
    bootstrap_initialize(0);assert(!bootstrap_resolve(name,0));bootstrap_initialize(1);
    const uintptr_t fn=bootstrap_resolve(name,0);assert(fn && !bootstrap_resolve(name,1));
    assert(!bootstrap_resolve("dyIhnXq-0SM#O#P",0) && !bootstrap_resolve("dyIhnXq-0SM#0#F",0));
    const uintptr_t entry=guest_bridge(fn);
    check_unavailable_savedata_search(entry);
    const uintptr_t init=guest_bridge(bootstrap_resolve("TywrFKCoLGY#u#d",0));
    const uint64_t args[]={0};assert((uint32_t)bbcpu_call(init,1,args)==0x809f000b);
    check_unavailable_savedata_search(entry); /* Failed init never creates a search context. */
    puts("PASS: SaveData directory-search two-pointer guest ABI, init-before-pointer checks, repeated NOT_INITIALIZED before/after failed init, protected/null inputs/outputs, unchanged buffers/errno and exact scopes/capability");
}
static void check_unavailable_savedata(uintptr_t entry) {
    const size_t page=(size_t)sysconf(_SC_PAGESIZE);
    void *protected=mmap(NULL,page,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);assert(protected!=MAP_FAILED);
    unsigned char param[64],before[64];memset(param,0xa5,sizeof(param));memcpy(before,param,sizeof(before));
    const uint64_t pointers[]={0,1,UINT64_MAX,(uintptr_t)param,(uintptr_t)protected};
    *runtime_errno()=17;
    for(unsigned round=0;round<3;++round) for(unsigned i=0;i<sizeof(pointers)/sizeof(*pointers);++i) {
        const uint64_t args[]={pointers[i],UINT64_MAX,1,3,5,7};
        assert((uint32_t)bbcpu_call(entry,6,args)==0x809f000b);
        assert(!memcmp(param,before,sizeof(param)) && *runtime_errno()==17);
    }
    assert(!munmap(protected,page));
}
static void test_unavailable_savedata(void) {
    const char *name="TywrFKCoLGY#u#d";
    bootstrap_initialize(0);assert(!bootstrap_resolve(name,0));bootstrap_initialize(1);
    const uintptr_t fn=bootstrap_resolve(name,0);assert(fn && !bootstrap_resolve(name,1));
    assert(!bootstrap_resolve("TywrFKCoLGY#V#W",0) && !bootstrap_resolve("TywrFKCoLGY#0#F",0));
    check_unavailable_savedata(guest_bridge(fn));
    puts("PASS: unavailable SaveData Initialize3 one-pointer guest ABI, repeated INTERNAL, opaque null/protected parameters, ignored extra registers, scopes/capability and unchanged inputs/errno");
}
static void check_unavailable_network_control(uintptr_t entry) {
    const uint64_t before[]={UINT64_C(0x123456789abcdef0),UINT64_C(0x23456789abcdef01)};
    uint64_t buffer[2];memcpy(buffer,before,sizeof(buffer));
    const uint64_t junk[]={1,UINT64_MAX,(uintptr_t)buffer,3,5,7};
    *runtime_errno()=17;
    for(unsigned round=0;round<3;++round) {
        assert((uint32_t)bbcpu_call(entry,0,NULL)==0x80412109);
        assert((uint32_t)bbcpu_call(entry,6,junk)==0x80412109);
        assert(!memcmp(buffer,before,sizeof(buffer)) && *runtime_errno()==17);
    }
}
static void test_unavailable_network_control(void) {
    const char *name="gky0+oaNM4k#D#E";
    bootstrap_initialize(0);assert(!bootstrap_resolve(name,0));bootstrap_initialize(1);
    const uintptr_t fn=bootstrap_resolve(name,0);assert(fn && !bootstrap_resolve(name,1));
    assert(!bootstrap_resolve("gky0+oaNM4k#V#W",0) && !bootstrap_resolve("gky0+oaNM4k#0#F",0));
    check_unavailable_network_control(guest_bridge(fn));
    assert(!bootstrap_resolve("Z4wwCFiBELQ#D#E",0)); /* No successful termination. */
    assert(!bootstrap_resolve("UJ+Z7Q+4ck0#D#E",0)); /* No callback registry. */
    puts("PASS: unavailable NetCtl no-argument guest ABI, repeated NOT_AVAIL, ignored stale registers, exact scopes/capability, unchanged buffers and errno");
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
static void check_unavailable_np_title(uintptr_t entry) {
    unsigned char title[16],secret[128],initial_title[16],initial_secret[128];
    memset(title,0xa5,sizeof(title));memset(secret,0x5a,sizeof(secret));
    memcpy(initial_title,title,sizeof(title));memcpy(initial_secret,secret,sizeof(secret));
    const uint64_t titles[]={0,1,(uintptr_t)title,UINT64_MAX};
    const uint64_t secrets[]={0,1,(uintptr_t)secret,UINT64_MAX};
    *runtime_errno()=17;
    for(unsigned repeat=0;repeat<2;++repeat) for(unsigned a=0;a<4;++a) for(unsigned b=0;b<4;++b) {
        const uint64_t args[]={titles[a],secrets[b],UINT64_MAX,3,5,7};
        assert((uint32_t)bbcpu_call(entry,6,args)==0x80550002);
        assert(!memcmp(title,initial_title,sizeof(title)) && !memcmp(secret,initial_secret,sizeof(secret)));
        assert(*runtime_errno()==17);
    }
}
static void test_unavailable_np_title(void) {
    const char *name="Ec63y59l9tw#S#T";
    bootstrap_initialize(0);assert(!bootstrap_resolve(name,0));
    bootstrap_initialize(1);
    const uintptr_t fn=bootstrap_resolve(name,0);assert(fn && !bootstrap_resolve(name,1));
    assert(!bootstrap_resolve("Ec63y59l9tw#R#S",0) && !bootstrap_resolve("Ec63y59l9tw#0#F",0));
    assert(!bootstrap_resolve("eQH7nWPcAgc#S#T",0)); /* No fabricated NP user state. */
    check_unavailable_np_title(guest_bridge(fn));
    puts("PASS: NP title two-pointer guest ABI, initialization error before pointer validation, repeated calls, scopes/capability and untouched inputs/errno");
}
static void check_unavailable_np_restriction(uintptr_t entry) {
    unsigned char restriction[24],before[24];memset(restriction,0xa5,sizeof(restriction));
    memcpy(before,restriction,sizeof(before));
    const uint64_t pointers[]={0,1,(uintptr_t)restriction,UINT64_MAX};
    *runtime_errno()=17;
    for(unsigned repeat=0;repeat<3;++repeat) for(unsigned i=0;i<4;++i) {
        const uint64_t args[]={pointers[i],UINT64_MAX,1,3,5,7};
        assert((uint32_t)bbcpu_call(entry,6,args)==0x80550002);
        assert(!memcmp(restriction,before,sizeof(before)) && *runtime_errno()==17);
    }
}
static void test_unavailable_np_restriction(void) {
    const char *name="A2CQ3kgSopQ#S#T";
    bootstrap_initialize(0);assert(!bootstrap_resolve(name,0));bootstrap_initialize(1);
    const uintptr_t fn=bootstrap_resolve(name,0);assert(fn && !bootstrap_resolve(name,1));
    assert(!bootstrap_resolve("A2CQ3kgSopQ#R#S",0) && !bootstrap_resolve("A2CQ3kgSopQ#0#F",0));
    check_unavailable_np_restriction(guest_bridge(fn));
    puts("PASS: NP content-restriction one-pointer guest ABI, init-before-structure/size validation, repeated errors, scopes and unchanged inputs/errno");
}
static void check_system_status(uintptr_t entry) {
    const size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *guard=mmap(NULL,page*3,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);
    assert(guard!=MAP_FAILED && !mprotect(guard+page,page,PROT_READ|PROT_WRITE));
    *runtime_errno()=17;
    const size_t offsets[]={page+1,page*2-136};
    for(unsigned round=0;round<3;++round) for(unsigned slot=0;slot<2;++slot) {
        memset(guard+page,0xa5,page);
        const uint64_t args[]={(uintptr_t)(guard+offsets[slot]),UINT64_MAX,1,3,5,7};
        assert((uint32_t)bbcpu_call(entry,6,args)==0);
        for(size_t i=page;i<page*2;++i)
            assert(guard[i]==(i>=offsets[slot] && i<offsets[slot]+136 ? 0 : 0xa5));
        assert(*runtime_errno()==17);
    }
    assert(!munmap(guard,page*3));
}
static void test_system_status(void) {
    const char *name="rPo6tV8D9bM#a#a";
    bootstrap_initialize(0);assert(!bootstrap_resolve(name,0));bootstrap_initialize(1);
    const uintptr_t fn=bootstrap_resolve(name,0);assert(fn && !bootstrap_resolve(name,1));
    assert(!bootstrap_resolve("rPo6tV8D9bM#V#W",0) && !bootstrap_resolve("rPo6tV8D9bM#0#F",0));
    const uintptr_t entry=guest_bridge(fn);
    const uint64_t args[]={0};
    *runtime_errno()=17;
    assert((uint32_t)bbcpu_call(entry,1,args)==0x80a10003 && *runtime_errno()==17);
    check_system_status(entry);
    puts("PASS: standalone system-status one-pointer guest ABI, exact 136-byte profile/reserved output, unaligned/guarded boundaries, null error, scopes and errno");
}
static void check_system_language(uintptr_t entry) {
    unsigned char buffer[12], before[12];
    memset(before,0xa5,sizeof(before));
    *runtime_errno()=17;
    for(unsigned round=0;round<3;++round) {
        memcpy(buffer,before,sizeof(buffer));
        const uint64_t args[]={UINT64_C(0xabcdef0000000001),(uintptr_t)(buffer+3)};
        assert((uint32_t)bbcpu_call(entry,2,args)==0);
        int32_t value;memcpy(&value,buffer+3,sizeof(value));assert(value==1);
        assert(!memcmp(buffer,before,3) && !memcmp(buffer+7,before+7,5));
        const uint64_t null_args[]={1,0};
        assert((uint32_t)bbcpu_call(entry,2,null_args)==0x80a10003);
        const uint32_t rejected[]={0,2,3,4,5,7,1000,UINT32_MAX};
        for(unsigned i=0;i<sizeof(rejected)/sizeof(rejected[0]);++i) {
            const uint64_t bad[]={rejected[i],1};
            assert((uint32_t)bbcpu_call(entry,2,bad)==0x80a10003);
        }
        assert(*runtime_errno()==17);
    }
}
static void test_system_language(void) {
    const char *name="fZo48un7LK4#a#a";
    bootstrap_initialize(0);assert(!bootstrap_resolve(name,0));bootstrap_initialize(1);
    const uintptr_t fn=bootstrap_resolve(name,0);assert(fn && !bootstrap_resolve(name,1));
    assert(!bootstrap_resolve("fZo48un7LK4#V#W",0) && !bootstrap_resolve("fZo48un7LK4#0#F",0));
    check_system_language(guest_bridge(fn));
    puts("PASS: local English-US system-language profile, two-argument guest ABI, 32-bit ID, guarded unaligned output, null/unsupported errors, scopes and errno");
}
static void check_unavailable_splash(uintptr_t entry) {
    const uint64_t initial[]={UINT64_C(0x123456789abcdef0),UINT64_C(0x23456789abcdef01)};
    uint64_t buffer[2];memcpy(buffer,initial,sizeof(buffer));
    const uint64_t junk[]={1,UINT64_MAX,(uintptr_t)buffer,3,5,7};
    *runtime_errno()=17;
    for(unsigned i=0;i<3;++i) {
        assert((uint32_t)bbcpu_call(entry,0,NULL)==0x80e30002);
        assert((uint32_t)bbcpu_call(entry,6,junk)==0x80e30002);
        assert(!memcmp(buffer,initial,sizeof(buffer)) && *runtime_errno()==17);
    }
}
static void test_unavailable_splash(void) {
    const char *name="Vo5V8KAwCmk#a#a";
    bootstrap_initialize(0);assert(!bootstrap_resolve(name,0));
    bootstrap_initialize(1);
    const uintptr_t fn=bootstrap_resolve(name,0);assert(fn && !bootstrap_resolve(name,1));
    assert(!bootstrap_resolve("Vo5V8KAwCmk#V#W",0) && !bootstrap_resolve("Vo5V8KAwCmk#0#F",0));
    check_unavailable_splash(guest_bridge(fn));
    puts("PASS: splash-hide no-argument guest ABI, absent launch-control error, repeated calls, exact scope/capability and unchanged buffers/errno");
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
    test_video_capabilities();
    test_video_attribute();
    test_video_buffers();
    test_video_modes();
    test_video_flip_event();
    test_video_flip_rate();
    test_video_adjust_color();
    test_video_gamma();
    test_unavailable_video();
    test_display_safe_area();
    test_unavailable_gnm();
    test_unavailable_compute_queue();
    test_unavailable_share();
    test_unavailable_network();
    test_unavailable_network_control();
    test_unavailable_savedata();
    test_unavailable_savedata_search();
    test_unavailable_np_title();
    test_unavailable_np_restriction();
    test_unavailable_dialogs();
    test_unavailable_splash();
    test_system_language();
    test_system_status();
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
