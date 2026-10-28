/* Isolated native loader/renderer prerequisites for bloodborne-runtime's GPU.
 * Load-only mode invokes no GPU API. Renderer mode initializes on the host main
 * thread and pumps its window, with a fixed budget. The optional game path also
 * registers the shared kernel provider and keeps the module resident. Image
 * patches are never applied here; game imports stay behind a scoped allowlist.
 */
#include "bootstrap_contracts.h"
#include "bootstrap_pad.h"
#include "bootstrap_audio.h"
#include "bbgpu.h"
#include <dlfcn.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void renderer_timeout(int signal_number) {
    (void)signal_number;
    static const char message[] = "STOP_RENDERER_TIMEOUT: 60-second diagnostic budget exhausted\n";
    (void)write(STDERR_FILENO, message, sizeof(message)-1);
    _Exit(21);
}
static void *game_gpu_library;
static int (*game_gpu_fault)(void *, void *);
static void (*game_gpu_window)(void);
int bootstrap_gpu_handle_fault(void *context, void *address) {
    return game_gpu_fault ? game_gpu_fault(context, address) : 0;
}
void bootstrap_gpu_window_loop(void) {
    if (!game_gpu_library || !game_gpu_window) _Exit(28);
    game_gpu_window();
    _Exit(0);
}
static int gpu_check(const char *path, int initialize, uint32_t sdk) {
    if (initialize && !pthread_main_np()) {
        fputs("STOP_GPU_THREAD: renderer initialization requires the host main thread\n", stderr);
        return 28;
    }
    /* The pinned library creates cache folders from a C++ static initializer.
     * Configure an existing private directory BEFORE dlopen, otherwise it falls
     * back to the user's ordinary shadPS4 data directory. */
    const char *user_dir = getenv("BB_GPU_USER_DIR");
    struct stat st;
    if (!user_dir || user_dir[0] != '/' || lstat(user_dir, &st) || !S_ISDIR(st.st_mode) || access(user_dir, W_OK)) {
        fputs("STOP_GPU_STATE: BB_GPU_USER_DIR must name an existing writable absolute directory\n", stderr);
        return 28;
    }
    /* Withdraw the unvalidated DMA experiment after a reported host watchdog
     * panic. Reject inherited overrides before library constructors can run.
     * DMA off is a rollback, not proof that renderer startup cannot hang. */
    const char *dma = getenv("BB_DIRECT_MEMORY_ACCESS");
    if (dma && strcmp(dma, "0")) {
        fputs("STOP_GPU_CONFIG: BB_DIRECT_MEMORY_ACCESS must be unset or 0; "
              "dynamic shader reads are suspended after a host watchdog panic\n", stderr);
        return 28;
    }
    if (setenv("BB_DIRECT_MEMORY_ACCESS", "0", 1)) {
        perror("STOP_GPU_CONFIG"); return 28;
    }
    if (initialize == 2) {
        printf("GPU: shader dynamic memory reads BB_DIRECT_MEMORY_ACCESS=%s\n",
               getenv("BB_DIRECT_MEMORY_ACCESS"));
    }
    void *library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        fprintf(stderr, "STOP_GPU_LOAD: %s\n", dlerror());
        return 28;
    }
    static const char *required[] = {
        "bbgpu_init", "bbgpu_register_kernel", "bbgpu_resolve",
        "bbgpu_window_loop", "bbgpu_handle_fault", "bbgpu_symbol_count"
    };
    int failed = 0;
    for (size_t i = 0; i < sizeof(required) / sizeof(*required); ++i) {
        dlerror();
        void *symbol = dlsym(library, required[i]);
        const char *error = dlerror();
        if (!symbol || error) {
            fprintf(stderr, "STOP_GPU_EXPORT: %s (%s)\n", required[i], error ? error : "null symbol");
            failed = 1;
        }
    }
    if (!failed && initialize) {
        /* No image patches, guest execution, or registration of the GPU's
         * kernel queue provider: this process only initializes the renderer. */
        int (*initialize_gpu)(const BbGpuConfig *) = (void *)dlsym(library, "bbgpu_init");
        void (*window_loop)(void) = (void *)dlsym(library, "bbgpu_window_loop");
        const BbGpuConfig config = {
            .title = "SotC renderer diagnostic - no game frame",
            .serial = "CUSA08809", .user_dir = user_dir,
            .sdk_version = sdk, .psf_attributes = 0, .width = 1280, .height = 720,
        };
        struct sigaction action = {0};
        action.sa_handler = renderer_timeout;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGALRM, &action, NULL)) {
            perror("STOP_GPU_TIMER");
            dlclose(library);
            return 28;
        }
        alarm(60);
        if (initialize == 2) {
            /* SDL subsystem initialization stays on the window's main thread.
             * A missing audio backend is an AudioOut error, not a fake port. */
            bool (*init_subsystem)(uint32_t)=(void *)dlsym(library,"SDL_InitSubSystem");
            const char *(*audio_driver)(void)=(void *)dlsym(library,"SDL_GetCurrentAudioDriver");
            BootstrapAudioBackend audio={
                .open=(void *)dlsym(library,"SDL_OpenAudioDeviceStream"),
                .resume=(void *)dlsym(library,"SDL_ResumeAudioStreamDevice"),
                .destroy=(void *)dlsym(library,"SDL_DestroyAudioStream"),
                .device=(void *)dlsym(library,"SDL_GetAudioStreamDevice"),
                .device_format=(void *)dlsym(library,"SDL_GetAudioDeviceFormat"),
                .put=(void *)dlsym(library,"SDL_PutAudioStreamData"),
                .queued=(void *)dlsym(library,"SDL_GetAudioStreamQueued"),
                .flush=(void *)dlsym(library,"SDL_FlushAudioStream"),
                .paused=(void *)dlsym(library,"SDL_AudioDevicePaused"),
                .available=(void *)dlsym(library,"SDL_GetAudioStreamAvailable"),
                .error=(void *)dlsym(library,"SDL_GetError"),
            };
            if(init_subsystem && init_subsystem(0x10)) {
                audio.initialized_subsystems=0x10;
                audio.driver=audio_driver ? audio_driver() : NULL;
            }
            if(bootstrap_audio_bind(&audio))
                fputs("AudioOut: SDL playback backend unavailable; initialization will report an error\n",stderr);
            void (*register_kernel)(void) = (void *)dlsym(library, "bbgpu_register_kernel");
            game_gpu_library = library;
            game_gpu_fault = (void *)dlsym(library, "bbgpu_handle_fault");
            game_gpu_window = window_loop;
            register_kernel();
        }
        const int result = initialize_gpu(&config);
        if (result) {
            fprintf(stderr, "STOP_GPU_INIT: renderer returned %d\n", result);
            fflush(NULL);
            _Exit(28); /* Initialization may have started threads: do not unload. */
        }
        if (initialize == 2) {
            /* SDL_WasInit is not thread-safe: snapshot readiness on this main
             * thread before the guest starts. Sampling uses thread-safe SDL APIs. */
            uint32_t (*was_init)(uint32_t) = (void *)dlsym(library, "SDL_WasInit");
            BootstrapKeyboard input = {
                .keyboard = (void *)dlsym(library, "SDL_GetKeyboardState"),
                .scancode = (void *)dlsym(library, "SDL_GetScancodeFromName"),
                .captured = (void *)dlsym(library, "bbgpu_overlay_captures_input"),
            };
            input.initialized_subsystems = was_init ? was_init(UINT32_C(0x2020)) : 0;
            if (bootstrap_pad_bind(&input)) {
                fputs("STOP_INPUT_BACKEND: initialized SDL video/gamepad keyboard backend unavailable\n", stderr);
                fflush(NULL); _Exit(28);
            }
            BootstrapGamepad controller = {
                .devices=(void *)dlsym(library,"SDL_GetGamepads"),
                .free_devices=(void *)dlsym(library,"SDL_free"),
                .open=(void *)dlsym(library,"SDL_OpenGamepad"),
                .close=(void *)dlsym(library,"SDL_CloseGamepad"),
                .connected=(void *)dlsym(library,"SDL_GamepadConnected"),
                .name=(void *)dlsym(library,"SDL_GetGamepadName"),
                .axis=(void *)dlsym(library,"SDL_GetGamepadAxis"),
                .button=(void *)dlsym(library,"SDL_GetGamepadButton"),
                .touch=(void *)dlsym(library,"SDL_GetGamepadTouchpadFinger"),
                .rumble=(void *)dlsym(library,"SDL_RumbleGamepad"),
                .led=(void *)dlsym(library,"SDL_SetGamepadLED"),
            };
            if (bootstrap_pad_bind_gamepad(&controller)) {
                fputs("STOP_INPUT_BACKEND: incomplete SDL3 gamepad provider\n",stderr);
                fflush(NULL); _Exit(28);
            }
            int controller_count = 0;
            uint32_t *controller_ids = controller.devices(&controller_count);
            printf("Input: SDL3 gamepads visible at startup=%d; hotplug enabled\n", controller_count);
            if (!controller_count)
                puts("Input: no physical gamepad detected; connect by Bluetooth or a USB data cable");
            controller.free_devices(controller_ids);
            uintptr_t (*resolve)(const char *) = (void *)dlsym(library, "bbgpu_resolve");
            if (bootstrap_graphics_bind(resolve)) {
                fputs("STOP_GPU_IMPORTS: incomplete provider or existing CPU queues\n", stderr);
                fflush(NULL);
                _Exit(28);
            }
            puts("GPU: renderer and scoped graphics/queue provider ready; no game frame verified.");
            return 0; /* Keep the library resident while guest/GPU threads run. */
        }
        puts("PASS: renderer initialization returned; no game commands submitted or game frame verified.");
        puts("Close the diagnostic window to exit (60-second maximum).");
        fflush(NULL);
        window_loop();
        /* The pinned event loop exits the process. Keep a future returning
         * implementation from unloading code still used by renderer workers. */
        fflush(NULL);
        _Exit(0);
    }
    if (dlclose(library)) {
        fprintf(stderr, "STOP_GPU_UNLOAD: %s\n", dlerror());
        return 28;
    }
    if (failed) return 28;
    puts("PASS: native GPU library load, six API exports and unload; renderer not initialized, no game frame.");
    return 0;
}

int bootstrap_gpu_load_check(const char *path) { return gpu_check(path, 0, 0); }
int bootstrap_gpu_renderer_check(const char *path) { return gpu_check(path, 1, 0); }
int bootstrap_gpu_start(const char *path, uint32_t sdk) { return gpu_check(path, 2, sdk); }
