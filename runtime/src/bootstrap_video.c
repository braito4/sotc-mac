/* Headless VideoOut availability profile for the CPU diagnostic.
 * Open ABI and error definitions: shadPS4 video_out.h/.cpp and
 * videoout_error.h (GPL-2.0-or-later). Foundations: shadPS4,
 * bloodborne-native, deadinside28/bbport and AjncSystem's SotC reference.
 * The suffixed gamma helper follows the supplied retail module ABI/layout.
 * This does not open a device, window, display port or rendering surface.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic size_t open_requests, gamma_requests, adjust_requests;
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
static int32_t color_settings_gamma(void *settings, uint32_t size, float gamma) {
    atomic_fetch_add_explicit(&gamma_requests, 1, memory_order_relaxed);
    /* The suffixed retail export takes an explicit byte size in RSI and
     * gamma in XMM0. It accepts 12 or 16 bytes, clears the requested extent,
     * then writes the same float into three channels. The original ordered
     * comparisons also accept NaN; preserve its input bits rather than
     * silently substituting a value. This prepares data, not device state. */
    if (!settings || (size != 12 && size != 16) || gamma > 2.0f || (double)gamma < 0.1)
        return (int32_t)UINT32_C(0x80290001);
    memset(settings, 0, size);
    for (unsigned i = 0; i < 3; ++i)
        memcpy((unsigned char *)settings + i * sizeof(gamma), &gamma, sizeof(gamma));
    return 0;
}
static int32_t color_adjust_unavailable(int32_t handle, const void *settings, uint32_t size) {
    atomic_fetch_add_explicit(&adjust_requests, 1, memory_order_relaxed);
    /* The suffixed retail ABI includes the size. Handle lookup precedes
     * settings/size validation. No successful open exists in this profile,
     * so every handle is invalid, including a previous open's error result.
     * Do not access settings, initialize state or claim a display update. */
    (void)handle;
    (void)settings;
    (void)size;
    return (int32_t)UINT32_C(0x8029000b); /* INVALID_HANDLE */
}
uintptr_t bootstrap_video_resolve(const char *name) {
    if (!strcmp(name, "Up36PTk687E#G#H")) return (uintptr_t)video_open;
    if (!strcmp(name, "DYhhWbJSeRg#G#H")) return (uintptr_t)color_settings_gamma;
    if (!strcmp(name, "pv9CI5VC+R0#G#H")) return (uintptr_t)color_adjust_unavailable;
    return 0;
}
void bootstrap_video_report(void) {
    printf("Bootstrap: unavailable VideoOut open requests=%zu (no device or display handle)\n",
           atomic_load_explicit(&open_requests, memory_order_relaxed));
    printf("Bootstrap: VideoOut color-settings gamma preparations=%zu (no device update)\n",
           atomic_load_explicit(&gamma_requests, memory_order_relaxed));
    printf("Bootstrap: rejected VideoOut color adjustments=%zu (no valid display handle)\n",
           atomic_load_explicit(&adjust_requests, memory_order_relaxed));
}
