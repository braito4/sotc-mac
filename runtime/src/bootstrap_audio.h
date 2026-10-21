#ifndef SOTC_BOOTSTRAP_AUDIO_H
#define SOTC_BOOTSTRAP_AUDIO_H
#include <stdbool.h>
#include <stdint.h>
/* Pinned SDL3 callback ABI, resolved from the already loaded GPU dependency.
 * Audio subsystem readiness is queried on the main thread before guest start. */
typedef struct { int format, channels, frequency; } BootstrapAudioSpec;
typedef struct {
    uint32_t initialized_subsystems;
    const char *driver;
    void *(*open)(uint32_t device, const BootstrapAudioSpec *spec, void *callback, void *userdata);
    bool (*resume)(void *stream);
    void (*destroy)(void *stream);
    uint32_t (*device)(void *stream);
    bool (*device_format)(uint32_t device, BootstrapAudioSpec *spec, int *frames);
    bool (*put)(void *stream, const void *data, int bytes);
    int (*queued)(void *stream);
    bool (*flush)(void *stream);
} BootstrapAudioBackend;
int bootstrap_audio_bind(const BootstrapAudioBackend *backend);
uintptr_t bootstrap_audio_resolve(const char *name);
void bootstrap_audio_report(void);
#endif
