/* Narrow libSceSysmodule registry for the CPU diagnostic. Module identifiers
 * follow shadPS4 and the SotC reference port; only an implemented library can
 * be loaded. Missing services remain unavailable instead of returning success.
 * Foundations: deadinside28/bbport and shadPS4 (GPL-2.0-or-later).
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#define INVALID_ID ((int32_t)UINT32_C(0x805a1000))
#define NOT_LOADED ((int32_t)UINT32_C(0x805a1001))
static _Atomic unsigned fiber_refs;
static int32_t module_load(uint16_t id) {
    if (!id) return INVALID_ID;
    if (id != 6) {
        fprintf(stderr, "Bootstrap: system module %#x is unavailable in this diagnostic\n", id);
        return NOT_LOADED;
    }
    atomic_fetch_add(&fiber_refs, 1);
    return 0;
}
static int32_t module_loaded(uint16_t id) {
    if (!id) return INVALID_ID;
    return id == 6 && atomic_load(&fiber_refs) ? 0 : NOT_LOADED;
}
static int32_t module_unload(uint16_t id) {
    if (!id) return INVALID_ID;
    if (id != 6) return NOT_LOADED;
    unsigned count = atomic_load(&fiber_refs);
    while (count)
        if (atomic_compare_exchange_weak(&fiber_refs, &count, count - 1)) return 0;
    return NOT_LOADED;
}
uintptr_t bootstrap_modules_resolve(const char *name) {
    if (!strcmp(name, "g8cM39EUZ6o#F#G")) return (uintptr_t)module_load;
    if (!strcmp(name, "fMP5NHUOaMk#F#G")) return (uintptr_t)module_loaded;
    if (!strcmp(name, "eR2bZFAAU0Q#F#G")) return (uintptr_t)module_unload;
    return 0;
}
