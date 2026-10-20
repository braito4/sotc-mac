/* Original synthetic renderer double; exercise retail-to-HLE ABI conversion. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
static int open_now, adjust_calls, query_calls, native_result, status_result;
static uint32_t gamma_bits;
static int32_t native_open(int32_t user, int32_t bus, int32_t index, const void *p) {
    assert(user == 255 && !bus && !index && !p);
    if (open_now) return -77;
    open_now = 1;
    return 37; /* Test backend's handle; the adapter must preserve it. */
}
static int32_t native_close(int32_t handle) { assert(handle == 37); open_now = 0; return 0; }
static int32_t native_status(int32_t handle, void *out) {
    assert(handle == 37 && open_now); memset(out, 0, 64); return status_result;
}
static int32_t native_color(int32_t handle, const void *settings) {
    assert(handle == 37 && open_now); ++adjust_calls;
    if (!settings) return (int32_t)UINT32_C(0x80290002);
    uint32_t data[4]; memcpy(data, settings, sizeof(data));
    assert(!data[1] && !data[2] && !data[3]); gamma_bits = data[0];
    return native_result;
}
static int32_t native_capability(int32_t handle, void *out) {
    assert(handle == 37 && open_now); ++query_calls;
    const uint64_t caps = UINT64_C(0x123456789abcdef0);
    memcpy(out, &caps, sizeof(caps)); return native_result;
}
static void unused(void) {}
static uintptr_t resolver(const char *name) {
    if (!strcmp(name, "Up36PTk687E#G#H")) return (uintptr_t)native_open;
    if (!strcmp(name, "uquVH4-Du78#G#H")) return (uintptr_t)native_close;
    if (!strcmp(name, "SbU3dwp80lQ#G#H")) return (uintptr_t)native_status;
    if (!strcmp(name, "pv9CI5VC+R0#G#H")) return (uintptr_t)native_color;
    if (!strcmp(name, "kGVLc3htQE8#G#H")) return (uintptr_t)native_capability;
    return (uintptr_t)unused;
}
static uintptr_t bridge(const char *name) {
    const uintptr_t target = bootstrap_resolve(name, 0); assert(target);
    unsigned char *code = runtime_low_map(16384, PROT_READ | PROT_WRITE); assert(code);
    code[0] = 0x48; code[1] = 0xb8; memcpy(code+2, &target, 8);
    code[10] = 0xff; code[11] = 0xe0;
    bbcpu_add_guest_code((uintptr_t)code, 16384); return (uintptr_t)code;
}
static uint32_t call(uintptr_t entry, uint64_t handle, const void *data, uint64_t size) {
    const uint64_t args[3] = {handle, (uintptr_t)data, size};
    return (uint32_t)bbcpu_call(entry, 3, args);
}
int main(int argc, char **argv) {
    bootstrap_initialize(1); assert(bootstrap_graphics_bind(resolver) == 0);
    const uintptr_t color = bridge("pv9CI5VC+R0#G#H"), cap = bridge("kGVLc3htQE8#G#H");
    const uintptr_t open = bridge("Up36PTk687E#G#H"), close = bridge("uquVH4-Du78#G#H");
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    void *protected = mmap(NULL, page, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0); assert(protected != MAP_FAILED);
    /* Even the HLE's usual handle value is invalid until an actual open. */
    assert(call(color, 37, protected, 16) == UINT32_C(0x8029000b));
    assert(call(cap, 37, protected, 8) == UINT32_C(0x8029000b));
    const uint64_t open_args[4] = {255, 0, 0, 0};
    assert(bbcpu_call(open, 4, open_args) == 37);
    assert((int32_t)bbcpu_call(open, 4, open_args) == -77); /* Failed open retains the live handle. */
    if (argc == 2) {
        uint32_t channels[4] = {0x3f800000, 0x3f800000, 0x3f800000, 0};
        uint32_t size = 16;
        if (!strcmp(argv[1], "--unequal")) channels[1] = 0x40000000;
        else if (!strcmp(argv[1], "--reserved")) channels[3] = 1;
        else if (!strcmp(argv[1], "--size")) size = 13;
        else assert(0);
        call(color, 37, channels, size); assert(!"Unsupported profile must stop");
    }
    assert(!bootstrap_resolve("pv9CI5VC+R0#A#B", 0));
    assert(!bootstrap_resolve("kGVLc3htQE8#A#B", 0));
    const uint32_t bits[] = {0x3f000000, 0x3f800000, 0x40000000, 0x7fc12345};
    for (unsigned b = 0; b < sizeof(bits)/sizeof(*bits); ++b) for (unsigned size = 12; size <= 16; size += 4) {
        unsigned char settings[32], saved[32]; memset(settings, 0xa5, sizeof(settings));
        const uint32_t channels[4] = {bits[b], bits[b], bits[b], 0};
        memcpy(settings+1, channels, size); memcpy(saved, settings, sizeof(saved));
        assert(call(color, 37, settings+1, size) == 0 && gamma_bits == bits[b]);
        assert(!memcmp(settings, saved, sizeof(settings)));
        assert(call(color, 37, settings+1, (UINT64_C(1)<<32) | size) == 0); /* Retail color size is 32-bit. */
    }
    assert(call(color, 37, NULL, 16) == UINT32_C(0x80290002));
    native_result = -23;
    uint32_t channels[4] = {0x3f800000, 0x3f800000, 0x3f800000, 0};
    assert((int32_t)call(color, 37, channels, 16) == -23);
    unsigned char output[24], saved[24]; memset(output, 0xa5, sizeof(output)); memcpy(saved, output, sizeof(saved));
    assert((int32_t)call(cap, 37, output+1, 8) == -23 && !memcmp(output, saved, sizeof(output)));
    native_result = 0;
    assert(call(cap, 37, output+1, 8) == 0);
    const uint64_t expected = UINT64_C(0x123456789abcdef0);
    memcpy(saved+1, &expected, 8); assert(!memcmp(output, saved, sizeof(output)));
    const int previous_queries = query_calls;
    assert(call(cap, 37, protected, (UINT64_C(1)<<32) | 8) == UINT32_C(0x80290001));
    assert(call(cap, 37, protected, 16) == UINT32_C(0x80290001));
    assert(call(cap, 37, NULL, 8) == UINT32_C(0x80290001) && query_calls == previous_queries);
    status_result = -55;
    assert((int32_t)call(color, 37, protected, 16) == -55);
    assert((int32_t)call(cap, 37, protected, 8) == -55);
    status_result = 0;
    unsigned char *boundary = mmap(NULL, page*2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(boundary != MAP_FAILED && mprotect(boundary+page, page, PROT_NONE) == 0);
    memcpy(boundary+page-12, channels, 12);
    assert(call(color, 37, boundary+page-12, 12) == 0);
    assert(munmap(boundary, page*2) == 0);
    const uint64_t close_args[1] = {37}; assert(bbcpu_call(close, 1, close_args) == 0);
    const int previous_adjusts = adjust_calls;
    assert(call(color, 37, protected, 16) == UINT32_C(0x8029000b));
    assert(call(cap, 37, protected, 8) == UINT32_C(0x8029000b));
    assert(adjust_calls == previous_adjusts && query_calls == previous_queries);
    assert(munmap(protected, page) == 0);
    puts("PASS: guest RGB-to-scalar gamma, 32/64-bit sizes, actual capability bits, guarded outputs, real backend errors and live display lifecycle");
}
