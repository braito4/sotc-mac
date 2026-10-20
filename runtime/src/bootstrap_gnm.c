/* Narrow GNM failure profiles; no graphics backend.
 * shadPS4 gnmdriver.cpp/gnm_error.h and the supplied retail module agree on
 * the fixed error returned by these unavailable diagnostic-registration APIs.
 * Foundations: shadPS4, bloodborne-native, deadinside28/bbport and AjncSystem
 * (GPL-2.0-or-later). No owner/resource handles or graphics state are created.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic size_t owner_requests, resource_requests, compute_requests;
/* These are argument-independent error providers, not working registration
 * APIs. Like the retail exports, they never read argument registers, stack
 * arguments, names or output handles. In particular, no seventh resource
 * argument needs to be forwarded through the host bridge. */
static int32_t owner_unavailable(void) {
    atomic_fetch_add_explicit(&owner_requests, 1, memory_order_relaxed);
    return (int32_t)UINT32_C(0x8eee00ff); /* ORBIS_GNM_ERROR_FAILURE */
}
static int32_t resource_unavailable(void) {
    atomic_fetch_add_explicit(&resource_requests, 1, memory_order_relaxed);
    return (int32_t)UINT32_C(0x8eee00ff);
}
static int32_t compute_queue_unavailable(uint32_t pipe, uint32_t queue, uint64_t ring,
                                         uint32_t size_dw, const void *read_pointer) {
    atomic_fetch_add_explicit(&compute_requests, 1, memory_order_relaxed);
    /* Preserve the supplied retail module's validation order and widths.
     * Its clear-lowest-bit test accepts zero as well as powers of two; zero
     * reaches the unavailable backend. Alignment checks do not dereference
     * either address, and no queue/read-pointer state is created on failure. */
    if (pipe > 6) return (int32_t)UINT32_C(0x80d17000);
    if (queue > 7) return (int32_t)UINT32_C(0x80d17001);
    if (size_dw & (size_dw - 1)) return (int32_t)UINT32_C(0x80d17002);
    if (ring & 255) return (int32_t)UINT32_C(0x80d17003);
    if ((uintptr_t)read_pointer & 3) return (int32_t)UINT32_C(0x80d17004);
    return (int32_t)UINT32_C(0x80d170ff); /* COMPUTEQUEUE_INTERNAL: no backend. */
}
uintptr_t bootstrap_gnm_resolve(const char *name) {
    if (!strcmp(name, "ZFqKFl23aMc#A#B")) return (uintptr_t)owner_unavailable;
    if (!strcmp(name, "nvEwfYAImTs#A#B")) return (uintptr_t)resource_unavailable;
    if (!strcmp(name, "29oKvKXzEZo#A#B")) return (uintptr_t)compute_queue_unavailable;
    return 0;
}
void bootstrap_gnm_report(void) {
    printf("Bootstrap: unavailable retail GNM registrations: owners=%zu, resources=%zu\n",
           atomic_load_explicit(&owner_requests, memory_order_relaxed),
           atomic_load_explicit(&resource_requests, memory_order_relaxed));
    printf("Bootstrap: unavailable GNM compute queue requests=%zu\n",
           atomic_load_explicit(&compute_requests, memory_order_relaxed));
}
