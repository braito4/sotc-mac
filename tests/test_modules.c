/* Synthetic x86 calls to the module loader and absent optical metadata profile.
 * No game input and no assumed physical-disc placement. */
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
