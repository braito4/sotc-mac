/* Headless VideoOut availability profile for the CPU diagnostic.
 * Open ABI and error definitions: shadPS4 video_out.h/.cpp and
 * videoout_error.h (GPL-2.0-or-later). Foundations: shadPS4,
 * bloodborne-native, deadinside28/bbport and AjncSystem's SotC reference.
 * Data helpers and early error paths are checked against the supplied retail
 * module ABI/layout, including its ConfigureOutputMode validation order.
 * This does not open a device, window, display port or rendering surface.
 */
#include "bootstrap_contracts.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static _Atomic size_t open_requests, gamma_requests, adjust_requests, rate_requests, capability_requests;
static _Atomic size_t margin_requests;
static _Atomic size_t flip_event_requests, attribute_requests, register_requests, mode_requests, configure_requests;
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
static int32_t window_margins_unavailable(int32_t handle,int32_t top,int32_t bottom) {
    (void)handle;(void)top;(void)bottom;
    atomic_fetch_add_explicit(&margin_requests,1,memory_order_relaxed);
    return (int32_t)UINT32_C(0x8029000b); /* No open port; handle lookup first. */
}
static int32_t flip_rate_unavailable(int32_t handle, int32_t rate) {
    atomic_fetch_add_explicit(&rate_requests, 1, memory_order_relaxed);
    /* Retail handle lookup precedes rate validation. The headless profile
     * has no open ports; even NO_DEVICE passed as a handle stays invalid. */
    (void)handle;
    (void)rate;
    return (int32_t)UINT32_C(0x8029000b);
}
static int32_t capability_unavailable(int32_t handle, void *settings, uint64_t size) {
    atomic_fetch_add_explicit(&capability_requests, 1, memory_order_relaxed);
    /* The suffixed retail query carries a full-width size; the empty handle
     * table rejects the handle before it can access either output argument. */
    (void)handle;
    (void)settings;
    (void)size;
    return (int32_t)UINT32_C(0x8029000b);
}
static int32_t flip_event_unavailable(uintptr_t queue, int32_t handle, void *user_data) {
    atomic_fetch_add_explicit(&flip_event_requests, 1, memory_order_relaxed);
    /* Port lookup precedes queue validation. No open port exists, so do not
     * inspect the queue or register an event that could never be signaled. */
    (void)queue;
    (void)handle;
    (void)user_data;
    return (int32_t)UINT32_C(0x8029000b);
}
static void buffer_attribute(void *output, uint32_t format, uint32_t tiling,
                             uint32_t aspect, uint32_t width, uint32_t height, uint32_t pitch) {
    atomic_fetch_add_explicit(&attribute_requests, 1, memory_order_relaxed);
    /* Pure data helper, not a registration or allocation. Seven integer
     * arguments: pitch arrives through the guest stack. Copy preserves an
     * unaligned output and writes exactly the 40-byte public structure. */
    const uint32_t words[10] = {format, tiling, aspect, width, height, pitch, 0, 0, 0, 0};
    memcpy(output, words, sizeof(words));
}
static int32_t register_buffers_unavailable(int32_t handle, int32_t start,
                                            void *const *addresses, int32_t count, const void *attribute) {
    atomic_fetch_add_explicit(&register_requests, 1, memory_order_relaxed);
    /* Null required addresses are checked first; no port is open in this
     * profile, so non-null arguments are never dereferenced or registered. */
    (void)handle;
    (void)start;
    (void)count;
    if (!addresses || !attribute) return (int32_t)UINT32_C(0x80290002);
    return (int32_t)UINT32_C(0x8029000b);
}
static void mode_any(void *output, uint32_t size) {
    atomic_fetch_add_explicit(&mode_requests, 1, memory_order_relaxed);
    /* The retail helper fills the requested extent and writes a size field.
     * Even short sizes still write that four-byte field. No display state. */
    memset(output, 0xff, size);
    memcpy(output, &size, sizeof(size));
}
static int32_t configure_unavailable(int32_t handle, uint32_t reserved, const void *mode,
                                     const void *options, uint32_t mode_size, uint32_t options_size) {
    atomic_fetch_add_explicit(&configure_requests, 1, memory_order_relaxed);
    (void)handle; (void)options; (void)mode_size; (void)options_size;
    /* Retail validates reserved and null mode before the empty port lookup;
     * it does not dereference non-null mode/options for an invalid handle. */
    if (reserved || !mode) return (int32_t)UINT32_C(0x80290001);
    return (int32_t)UINT32_C(0x8029000b);
}
uintptr_t bootstrap_video_resolve(const char *name) {
    if (!strcmp(name, "MTxxrOCeSig#G#H")) return (uintptr_t)window_margins_unavailable;
    if (!strcmp(name, "pjkDsgxli6c#G#H")) return (uintptr_t)mode_any;
    if (!strcmp(name, "N1bEoJ4SRw4#G#H")) return (uintptr_t)configure_unavailable;
    if (!strcmp(name, "w3BY+tAEiQY#G#H")) return (uintptr_t)register_buffers_unavailable;
    if (!strcmp(name, "i6-sR91Wt-4#G#H")) return (uintptr_t)buffer_attribute;
    if (!strcmp(name, "HXzjK9yI30k#G#H")) return (uintptr_t)flip_event_unavailable;
    if (!strcmp(name, "Up36PTk687E#G#H")) return (uintptr_t)video_open;
    if (!strcmp(name, "DYhhWbJSeRg#G#H")) return (uintptr_t)color_settings_gamma;
    if (!strcmp(name, "pv9CI5VC+R0#G#H")) return (uintptr_t)color_adjust_unavailable;
    if (!strcmp(name, "CBiu4mCE1DA#G#H")) return (uintptr_t)flip_rate_unavailable;
    if (!strcmp(name, "kGVLc3htQE8#G#H")) return (uintptr_t)capability_unavailable;
    return 0;
}
void bootstrap_video_report(void) {
    printf("Bootstrap: rejected VideoOut window-margin requests=%zu (no valid display handle)\n",
           atomic_load_explicit(&margin_requests,memory_order_relaxed));
    printf("Bootstrap: VideoOut mode preparations=%zu, rejected configurations=%zu (no valid display handle)\n",
           atomic_load_explicit(&mode_requests, memory_order_relaxed),
           atomic_load_explicit(&configure_requests, memory_order_relaxed));
    printf("Bootstrap: rejected VideoOut buffer registrations=%zu (no valid display handle)\n",
           atomic_load_explicit(&register_requests, memory_order_relaxed));
    printf("Bootstrap: VideoOut buffer attributes prepared=%zu (no buffer registration)\n",
           atomic_load_explicit(&attribute_requests, memory_order_relaxed));
    printf("Bootstrap: rejected VideoOut flip-event registrations=%zu (no valid display handle)\n",
           atomic_load_explicit(&flip_event_requests, memory_order_relaxed));
    printf("Bootstrap: unavailable VideoOut open requests=%zu (no device or display handle)\n",
           atomic_load_explicit(&open_requests, memory_order_relaxed));
    printf("Bootstrap: VideoOut color-settings gamma preparations=%zu (no device update)\n",
           atomic_load_explicit(&gamma_requests, memory_order_relaxed));
    printf("Bootstrap: rejected VideoOut color adjustments=%zu (no valid display handle)\n",
           atomic_load_explicit(&adjust_requests, memory_order_relaxed));
    printf("Bootstrap: rejected VideoOut flip-rate changes=%zu (no valid display handle)\n",
           atomic_load_explicit(&rate_requests, memory_order_relaxed));
    printf("Bootstrap: rejected VideoOut capability queries=%zu (no valid display handle)\n",
           atomic_load_explicit(&capability_requests, memory_order_relaxed));
}
