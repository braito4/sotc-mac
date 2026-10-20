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
int main(void) {
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
    assert(bootstrap_mount_game_root(root) == 0);
    assert(load(entry, 0xd7, 0, NULL, UINT64_C(1) << 40, &out.result) == 0x805a1000);
    assert(load(entry, 0x100d7, 0, NULL, 0, &out.result) == 0x805a1001);
    assert(load(entry, 0x80000006, 0, NULL, 0, &out.result) == 0x805a1001);
    assert(load(entry, 0xd7, -1, (void *)(uintptr_t)1, 0, &out.result) == 0x805a1001);
    assert((uint32_t)loaded(0xd7) == 0x805a1001 && out.result == 123);
    assert(load(entry, 0xd7, 0, NULL, 0, &out.result) == 0 && out.result == 0);
    assert(out.head == 0x12345678 && out.tail == 0x87654321 && loaded(0xd7) == 0);
    assert(plain_load(0xd7) == 0 && unload(0xd7) == 0 && loaded(0xd7) == 0);
    assert(!bootstrap_resolve("fJgP+wqifno#libSceDiscMap", 0)); /* Opaque query stays unresolved. */
    const uintptr_t request = bootstrap_resolve("lbQKqsERhtE#libSceDiscMap", 0);
    const uintptr_t package = bootstrap_resolve("fl1eoDnwQ4s#libSceDiscMap", 0);
    const uintptr_t query = bootstrap_resolve("ioKMruft1ek#libSceDiscMap", 0);
    assert(request && package && query);
    assert(!bootstrap_resolve("lbQKqsERhtE#0#F", 0) && !bootstrap_resolve("lbQKqsERhtE#libSceDiscMap", 1));
    int32_t values[] = {101, 202, 303};
    const uint64_t request_args[] = {(uintptr_t)"/app0/not-a-disc.bin", 17, 4096, (uintptr_t)&values[0]};
    assert((uint32_t)bbcpu_call(guest_bridge(request), 4, request_args) == 0x81100004 && values[0] == 101);
    const uint64_t package_args[] = {0, (uintptr_t)&values[1], (uintptr_t)&values[2]};
    assert((uint32_t)bbcpu_call(guest_bridge(package), 3, package_args) == 0x81100004);
    const uint64_t query_args[] = {0, 0, 0, (uintptr_t)&values[0], (uintptr_t)&values[1], (uintptr_t)&values[2]};
    assert((uint32_t)bbcpu_call(guest_bridge(query), 6, query_args) == 0x81100004);
    assert(values[0] == 101 && values[1] == 202 && values[2] == 303);
    assert(unload(0xd7) == 0 && (uint32_t)loaded(0xd7) == 0x805a1001);
    assert((uint32_t)unload(0xd7) == 0x805a1001);
    out.result = 321;
    assert(load(entry, 0xad, 0, NULL, 0, &out.result) == 0x805a1001 && out.result == 321);
    bootstrap_unmount_game_root(); assert(rmdir(root) == 0);
    puts("PASS: five-argument internal module-load ABI, u32 IDs/reserved fields, reference counts, unavailable modules and DiscMap no-bitmap errors with untouched outputs");
}
