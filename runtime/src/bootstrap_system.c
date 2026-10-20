/* System-display metadata and host-generated UUIDs for the diagnostic.
 * ABI/error reference: shadPS4 systemservice.h/.cpp/systemservice_error.h;
 * SotC reference foundations: AjncSystem, deadinside28/bbport and
 * bloodborne-native (GPL-2.0-or-later). No host display or renderer is used.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <uuid/uuid.h>
static _Atomic size_t safe_area_requests;
static _Atomic size_t uuid_requests;
static int32_t create_uuid(void *out) {
    if (!out) return (int32_t)UINT32_C(0x80020016); /* Original wrapper: EINVAL. */
    uuid_t network;
    uuid_generate_random(network);
    /* FreeBSD/Orbis uuidgen uses native little-endian uint32/uint16 fields
     * for time_low, time_mid and time_hi_and_version. macOS uuid_t uses RFC
     * byte order. The remaining clock-sequence/node bytes keep their order.
     * This diagnostic uses a random v4 profile, without a hardware node ID. */
    static const unsigned char order[16] = {3,2,1,0,5,4,7,6,8,9,10,11,12,13,14,15};
    unsigned char guest[16];
    for (size_t i=0; i<sizeof(guest); ++i) guest[i]=network[order[i]];
    memcpy(out, guest, sizeof(guest));
    atomic_fetch_add_explicit(&uuid_requests, 1, memory_order_relaxed);
    return 0;
}
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
    if (!strcmp(name, "Xjoosiw+XPI#0#F")) return (uintptr_t)create_uuid;
    return 0;
}
void bootstrap_system_report(void) {
    printf("Bootstrap: logical display safe-area queries=%zu (ratio=1.0; no host display)\n",
           atomic_load_explicit(&safe_area_requests, memory_order_relaxed));
    printf("Bootstrap: UUID creations=%zu (host entropy; random v4 profile)\n",
           atomic_load_explicit(&uuid_requests, memory_order_relaxed));
}
