/* Uninitialized trophy-service paths checked against supplied retail exports.
 * Typed ABI also follows shadPS4 np_trophy.h (GPL-2.0-or-later).
 * Foundations: AjncSystem, deadinside28/bbport, bloodborne-native and shadPS4.
 * No trophy backend, files, contexts, handles or progress are created/accessed.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic size_t requests[6];
static int32_t unavailable(unsigned operation) {
    atomic_fetch_add_explicit(&requests[operation],1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x80551601); /* Trophy NOT_INITIALIZED. */
}
/* Each supplied export checks initialization before its arguments. Keep
 * outputs untouched, including the usual invalid-context/handle sentinels. */
static int32_t create_context(int32_t *out,int32_t user,uint32_t label,uint64_t options) {
    (void)out;(void)user;(void)label;(void)options;return unavailable(0);
}
static int32_t create_handle(int32_t *out) { (void)out;return unavailable(1); }
static int32_t destroy_context(int32_t context) { (void)context;return unavailable(2); }
static int32_t destroy_handle(int32_t handle) { (void)handle;return unavailable(3); }
static int32_t register_context(int32_t context,int32_t handle,uint64_t options) {
    (void)context;(void)handle;(void)options;return unavailable(4);
}
static int32_t unlock_trophy(int32_t context,int32_t handle,int32_t trophy,int32_t *platinum) {
    (void)context;(void)handle;(void)trophy;(void)platinum;return unavailable(5);
}
uintptr_t bootstrap_trophy_resolve(const char *name) {
    static const struct { const char *name;void *fn; } exports[]={
        {"XbkjbobZlCY#O#P",create_context},{"q7U6tEAQf7c#O#P",create_handle},
        {"E1Wrwd07Lr8#O#P",destroy_context},{"GNcF4oidY0Y#O#P",destroy_handle},
        {"TJCAxto9SEU#O#P",register_context},{"28xmRUFao68#O#P",unlock_trophy},
    };
    for(size_t i=0;i<sizeof(exports)/sizeof(*exports);++i)
        if(!strcmp(name,exports[i].name)) return (uintptr_t)exports[i].fn;
    return 0;
}
void bootstrap_trophy_report(void) {
    printf("Bootstrap: unavailable trophy requests: create context=%zu, create handle=%zu, destroy context=%zu, destroy handle=%zu, register=%zu, unlock=%zu (not initialized; no trophy access)\n",
           atomic_load_explicit(&requests[0],memory_order_relaxed),atomic_load_explicit(&requests[1],memory_order_relaxed),
           atomic_load_explicit(&requests[2],memory_order_relaxed),atomic_load_explicit(&requests[3],memory_order_relaxed),
           atomic_load_explicit(&requests[4],memory_order_relaxed),atomic_load_explicit(&requests[5],memory_order_relaxed));
}
