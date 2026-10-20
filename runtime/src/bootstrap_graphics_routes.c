/* Optional graphics routing for the pinned shadPS4/bloodborne-native provider.
 * Only explicit SotC scopes and reviewed entry points cross this boundary.
 * Unknown imports and unrepresentable color profiles stop explicitly.
 * The CPU-only diagnostic remains the default; no provider is replaced there.
 */
#include "bootstrap_contracts.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef uintptr_t (*Resolver)(const char *);
static Resolver graphics_resolver;
static pthread_mutex_t display_lock = PTHREAD_MUTEX_INITIALIZER;
static int32_t live_display = -1; /* Pinned provider supports one main port. */
static int32_t (*graphics_open)(int32_t, int32_t, int32_t, const void *);
static int32_t (*graphics_close)(int32_t);
static int32_t display_open(int32_t user, int32_t bus, int32_t index, const void *params) {
    pthread_mutex_lock(&display_lock);
    const int32_t result = graphics_open(user, bus, index, params);
    if (result >= 0) live_display = result;
    pthread_mutex_unlock(&display_lock);
    return result;
}
static int32_t display_close(int32_t handle) {
    pthread_mutex_lock(&display_lock);
    int32_t result = (int32_t)UINT32_C(0x8029000b);
    if (handle == live_display && live_display >= 0) {
        result = graphics_close(handle);
        if (!result) live_display = -1;
    }
    pthread_mutex_unlock(&display_lock);
    return result;
}

static int32_t (*graphics_flip_status)(int32_t, void *);
static int32_t (*graphics_adjust_color)(int32_t, const void *);
static int32_t (*graphics_capability)(int32_t, void *);
static int32_t validate_display(int32_t handle) {
    /* Pinned FlipStatus is 64 bytes. Lookup precedes accessing the caller's
     * settings/output, matching the supplied retail empty-handle behavior. */
    if (handle != live_display || live_display < 0) return (int32_t)UINT32_C(0x8029000b);
    uint64_t status[8] = {0};
    return graphics_flip_status(handle, status);
}
static _Noreturn void unsupported_color(void) {
    fputs("STOP_GRAPHICS_COLOR: unsupported color size, unequal channel gamma or reserved data\n", stderr);
    fflush(NULL);
    _Exit(29);
}
static int32_t adjust_color_locked(int32_t handle, const void *settings, uint32_t size) {
    const int32_t result = validate_display(handle);
    if (result) return result;
    if (size != 12 && size != 16) unsupported_color();
    if (!settings) return graphics_adjust_color(handle, NULL);
    uint32_t channels[4] = {0};
    memcpy(channels, settings, size);
    /* The checked retail gamma helper repeats one gamma in RGB. Translate
     * that representable profile into the HLE's scalar gamma + reserved words.
     * Bit comparisons retain NaN payloads and avoid silently averaging RGB. */
    if (channels[0] != channels[1] || channels[0] != channels[2] || channels[3]) unsupported_color();
    struct { float gamma; uint32_t reserved[3]; } native = {0};
    memcpy(&native.gamma, channels, sizeof(native.gamma));
    return graphics_adjust_color(handle, &native);
}
static int32_t capability_locked(int32_t handle, void *output, uint64_t size) {
    const int32_t result = validate_display(handle);
    if (result) return result;
    /* Retail uses an exact eight-byte output and a full-width size, unlike
     * the upstream two-argument helper. Stage the output to preserve guards,
     * alignment and failure behavior without inventing capability bits. */
    if (!output || size != sizeof(uint64_t)) return (int32_t)UINT32_C(0x80290001);
    uint64_t capability = 0;
    const int32_t query = graphics_capability(handle, &capability);
    if (!query) memcpy(output, &capability, sizeof(capability));
    return query;
}

