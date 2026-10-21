#ifndef SOTC_BOOTSTRAP_PAD_H
#define SOTC_BOOTSTRAP_PAD_H
#include <stdbool.h>
#include <stdint.h>
/* SDL callback ABI; populated on the renderer's main thread after SDL startup.
 * Numeric subsystem flags are checked against the pinned SDL3 API. */
typedef struct {
    uint32_t initialized_subsystems;
    const bool *(*keyboard)(int *count);
    int (*scancode)(const char *name);
    int (*captured)(void);
} BootstrapKeyboard;
/* SDL3 gamepad ABI, resolved from the resident graphics provider. All callbacks
 * below are thread-safe in SDL3. Initialization/event pumping stays on main. */
typedef struct {
    uint32_t *(*devices)(int *count);
    void (*free_devices)(void *devices);
    void *(*open)(uint32_t id);
    void (*close)(void *pad);
    bool (*connected)(void *pad);
    const char *(*name)(void *pad);
    int16_t (*axis)(void *pad, int axis);
    bool (*button)(void *pad, int button);
    bool (*touch)(void *pad, int touchpad, int finger, bool *down,
                  float *x, float *y, float *pressure);
    bool (*rumble)(void *pad, uint16_t low, uint16_t high, uint32_t ms);
    bool (*led)(void *pad, uint8_t r, uint8_t g, uint8_t b);
} BootstrapGamepad;
int bootstrap_pad_bind(const BootstrapKeyboard *backend);
int bootstrap_pad_bind_gamepad(const BootstrapGamepad *backend);
uintptr_t bootstrap_pad_resolve(const char *name);
void bootstrap_pad_report(void);
#endif
