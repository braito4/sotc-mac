/* Narrow libSceSysmodule registry for the CPU diagnostic. Module identifiers
 * follow shadPS4 and the SotC reference port; only an implemented library can
 * be loaded. Missing services remain unavailable instead of returning success.
 * Foundations: deadinside28/bbport and shadPS4 (GPL-2.0-or-later).
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define INVALID_ID ((int32_t)UINT32_C(0x805a1000))
#define NOT_LOADED ((int32_t)UINT32_C(0x805a1001))
static _Atomic unsigned fiber_refs, discmap_refs;
static _Atomic unsigned *references(uint32_t id) {
    return id == 6 ? &fiber_refs : id == 0xd7 ? &discmap_refs : NULL;
}
static int32_t load(uint32_t id) {
    if (!(id & UINT32_C(0x7fffffff))) return INVALID_ID;
    _Atomic unsigned *refs = references(id);
    if (!refs || (id == 0xd7 && !bootstrap_files_mounted())) {
        fprintf(stderr, "Bootstrap: system module %#x is unavailable in this diagnostic\n", id);
        return NOT_LOADED;
    }
    unsigned count = atomic_load(refs);
    do {
        if (count == UINT_MAX) { fputs("STOP: system-module reference overflow\n", stderr); _Exit(23); }
    } while (!atomic_compare_exchange_weak(refs, &count, count + 1));
    if (id == 0xd7 && !count)
        puts("Bootstrap: DiscMap provider loaded; local content has no optical bitmap metadata");
    return 0;
}
static int32_t loaded(uint32_t id) {
    if (!(id & UINT32_C(0x7fffffff))) return INVALID_ID;
    _Atomic unsigned *refs = references(id);
    return refs && atomic_load(refs) ? 0 : NOT_LOADED;
}
static int32_t module_load(uint16_t id) { return load(id); }
static int32_t module_loaded(uint16_t id) { return loaded(id); }
static int32_t module_unload(uint16_t id) {
    if (!id) return INVALID_ID;
    _Atomic unsigned *refs = references(id);
    if (!refs) return NOT_LOADED;
    unsigned count = atomic_load(refs);
    while (count)
        if (atomic_compare_exchange_weak(refs, &count, count - 1)) return 0;
    return NOT_LOADED;
}
/* PS4 internal-with-argument ABI has five arguments: u32 id, s32 byte count,
 * argument data, a reserved u64, and an s32 initializer-result output. Only
 * the empty argument profile is supported by these static host providers.
 * A failed load leaves the initializer output untouched. */
static int32_t load_internal_with_arg(uint32_t id, int32_t bytes, const void *args,
                                      uint64_t reserved, int32_t *initializer_result) {
    if (!(id & UINT32_C(0x7fffffff)) || reserved) return INVALID_ID;
    if (bytes || args) {
        fprintf(stderr, "Bootstrap: system module %#x initialization arguments are unsupported\n", id);
        return NOT_LOADED;
    }
    const int32_t result = load(id);
    if (!result && initializer_result) *initializer_result = 0;
    return result;
}
uintptr_t bootstrap_modules_resolve(const char *name) {
    if (!strcmp(name, "g8cM39EUZ6o#F#G")) return (uintptr_t)module_load;
    if (!strcmp(name, "fMP5NHUOaMk#F#G")) return (uintptr_t)module_loaded;
    if (!strcmp(name, "eR2bZFAAU0Q#F#G")) return (uintptr_t)module_unload;
    if (!strcmp(name, "39iV5E1HoCk#F#G")) return (uintptr_t)load;
    if (!strcmp(name, "ynFKQ5bfGks#F#G")) return (uintptr_t)loaded;
    if (!strcmp(name, "hHrGoGoNf+s#F#G")) return (uintptr_t)load_internal_with_arg;
    return 0;
}
