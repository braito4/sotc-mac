/* Fixed system-display metadata for the CPU diagnostic.
 * ABI/error reference: shadPS4 systemservice.h/.cpp/systemservice_error.h;
 * SotC reference foundations: AjncSystem, deadinside28/bbport and
 * bloodborne-native (GPL-2.0-or-later). No host display or renderer is used.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic size_t safe_area_requests;
static int32_t display_safe_area(void *info) {
    atomic_fetch_add_explicit(&safe_area_requests, 1, memory_order_relaxed);
    if (!info) return (int32_t)UINT32_C(0x80a10003); /* PARAMETER */
    /* Diagnostic profile: the whole logical viewport is safe. The ABI is
     * float ratio followed by 128 reserved bytes. Like the original module,
     * write only the ratio; do not initialize or read the reserved storage. */
    const float ratio = 1.0f;
    memcpy(info, &ratio, sizeof(ratio));
    return 0;
}
uintptr_t bootstrap_system_resolve(const char *name) {
    if (!strcmp(name, "1n37q1Bvc5Y#a#a")) return (uintptr_t)display_safe_area;
    return 0;
}
void bootstrap_system_report(void) {
    printf("Bootstrap: logical display safe-area queries=%zu (ratio=1.0; no host display)\n",
           atomic_load_explicit(&safe_area_requests, memory_order_relaxed));
}
