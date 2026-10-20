/* Read-only game-content subset of bloodborne-native/bloodborne-runtime
 * 3170535f59cadbe5ce9c38e6bb961ed5fd215194/src/runtime_file.c.
 * The unmodified backend owns descriptors, PS4 stat layouts and case matching.
 * Only /app0 and /hostapp are mounted; no writable or save mounts are created.
 * Foundations: deadinside28/bbport and shadPS4.
 */
#include "bootstrap_contracts.h"
#include "runtime.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int mounted;
int bootstrap_files_mounted(void) { return mounted; }
/* Serialize the native descriptor table's complete operations, including
 * close versus read and its diagnostic counters. These CPU-only probes use
 * regular read-only content; parallel asynchronous I/O is not claimed. */
static pthread_mutex_t file_gate = PTHREAD_MUTEX_INITIALIZER;
static int64_t read_content(int fd, void *buffer, uint64_t size, int64_t offset, int positional) {
    if (fd < 3) return -EBADF;
    if (size > (uint64_t)SSIZE_MAX || (positional && offset < 0)) return -EINVAL;
    if (!buffer && size) return -EFAULT;
    pthread_mutex_lock(&file_gate);
    const int64_t result = positional ? runtime_file_pread(fd, buffer, size, offset) : runtime_file_read(fd, buffer, size);
    pthread_mutex_unlock(&file_gate);
    return result;
}
static int64_t kernel_read_result(int64_t result) {
    return result < 0 ? (int64_t)(int32_t)(UINT32_C(0x80020000) | (uint32_t)runtime_guest_errno((int)-result)) : result;
}
static int64_t posix_read_result(int64_t result) {
    if (result >= 0) return result;
    *runtime_errno() = runtime_guest_errno((int)-result);
    return -1;
}
static int64_t kernel_read(int fd, void *buffer, uint64_t size) { return kernel_read_result(read_content(fd, buffer, size, 0, 0)); }
static int64_t posix_read(int fd, void *buffer, uint64_t size) { return posix_read_result(read_content(fd, buffer, size, 0, 0)); }
static int64_t kernel_pread(int fd, void *buffer, uint64_t size, int64_t offset) { return kernel_read_result(read_content(fd, buffer, size, offset, 1)); }
static int64_t posix_pread(int fd, void *buffer, uint64_t size, int64_t offset) { return posix_read_result(read_content(fd, buffer, size, offset, 1)); }
#define FILE_PAIR(name, params, args, call) \
    static int64_t raw_##name params { \
        pthread_mutex_lock(&file_gate); \
        const int64_t result = call args; \
        pthread_mutex_unlock(&file_gate); return result; \
    } \
    static int64_t kernel_##name params { return kernel_read_result(raw_##name args); } \
    static int64_t posix_##name params { return posix_read_result(raw_##name args); }
FILE_PAIR(open, (const char *p, int f, int m), (p, f, m), runtime_file_open)
FILE_PAIR(close, (int fd), (fd), runtime_file_close)
FILE_PAIR(seek, (int fd, int64_t offset, int whence), (fd, offset, whence), runtime_file_lseek)
FILE_PAIR(stat, (const char *p, void *s), (p, s), runtime_file_stat)
FILE_PAIR(fstat, (int fd, void *s), (fd, s), runtime_file_fstat)
FILE_PAIR(getdents, (int fd, char *p, uint64_t n, int64_t *base), (fd, p, n, base), runtime_file_getdents)
#undef FILE_PAIR
static int32_t reachability(const char *path) { unsigned char info[120]; return (int32_t)kernel_stat(path, info); }
int bootstrap_mount_game_root(const char *root) {
    char canonical[PATH_MAX];
    struct stat info;
    if (mounted || !root || !realpath(root, canonical) || stat(canonical, &info) || !S_ISDIR(info.st_mode)) return -1;
    if (runtime_file_mount("/app0", canonical)) return -1;
    if (runtime_file_mount("/hostapp", canonical)) { runtime_file_unmount("/app0"); return -1; }
    mounted = 1;
    return 0;
}
void bootstrap_unmount_game_root(void) {
    runtime_file_unmount("/app0"); runtime_file_unmount("/hostapp"); mounted = 0;
}
uintptr_t bootstrap_files_resolve(const char *canonical) {
    if (!mounted) return 0;
    const char *symbol = runtime_symbol(canonical);
    if (!symbol) return 0;
    if (!strcmp(symbol, "sceKernelRead")) return (uintptr_t)kernel_read;
    if (!strcmp(symbol, "read") || !strcmp(symbol, "_read")) return (uintptr_t)posix_read;
    if (!strcmp(symbol, "sceKernelPread")) return (uintptr_t)kernel_pread;
    if (!strcmp(symbol, "pread")) return (uintptr_t)posix_pread;
    static const struct { const char *symbol; void *fn; } allowed[] = {
        {"sceKernelOpen", kernel_open}, {"open", posix_open}, {"_open", posix_open},
        {"sceKernelClose", kernel_close}, {"close", posix_close}, {"_close", posix_close},
        {"sceKernelLseek", kernel_seek}, {"lseek", posix_seek},
        {"sceKernelStat", kernel_stat}, {"stat", posix_stat},
        {"sceKernelFstat", kernel_fstat}, {"fstat", posix_fstat},
        {"sceKernelGetdirentries", kernel_getdents}, {"getdirentries", posix_getdents},
        {"sceKernelCheckReachability", reachability},
    };
    for (size_t i = 0; i < sizeof(allowed) / sizeof(*allowed); ++i)
        if (!strcmp(symbol, allowed[i].symbol)) return (uintptr_t)allowed[i].fn;
    return 0;
}
void bootstrap_files_report(void) {
    pthread_mutex_lock(&file_gate);
    if (mounted) runtime_file_report();
    pthread_mutex_unlock(&file_gate);
}

/* Error mapping copied from this native runtime's runtime_kernel.c. File APIs
 * must return PS4 errno values, not whichever numbers the host uses. */
int32_t runtime_guest_errno(int e) {
    switch (e) {
    case 0: return 0;
    case EPERM: return 1; case ENOENT: return 2; case ESRCH: return 3; case EINTR: return 4;
    case EIO: return 5; case ENXIO: return 6; case E2BIG: return 7; case ENOEXEC: return 8;
    case EBADF: return 9; case ECHILD: return 10; case EDEADLK: return 11; case ENOMEM: return 12;
    case EACCES: return 13; case EFAULT: return 14; case EBUSY: return 16; case EEXIST: return 17;
    case EXDEV: return 18; case ENODEV: return 19; case ENOTDIR: return 20; case EISDIR: return 21;
    case EINVAL: return 22; case ENFILE: return 23; case EMFILE: return 24; case ENOTTY: return 25;
    case EFBIG: return 27; case ENOSPC: return 28; case ESPIPE: return 29; case EROFS: return 30;
    case EMLINK: return 31; case EPIPE: return 32; case ERANGE: return 34; case EAGAIN: return 35;
    case ENAMETOOLONG: return 63; case ENOTEMPTY: return 66; case ETIMEDOUT: return 60;
    case ELOOP: return 62; case ENOSYS: return 78; case EOVERFLOW: return 84; case ECANCELED: return 85;
    default: return 5;
    }
}
