/* Keyboard-backed logical libScePad device for the graphics diagnostic.
 * ABI layouts/errors from bloodborne-native runtime_pad.c and AjncSystem's
 * SotC reference (GPL-2.0-or-later); SDL3 supplies actual keyboard state.
 * No Bloodborne heap addresses, input injection/replay or game hooks are used.
 * Physical gamepads, mouse, sensors, light-bar output and vibration remain
 * unsupported. Light-bar ABI/validation also checked against the supplied
 * retail module; NOT_PERMITTED is from shadPS4 pad_errors.h.
 */
#include "bootstrap_pad.h"
#include "bootstrap_contracts.h"
#include "runtime.h"
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define PAD_ERROR(code) ((int32_t)(UINT32_C(0x80920000) | (code)))
typedef struct { uint16_t x, y; uint8_t id, reserved[3]; } Touch;
typedef struct {
    uint32_t buttons;
    uint8_t left_x, left_y, right_x, right_y, l2, r2, analog_padding[2];
    float orientation[4], acceleration[3], angular_velocity[3];
    uint8_t touch_count, touch_reserved[3];
    uint32_t touch_held_time;
    Touch touches[2];
    uint8_t connected, padding[3];
    uint64_t timestamp;
    uint8_t extension[16], connected_count, reserved[2], unique_length, unique[12];
} PadData;
typedef struct {
    float density; uint16_t width, height;
    uint8_t dead_left, dead_right, connection_type, connected_count, connected, padding[3];
    int32_t device_class; uint8_t reserved[8];
} ControllerInfo;
_Static_assert(sizeof(PadData) == 120 && offsetof(PadData, timestamp) == 80, "Orbis pad ABI");
_Static_assert(sizeof(ControllerInfo) == 28 && sizeof(Touch) == 8, "Orbis controller ABI");
static pthread_mutex_t pad_lock = PTHREAD_MUTEX_INITIALIZER;
static BootstrapKeyboard keyboard;
static int bound, initialized, opened, handle, next_handle = 1;
static uint8_t connection_count, touch_id;
static int last_touch;
static uint64_t touch_since, reads, lightbar_requests, vibration_requests;
static const struct { const char *key; uint32_t button; } bindings[] = {
    {"Space",0x4000}, {"Left Shift",0x2000}, {"E",0x8000}, {"Q",0x1000},
    {"1",0x400}, {"3",0x800}, {"R",0x100}, {"F",0x200},
    {"Z",0x2}, {"C",0x4}, {"Return",0x8},
    {"I",0x10}, {"L",0x20}, {"K",0x40}, {"J",0x80},
    {"A",0}, {"D",0}, {"W",0}, {"S",0},
    {"Left",0}, {"Right",0}, {"Up",0}, {"Down",0}, {"Tab",0}, {"Backspace",0},
};
static int codes[sizeof(bindings)/sizeof(*bindings)];
int bootstrap_pad_bind(const BootstrapKeyboard *backend) {
    if (!backend || !backend->keyboard || !backend->scancode || !backend->captured ||
        (backend->initialized_subsystems & UINT32_C(0x2020)) != UINT32_C(0x2020)) return -1;
    pthread_mutex_lock(&pad_lock);
    if (bound) { pthread_mutex_unlock(&pad_lock); return -1; }
    int count = 0, prepared[sizeof(codes)/sizeof(*codes)];
    const bool *keys = backend->keyboard(&count);
    if (!keys || count <= 0) { pthread_mutex_unlock(&pad_lock); return -1; }
    for (size_t i = 0; i < sizeof(codes)/sizeof(*codes); ++i) {
        prepared[i] = backend->scancode(bindings[i].key);
        if (prepared[i] <= 0 || prepared[i] >= count) { pthread_mutex_unlock(&pad_lock); return -1; }
    }
    keyboard = *backend; memcpy(codes, prepared, sizeof(codes)); bound = 1;
    pthread_mutex_unlock(&pad_lock);
    return 0;
}
static int32_t pad_init(void) {
    pthread_mutex_lock(&pad_lock);
    const int32_t result = bound ? 0 : PAD_ERROR(5);
    if (!result) initialized = 1;
    pthread_mutex_unlock(&pad_lock); return result;
}
static int32_t pad_open(int32_t user, int32_t type, int32_t index, const void *params) {
    pthread_mutex_lock(&pad_lock);
    int32_t result = !initialized ? PAD_ERROR(5)
        : user != 1 || type || index || params ? PAD_ERROR(1)
        : opened ? PAD_ERROR(4) : next_handle == INT32_MAX ? PAD_ERROR(1) : 0;
    if (!result) {
        opened = 1; handle = next_handle++; result = handle;
        ++connection_count; if (!connection_count) connection_count = 1;
        last_touch = 0; touch_since = 0;
        puts("Input: keyboard-backed logical controller opened for local user 1");
    }
    pthread_mutex_unlock(&pad_lock); return result;
}
static int32_t pad_close(int32_t id) {
    pthread_mutex_lock(&pad_lock);
    const int32_t result = !opened || id != handle ? PAD_ERROR(3) : 0;
    if (!result) opened = 0;
    pthread_mutex_unlock(&pad_lock); return result;
}
static int pressed(const bool *keys, int count, size_t binding) {
    return keys && codes[binding] < count && keys[codes[binding]];
}
static uint8_t axis(int negative, int positive) { return negative == positive ? 128 : negative ? 0 : 255; }
/* Called under pad_lock. The window's main thread pumps SDL events; this only
 * reads SDL_GetKeyboardState, documented by SDL3 as safe from any thread. */
