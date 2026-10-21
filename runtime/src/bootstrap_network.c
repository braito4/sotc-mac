/* Explicit network and NP unavailability for the diagnostic.
 * No-argument sceNetInit ABI and network error constants follow shadPS4
 * net.h/net_error.h and NetCtl ABI/error declarations (GPL-2.0-or-later). Foundations: deadinside28/bbport,
 * bloodborne-native and shadPS4. NP initialization ordering also follows the
 * supplied original manager. This does not initialize a network/NP backend.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic size_t initialization_requests;
static _Atomic size_t control_initialization_requests;
static int32_t network_control_initialize(void) {
    atomic_fetch_add_explicit(&control_initialization_requests,1,memory_order_relaxed);
    /* No PS4 network-control IPC service exists in this diagnostic.
     * Return NetCtl NOT_AVAIL, without creating a service/client, callback
     * registry or apparent connection state. This API has no arguments. */
    return (int32_t)UINT32_C(0x80412109);
}
static _Atomic size_t np_title_requests;
static _Atomic size_t np_restriction_requests;
static int32_t np_set_restriction(const void *restriction) {
    (void)restriction;
    atomic_fetch_add_explicit(&np_restriction_requests,1,memory_order_relaxed);
    /* Retail checks NP initialization before the structure or its size. */
    return (int32_t)UINT32_C(0x80550002);
}
static int32_t np_set_title(const void *title_id,const void *title_secret) {
    (void)title_id;(void)title_secret;
    atomic_fetch_add_explicit(&np_title_requests,1,memory_order_relaxed);
    /* The supplied NP manager checks initialization before either pointer.
     * No NP backend is initialized here: never inspect, copy or log the title
     * secret, create a local registration or imply an authenticated session. */
    return (int32_t)UINT32_C(0x80550002); /* NP NOT_INITIALIZED. */
}
static int32_t network_initialize(void) {
    atomic_fetch_add_explicit(&initialization_requests, 1, memory_order_relaxed);
    /* Diagnostic policy: the network operation is not supported. This is an
     * encoded libSceNet error, not a POSIX -1/errno result or a connection state.
     * No sockets, pools, module references or initialized state are created. */
    return (int32_t)UINT32_C(0x8041012d); /* EOPNOTSUPP */
}
uintptr_t bootstrap_network_resolve(const char *name) {
    if (!strcmp(name, "gky0+oaNM4k#D#E")) return (uintptr_t)network_control_initialize;
    if (!strcmp(name, "Nlev7Lg8k3A#Bc#D")) return (uintptr_t)network_initialize;
    if (!strcmp(name, "Ec63y59l9tw#S#T")) return (uintptr_t)np_set_title;
    if (!strcmp(name, "A2CQ3kgSopQ#S#T")) return (uintptr_t)np_set_restriction;
    return 0;
}
void bootstrap_network_report(void) {
    printf("Bootstrap: unavailable network-control initialization requests=%zu (no PS4 network-control service)\n",
           atomic_load_explicit(&control_initialization_requests,memory_order_relaxed));
    printf("Bootstrap: unavailable network initialization requests=%zu\n",
           atomic_load_explicit(&initialization_requests, memory_order_relaxed));
    printf("Bootstrap: unavailable NP title registrations=%zu (NP not initialized; no title data read)\n",
           atomic_load_explicit(&np_title_requests,memory_order_relaxed));
    printf("Bootstrap: unavailable NP content-restriction requests=%zu (NP not initialized)\n",
           atomic_load_explicit(&np_restriction_requests,memory_order_relaxed));
}
