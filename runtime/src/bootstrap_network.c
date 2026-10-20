/* Explicit network unavailability for the CPU-only diagnostic.
 * No-argument sceNetInit ABI and network error constants follow shadPS4
 * net.h/net_error.h (GPL-2.0-or-later). Foundations: deadinside28/bbport,
 * bloodborne-native and shadPS4. This does not initialize a network backend.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic size_t initialization_requests;
static int32_t network_initialize(void) {
    atomic_fetch_add_explicit(&initialization_requests, 1, memory_order_relaxed);
    /* Diagnostic policy: the network operation is not supported. This is an
     * encoded libSceNet error, not a POSIX -1/errno result or a connection state.
     * No sockets, pools, module references or initialized state are created. */
    return (int32_t)UINT32_C(0x8041012d); /* EOPNOTSUPP */
}
uintptr_t bootstrap_network_resolve(const char *name) {
    if (!strcmp(name, "Nlev7Lg8k3A#Bc#D")) return (uintptr_t)network_initialize;
    return 0;
}
void bootstrap_network_report(void) {
    printf("Bootstrap: unavailable network initialization requests=%zu\n",
           atomic_load_explicit(&initialization_requests, memory_order_relaxed));
}
