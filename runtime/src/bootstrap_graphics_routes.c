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
#include <stdatomic.h>

typedef uintptr_t (*Resolver)(const char *);
static Resolver graphics_resolver;
static pthread_mutex_t display_lock = PTHREAD_MUTEX_INITIALIZER;
static _Atomic size_t margin_requests;
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
static int32_t retail_window_margins(int32_t handle,int32_t top,int32_t bottom) {
    (void)top;(void)bottom;
    atomic_fetch_add_explicit(&margin_requests,1,memory_order_relaxed);
    pthread_mutex_lock(&display_lock);
    const int32_t result=validate_display(handle);
    pthread_mutex_unlock(&display_lock);
    /* The pinned renderer has no window-margin implementation: never call
     * its unconditional-success stub. Retail rejects an otherwise valid main
     * port without margin capability before inspecting margin values. */
    return result ? result : (int32_t)UINT32_C(0x8029001c);
}
void bootstrap_graphics_report(void) {
    if (graphics_resolver) printf("Graphics: unsupported VideoOut window-margin requests=%zu (no margin capability; no display update)\n",
        atomic_load_explicit(&margin_requests,memory_order_relaxed));
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
/* Retail compute packet writer. The pinned provider defaults SetShReg to
 * graphics headers here; bit 1 must select compute for all three register sets.
 * Only 15 dwords are initialized; the remainder of the 25-dword reservation
 * belongs to the trailing NOP and must retain its previous contents. */
static int32_t retail_cs_shader(uint32_t *commands,uint32_t size,
                               const uint32_t *regs,uint32_t modifier) {
    if (!commands || size < 25 || !regs || (modifier & UINT32_C(0xfffffc3f))) return -1;
    uint32_t high;memcpy(&high,(const unsigned char *)regs+4,sizeof(high));
    if (high) return -1;
    uint32_t r[7];memcpy(r,regs,sizeof(r));
    const uint32_t packet[15]={
        UINT32_C(0xc0027602),0x20c,r[0],0,
        UINT32_C(0xc0027602),0x212,
        modifier ? (r[2] & UINT32_C(0xfffffc3f)) | modifier : r[2],r[3],
        UINT32_C(0xc0037602),0x207,r[4],r[5],r[6],
        UINT32_C(0xc00a1000),0
    };
    memcpy(commands,packet,sizeof(packet));return 0;
}
static int32_t retail_cs_shader_plain(uint32_t *commands,uint32_t size,const uint32_t *regs) {
    return retail_cs_shader(commands,size,regs,0);
}
/* Retail WAIT_REG_MEM uses a 48-bit address field; preserve NOP padding. */
static int32_t retail_compute_wait(uint32_t *commands,uint32_t size,uintptr_t address,
                                   uint32_t mask,uint32_t compare,uint32_t reference) {
    if (!commands || size!=14) return -1;
    uint32_t memory=address>UINT32_C(0xffff);
    uint32_t packet[13]={UINT32_C(0xc0021000),0,0,0,
        UINT32_C(0xc0053c00),(memory<<4)|(compare&7),
        (uint32_t)address & (memory ? UINT32_C(0xfffffffc) : UINT32_C(0xffff)),
        (uint32_t)(address>>32)&UINT32_C(0xffff),reference,mask,10,
        UINT32_C(0xc0011000),0};
    /* The first NOP's two padding dwords are not initialized by retail. */
    memcpy(commands,packet,8);memcpy((unsigned char *)commands+16,packet+4,36);return 0;
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
    /* Pure PM4 debug-scope packet writers, not successful submissions/draws. */
    "W1Etj-jlW7Y#A#B", "7qZVNgEu+SY#A#B",
    /* Base-PS4 compute initialization: real register/barrier/NOP packets. */
    "nF6bFRUBRAU#A#B",
    /* Query the same provider lock used by submission/completion. */
    "b08AgtPlHPg#A#B",
    /* Notify the provider-owned mapped compute ring; void, three arguments. */
    "byXlqupd8cE#A#B",
    /* Reviewed indirect dispatch and doorbell-marker packet writers. */
    "Z43vKp5k7r0#A#B", "wED4ZXCFJT0#A#B", "NfvOrNzy6sk#A#B",
    "29oKvKXzEZo#A#B", "ArSg-TGinhk#A#B", "bX5IbRvECXk#A#B",
    "zwY0YV91TTI#A#B", "xbxNatawohc#A#B", "yvZ73uQUqrk#A#B",
    "Idffwf3yh8s#A#B", "QhnyReteJ1M#A#B", "0H2vBYbTLHI#A#B",
    "yb2cRhagD1I#A#B", "8lH54sfjfmU#A#B", "im2ZuItabu4#A#B",
    "0BzLGljcwBo#A#B", "HlTPoZ-oY7Y#A#B", "GGsn7jMTxw4#A#B",
    "KXltnCwEJHQ#A#B", "bQVd5YzCal0#A#B", "gAhCn6UiU4Y#A#B",
};
static int adapted_operation(const char *name) {
    return !strcmp(name, "ffrNQOshows#A#B") || !strcmp(name, "KXltnCwEJHQ#A#B") || !strcmp(name, "Kx-h-nWQJ8A#A#B") || !strcmp(name, "MTxxrOCeSig#G#H") || !strcmp(name, "pv9CI5VC+R0#G#H") || !strcmp(name, "kGVLc3htQE8#G#H");
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
    if (!strcmp(name, "ffrNQOshows#A#B")) return (uintptr_t)retail_compute_wait;
    if (!strcmp(name, "KXltnCwEJHQ#A#B")) return (uintptr_t)retail_cs_shader_plain;
    if (!strcmp(name, "Kx-h-nWQJ8A#A#B")) return (uintptr_t)retail_cs_shader;
    if (!strcmp(name, "MTxxrOCeSig#G#H")) return (uintptr_t)retail_window_margins;
    if (!strcmp(name, "Up36PTk687E#G#H")) return (uintptr_t)display_open;
    if (!strcmp(name, "uquVH4-Du78#G#H")) return (uintptr_t)display_close;
    if (!strcmp(name, "pv9CI5VC+R0#G#H")) return (uintptr_t)retail_adjust_color;
    if (!strcmp(name, "kGVLc3htQE8#G#H")) return (uintptr_t)retail_capability;
    return bootstrap_graphics_owns(name) ? graphics_resolver(name) : 0;
}
