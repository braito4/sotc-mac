/* Narrow ShareUtility failure profile for the CPU diagnostic.
 * ABI and early error order checked against the supplied original module.
 * SotC reference foundations: AjncSystem, deadinside28/bbport,
 * bloodborne-native and shadPS4 (GPL-2.0-or-later).
 * No capture, sharing UI, service allocation or initialized state is provided.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic size_t initialization_requests;
static int32_t share_initialize(uint64_t mode, uint64_t heap_size,
                                uint32_t priority, const void *thread_attributes) {
    atomic_fetch_add_explicit(&initialization_requests, 1, memory_order_relaxed);
    /* Ex2 accepts mode 1 and at least 128 KiB before touching any attributes.
     * The original performs service allocation before validating the final
     * thread arguments. This diagnostic fails at that earlier resource stage;
     * it must not dereference attributes or claim successful initialization. */
    (void)priority;
    (void)thread_attributes;
    if (mode != 1 || heap_size < UINT64_C(0x20000))
        return (int32_t)UINT32_C(0x81170003);
    return (int32_t)UINT32_C(0x81170002); /* Service resource unavailable. */
}
uintptr_t bootstrap_share_resolve(const char *name) {
    if (!strcmp(name, "j7DlalBzHh8#o#o")) return (uintptr_t)share_initialize;
    return 0;
}
void bootstrap_share_report(void) {
    printf("Bootstrap: unavailable share initialization requests=%zu\n",
           atomic_load_explicit(&initialization_requests, memory_order_relaxed));
}
