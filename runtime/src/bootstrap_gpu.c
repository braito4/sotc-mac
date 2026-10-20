/* Isolated native loader/renderer prerequisites for bloodborne-runtime's GPU.
 * Load-only mode invokes no GPU API. Renderer mode initializes on the host main
 * thread and pumps its window, with a fixed budget. Neither mode reads game data,
 * patches an image, registers kernel queues, or submits game commands.
 */
#include "bootstrap_contracts.h"
#include "../upstream/gpu/bbgpu.h"
#include <dlfcn.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static void renderer_timeout(int signal_number) {
    (void)signal_number;
    static const char message[] = "STOP_RENDERER_TIMEOUT: 60-second diagnostic budget exhausted\n";
    (void)write(STDERR_FILENO, message, sizeof(message)-1);
    _Exit(21);
}
static int gpu_check(const char *path, int initialize) {
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
            .sdk_version = 0, .psf_attributes = 0, .width = 1280, .height = 720,
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
        const int result = initialize_gpu(&config);
        if (result) {
            fprintf(stderr, "STOP_GPU_INIT: renderer returned %d\n", result);
            fflush(NULL);
            _Exit(28); /* Initialization may have started threads: do not unload. */
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

int bootstrap_gpu_load_check(const char *path) { return gpu_check(path, 0); }
int bootstrap_gpu_renderer_check(const char *path) { return gpu_check(path, 1); }