static int32_t sample(PadData *out) {
    int count = 0;
    const bool *keys = keyboard.keyboard(&count);
    if (!keys || count <= 0) return PAD_ERROR(5);
    PadData data = {0};
    data.left_x = data.left_y = data.right_x = data.right_y = 128;
    data.orientation[3] = 1; data.connected = 1; data.connected_count = connection_count;
    data.timestamp = runtime_process_time_us();
    if (!keyboard.captured()) {
        for (size_t i = 0; i < 15; ++i) if (pressed(keys, count, i)) data.buttons |= bindings[i].button;
        data.left_x = axis(pressed(keys,count,15),pressed(keys,count,16));
        data.left_y = axis(pressed(keys,count,17),pressed(keys,count,18));
        data.right_x = axis(pressed(keys,count,19),pressed(keys,count,20));
        data.right_y = axis(pressed(keys,count,21),pressed(keys,count,22));
        data.l2 = data.buttons & 0x100 ? 255 : 0;
        data.r2 = data.buttons & 0x200 ? 255 : 0;
        const int left = pressed(keys,count,23), right = pressed(keys,count,24);
        if (left || right) {
            if (!last_touch) { touch_id = touch_id == 127 ? 1 : touch_id+1; touch_since = data.timestamp; }
            data.buttons |= 0x100000; data.touch_count = 1;
            data.touches[0] = (Touch){.x = right ? 1440 : 480, .y = 471, .id = touch_id};
            data.touch_held_time = (uint32_t)(data.timestamp-touch_since);
        }
    }
    last_touch = data.touch_count != 0;
    memcpy(out, &data, sizeof(data)); ++reads;
    return 0;
}
static int32_t read_state(int32_t id, PadData *out) {
    pthread_mutex_lock(&pad_lock);
    const int32_t result = !opened || id != handle ? PAD_ERROR(3) : !out ? PAD_ERROR(1) : sample(out);
    pthread_mutex_unlock(&pad_lock); return result;
}
static int32_t read_buffered(int32_t id, PadData *out, int32_t count) {
    pthread_mutex_lock(&pad_lock);
    int32_t result = !opened || id != handle ? PAD_ERROR(3) : !out || count < 1 || count > 64 ? PAD_ERROR(1) : sample(out);
    if (!result) result = 1; /* One real sample; no fabricated backlog. */
    pthread_mutex_unlock(&pad_lock); return result;
}
static int32_t controller_info(int32_t id, ControllerInfo *out) {
    pthread_mutex_lock(&pad_lock);
    const int32_t result = !opened || id != handle ? PAD_ERROR(3) : !out ? PAD_ERROR(1) : 0;
    if (!result) {
        /* This is the logical keyboard controller's advertised layout, not
         * a claim that a physical DualShock or motion sensor is connected. */
        const ControllerInfo info = {.density=44.86f, .width=1920, .height=943,
            .connected=1, .connected_count=connection_count, .device_class=0};
        memcpy(out, &info, sizeof(info));
    }
    pthread_mutex_unlock(&pad_lock); return result;
}
static int32_t set_vibration(int32_t id,const void *param) {
    pthread_mutex_lock(&pad_lock);
    ++vibration_requests;
    /* Retail: initialization, null parameter, handle, then two motor bytes.
     * The keyboard controller owns no motors. NOT_PERMITTED is an explicit
     * diagnostic policy, including zero intensities; no output is simulated. */
    int32_t result=!initialized ? PAD_ERROR(5) : !param ? PAD_ERROR(1)
        : !opened || id!=handle ? PAD_ERROR(3) : 0;
    unsigned char motors[2];
    if (!result) result=bb_read_memory(motors,(uintptr_t)param,sizeof(motors))!=sizeof(motors)
        ? PAD_ERROR(1) : PAD_ERROR(0x101);
    pthread_mutex_unlock(&pad_lock);
    return result;
}
static int32_t set_lightbar(int32_t id, const void *param) {
    pthread_mutex_lock(&pad_lock);
    ++lightbar_requests;
    /* Retail validation order: initialization, null parameter, handle, RGB.
     * Only RGB is defined input here; the reserved byte is never read. */
    int32_t result=!initialized ? PAD_ERROR(5) : !param ? PAD_ERROR(1)
        : !opened || id!=handle ? PAD_ERROR(3) : 0;
    unsigned char rgb[3];
    if (!result) {
        if (bb_read_memory(rgb,(uintptr_t)param,sizeof(rgb))!=sizeof(rgb)) result=PAD_ERROR(1);
        else if (rgb[0]<=12 && rgb[1]<=12 && rgb[2]<=12) result=PAD_ERROR(6);
        /* This logical keyboard device has no physical light bar. Explicit
         * diagnostic policy: NOT_PERMITTED, never pretend the color changed. */
        else result=PAD_ERROR(0x101);
    }
    pthread_mutex_unlock(&pad_lock);
    return result;
}
uintptr_t bootstrap_pad_resolve(const char *name) {
    if (!bound) return 0; /* Default headless mode keeps its existing import stop. */
    static const struct { const char *name; void *fn; } exports[] = {
        {"hv1luiJrqQM#H#I",pad_init}, {"xk0AcarP3V4#H#I",pad_open},
        {"6ncge5+l5Qs#H#I",pad_close}, {"YndgXqQVV7c#H#I",read_state},
        {"RR4novUEENY#H#I",set_lightbar}, {"yFVnOdGxvZY#H#I",set_vibration},
        {"q1cHNfGycLI#H#I",read_buffered}, {"gjP9-KQzoUk#H#I",controller_info},
    };
    for (size_t i=0; i<sizeof(exports)/sizeof(*exports); ++i)
        if (!strcmp(name,exports[i].name)) return (uintptr_t)exports[i].fn;
    return 0;
}
void bootstrap_pad_report(void) {
    pthread_mutex_lock(&pad_lock);
    if (bound) printf("Input: keyboard backend ready; initialized=%d, open=%d, samples=%llu, unavailable light-bar requests=%llu, unavailable vibration requests=%llu\n",
                     initialized,opened,(unsigned long long)reads,(unsigned long long)lightbar_requests,(unsigned long long)vibration_requests);
    pthread_mutex_unlock(&pad_lock);
}