static int32_t retail_adjust_color(int32_t handle, const void *settings, uint32_t size) {
    pthread_mutex_lock(&display_lock);
    const int32_t result = adjust_color_locked(handle, settings, size);
    pthread_mutex_unlock(&display_lock);
    return result;
}
static int32_t retail_capability(int32_t handle, void *output, uint64_t size) {
    pthread_mutex_lock(&display_lock);
    const int32_t result = capability_locked(handle, output, size);
    pthread_mutex_unlock(&display_lock);
    return result;
}
static const char *const graphics_imports[] = {
    /* One provider owns creation, waits, user events and GPU notifications. */
    "D0OdFMjp46I#0#F", "jpFjmgAC5AE#0#F", "fzyMKs9kim0#0#F",
    "4R6-OvI2cEA#0#F", "WDszmSbWuDk#0#F", "F6e0kwo4cnk#0#F",
    "LJDwdSNTnDg#0#F", "vz+pg2zdopI#0#F", "mJ7aghmgvfc#0#F",
    "23CPPI1tyBY#0#F", "kwGyyjohI50#0#F",
    /* Display ports, buffers, flip/vblank events and status. Data preparation
     * stays in the checked adapter; suffixed color/capability calls use typed
     * wrappers around the upstream two-argument HLE functions. */
    "Up36PTk687E#G#H", "uquVH4-Du78#G#H", "w3BY+tAEiQY#G#H",
    "N5KDtkIjjJ4#G#H", "U46NwOiJpys#G#H", "HXzjK9yI30k#G#H",
    "Xru92wHJRmg#G#H", "CBiu4mCE1DA#G#H", "SbU3dwp80lQ#G#H",
    "6kPnj51T62Y#G#H", "j6RaAUlaLv0#G#H", "1FZBKy8HeNU#G#H",
    "U2JJtSqNKZI#G#H", "rWUTcKdkUzQ#G#H", "Mt4QHHkxkOc#G#H",
    "-Ozn0F1AFRg#G#H", "oNOQn3knW6s#G#H", "OcQybQejHEY#G#H",
    "N1bEoJ4SRw4#G#H",
    /* Real compute mapping, command encoding, submission and completion.
     * Retail resource registration retains its existing checked failure path. */
    "29oKvKXzEZo#A#B", "ArSg-TGinhk#A#B", "bX5IbRvECXk#A#B",
    "zwY0YV91TTI#A#B", "xbxNatawohc#A#B", "yvZ73uQUqrk#A#B",
    "Idffwf3yh8s#A#B", "QhnyReteJ1M#A#B", "0H2vBYbTLHI#A#B",
    "yb2cRhagD1I#A#B", "8lH54sfjfmU#A#B", "im2ZuItabu4#A#B",
    "0BzLGljcwBo#A#B", "HlTPoZ-oY7Y#A#B", "GGsn7jMTxw4#A#B",
    "KXltnCwEJHQ#A#B", "bQVd5YzCal0#A#B", "gAhCn6UiU4Y#A#B",
};
static int adapted_operation(const char *name) {
    return !strcmp(name, "pv9CI5VC+R0#G#H") || !strcmp(name, "kGVLc3htQE8#G#H");
}
int bootstrap_graphics_owns(const char *name) {
    if (!graphics_resolver) return 0;
    if (adapted_operation(name)) return 1;
    for (size_t i = 0; i < sizeof(graphics_imports)/sizeof(*graphics_imports); ++i)
        if (!strcmp(name, graphics_imports[i])) return 1;
    return 0;
}
int bootstrap_graphics_bind(Resolver resolver) {
    /* Publish before guest startup only. Never mix existing CPU queue handles
     * with the renderer's queue registry, or publish a partial provider. */
    if (!resolver || graphics_resolver || bootstrap_equeue_objects()) return -1;
    for (size_t i = 0; i < sizeof(graphics_imports)/sizeof(*graphics_imports); ++i)
        if (!resolver(graphics_imports[i])) return -1;
    const uintptr_t adjust = resolver("pv9CI5VC+R0#G#H");
    const uintptr_t capability = resolver("kGVLc3htQE8#G#H");
    const uintptr_t open = resolver("Up36PTk687E#G#H");
    const uintptr_t close = resolver("uquVH4-Du78#G#H");
    const uintptr_t status = resolver("SbU3dwp80lQ#G#H");
    if (!adjust || !capability || !open || !close || !status) return -1;
    graphics_open = (void *)open;
    graphics_close = (void *)close;
    graphics_flip_status = (void *)status;
    graphics_adjust_color = (void *)adjust;
    graphics_capability = (void *)capability;
    graphics_resolver = resolver;
    return 0;
}
uintptr_t bootstrap_graphics_resolve(const char *name) {
    if (!graphics_resolver) return 0;
    if (!strcmp(name, "Up36PTk687E#G#H")) return (uintptr_t)display_open;
    if (!strcmp(name, "uquVH4-Du78#G#H")) return (uintptr_t)display_close;
    if (!strcmp(name, "pv9CI5VC+R0#G#H")) return (uintptr_t)retail_adjust_color;
    if (!strcmp(name, "kGVLc3htQE8#G#H")) return (uintptr_t)retail_capability;
    return bootstrap_graphics_owns(name) ? graphics_resolver(name) : 0;
}
