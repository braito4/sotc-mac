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
int bootstrap_pad_bind(const BootstrapKeyboard *backend);
uintptr_t bootstrap_pad_resolve(const char *name);
void bootstrap_pad_report(void);
#endif
