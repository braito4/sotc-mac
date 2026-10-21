/* Explicit savedata unavailability for the graphics diagnostic.
 * One-pointer Initialize3 ABI checked against the supplied retail export;
 * INTERNAL error from shadPS4 save_data/savedata_error.h (GPL-2.0-or-later).
 * Foundations: AjncSystem, deadinside28/bbport, bloodborne-native and shadPS4.
 * No save paths, accounts, mounts, files or persistent state are accessed.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic size_t initialization_requests;
static int32_t save_initialize3(const void *param) {
    (void)param;
    atomic_fetch_add_explicit(&initialization_requests,1,memory_order_relaxed);
    /* Diagnostic policy: there is no savedata backend to initialize. The
     * optional parameter remains opaque; do not inspect it, register a save
     * context, create a substitute filesystem or claim initialized state. */
    return (int32_t)UINT32_C(0x809f000b); /* SaveData INTERNAL. */
}
uintptr_t bootstrap_savedata_resolve(const char *name) {
    return !strcmp(name,"TywrFKCoLGY#u#d") ? (uintptr_t)save_initialize3 : 0;
}
void bootstrap_savedata_report(void) {
    printf("Bootstrap: unavailable SaveData Initialize3 requests=%zu (no save backend; no save access)\n",
           atomic_load_explicit(&initialization_requests,memory_order_relaxed));
}
