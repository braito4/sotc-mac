/* Headless VideoOut availability profile for the CPU diagnostic.
 * Four-argument ABI and error definitions: shadPS4 video_out.h/.cpp and
 * videoout_error.h (GPL-2.0-or-later). Foundations: shadPS4,
 * bloodborne-native, deadinside28/bbport and AjncSystem's SotC reference.
 * This does not open a device, window, display port or rendering surface.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic size_t open_requests;
static int32_t video_open(int32_t user, int32_t bus, int32_t index, const void *params) {
    atomic_fetch_add_explicit(&open_requests, 1, memory_order_relaxed);
    /* Diagnostic policy: only the MAIN/index-zero profile is recognized.
     * Other buses are unsupported here, not a claim that retail VideoOut
     * lacks those modes. No device exists, so no user/service-thread options
     * are inspected, and no host display or guest-visible handle is created. */
    (void)user;
    (void)params;
    if (bus != 0) return (int32_t)UINT32_C(0x80290016); /* UNSUPPORTED_OUTPUT_MODE */
    if (index != 0) return (int32_t)UINT32_C(0x80290001); /* INVALID_VALUE */
    return (int32_t)UINT32_C(0x80290018); /* NO_DEVICE */
}
uintptr_t bootstrap_video_resolve(const char *name) {
    if (!strcmp(name, "Up36PTk687E#G#H")) return (uintptr_t)video_open;
    return 0;
}
void bootstrap_video_report(void) {
    printf("Bootstrap: unavailable VideoOut open requests=%zu (no device or display handle)\n",
           atomic_load_explicit(&open_requests, memory_order_relaxed));
}
