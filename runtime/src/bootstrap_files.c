/* Read-only game-content subset of bloodborne-native/bloodborne-runtime
 * 3170535f59cadbe5ce9c38e6bb961ed5fd215194/src/runtime_file.c.
 * The unmodified backend owns descriptors, PS4 stat layouts and case matching.
 * Only /app0 and /hostapp are mounted; no writable or save mounts are created.
 * Foundations: deadinside28/bbport and shadPS4.
 */
#include "bootstrap_contracts.h"
#include "buffer_page_history_api.h"
#include "runtime.h"
#ifdef SOTC_SAVE_TRIAL
#include "bootstrap_savedata_trial.h"
#endif
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
    if(result>0 && sotc_buffer_page_cpu_v1)
        sotc_buffer_page_cpu_v1(positional?SOTC_PAGE_FILE_PREAD:SOTC_PAGE_FILE_READ,
            (uintptr_t)buffer,(uint64_t)result,(uintptr_t)fd,positional?(uint64_t)offset:UINT64_MAX);
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
#ifdef SOTC_SAVE_TRIAL
void bootstrap_files_lock(void) { pthread_mutex_lock(&file_gate); }
void bootstrap_files_unlock(void) { pthread_mutex_unlock(&file_gate); }
static int64_t checked_open(const char *p,int f,int m) {
    if (p && !strncmp(p,"/savedata",9)) {
        if (!(f&3) && (f&(0x8|0x200|0x400|0x800))) return -EINVAL;
        int error=sotc_save_path_check(p,(f&(3|0x8|0x200|0x400|0x800))!=0);
        if (error) return error;
    }
    int64_t fd=runtime_file_open(p,f,m);
    if (fd>=3) {
        sotc_save_fd_opened((int)fd,p,f);
        /* Creating/truncating an empty save is itself a write to commit. */
        if (sotc_save_fd_writable((int)fd) && (f&(0x200|0x400)))
            (void)runtime_file_write((int)fd,"",0);
    }
    return fd;
}
static int64_t checked_close(int fd) {
    int64_t r=runtime_file_close(fd);
    sotc_save_fd_closed(fd); return r;
}
static int64_t checked_stat(const char *p,void *st) {
    if (p && !strncmp(p,"/savedata",9)) { int error=sotc_save_path_check(p,0); if (error) return error; }
    return runtime_file_stat(p,st);
}
static int64_t checked_write(int fd,const void *b,uint64_t n) {
    if (!sotc_save_fd_writable(fd)) return -EBADF;
    if (n>(uint64_t)SSIZE_MAX) return -EINVAL;
    if (!b && n) return -EFAULT;
    return runtime_file_write(fd,b,n);
}
/* Positional operations use the backend's kernel wrappers; decode to raw errno
 * is unnecessary here because these functions already return guest errors. */
static int64_t saved_pwrite(int fd,const void *b,uint64_t n,int64_t offset) {
    pthread_mutex_lock(&file_gate);
    int64_t r;
    if (!sotc_save_fd_writable(fd)) r=kernel_read_result(-EBADF);
    else if (offset<0 || n>(uint64_t)SSIZE_MAX || n>(uint64_t)(INT64_MAX-offset)) r=kernel_read_result(-EINVAL);
    else if (!b && n) r=kernel_read_result(-EFAULT);
    else {
        int64_t (*fn)(int,const void *,uint64_t,int64_t)=(void *)runtime_file_resolve("nKWi-N2HBV4#p#J");
        r=fn ? fn(fd,b,n,offset) : kernel_read_result(-ENOSYS);
    }
    pthread_mutex_unlock(&file_gate); return r;
}
static int64_t saved_fsync(int fd) {
    pthread_mutex_lock(&file_gate);
    int64_t (*fn)(int)=(void *)runtime_file_resolve("fTx66l5iWIA#p#J");
    int64_t r=!sotc_save_fd_writable(fd) ? kernel_read_result(-EBADF) : fn ? fn(fd) : kernel_read_result(-ENOSYS);
    pthread_mutex_unlock(&file_gate); return r;
}
static int64_t saved_ftruncate(int fd,int64_t length) {
    pthread_mutex_lock(&file_gate);
    int64_t (*fn)(int,int64_t)=(void *)runtime_file_resolve("VW3TVZiM4-E#p#J");
    int64_t r=!sotc_save_fd_writable(fd) ? kernel_read_result(-EBADF) : length<0 ? kernel_read_result(-EINVAL) :
        fn ? fn(fd,length) : kernel_read_result(-ENOSYS);
    pthread_mutex_unlock(&file_gate); return r;
}
static int64_t saved_mkdir(const char *path,int mode) {
    pthread_mutex_lock(&file_gate);
    int error=sotc_save_path_check(path,1);
    int64_t (*fn)(const char *,int)=(void *)runtime_file_resolve("1-LFLmRFxxM#p#J");
    int64_t r=error ? kernel_read_result(error) : fn ? fn(path,mode) : kernel_read_result(-ENOSYS);
    pthread_mutex_unlock(&file_gate); return r;
}
static int64_t saved_posix_result(int64_t r) {
    if (r<0) { *runtime_errno()=(int)(r&0xffff); return -1; } return r;
}
static int64_t posix_saved_pwrite(int fd,const void *b,uint64_t n,int64_t off) { return saved_posix_result(saved_pwrite(fd,b,n,off)); }
static int64_t posix_saved_ftruncate(int fd,int64_t n) { return saved_posix_result(saved_ftruncate(fd,n)); }
static int64_t posix_saved_mkdir(const char *p,int mode) { return saved_posix_result(saved_mkdir(p,mode)); }
static int64_t posix_saved_fsync(int fd) { return saved_posix_result(saved_fsync(fd)); }
#endif
#define FILE_PAIR(name, params, args, call) \
    static int64_t raw_##name params { \
        pthread_mutex_lock(&file_gate); \
        const int64_t result = call args; \
        pthread_mutex_unlock(&file_gate); return result; \
    } \
    static int64_t kernel_##name params { return kernel_read_result(raw_##name args); } \
    static int64_t posix_##name params { return posix_read_result(raw_##name args); }
#ifdef SOTC_SAVE_TRIAL
FILE_PAIR(open, (const char *p, int f, int m), (p, f, m), checked_open)
FILE_PAIR(close, (int fd), (fd), checked_close)
FILE_PAIR(write, (int fd,const void *b,uint64_t n), (fd,b,n), checked_write)
#else
FILE_PAIR(open, (const char *p, int f, int m), (p, f, m), runtime_file_open)
FILE_PAIR(close, (int fd), (fd), runtime_file_close)
#endif
FILE_PAIR(seek, (int fd, int64_t offset, int whence), (fd, offset, whence), runtime_file_lseek)
#ifdef SOTC_SAVE_TRIAL
FILE_PAIR(stat, (const char *p, void *s), (p, s), checked_stat)
#else
FILE_PAIR(stat, (const char *p, void *s), (p, s), runtime_file_stat)
#endif
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
#ifdef SOTC_SAVE_TRIAL
    if (sotc_savedata_trial_enabled()) {
        if (!strcmp(symbol,"sceKernelWrite")) return (uintptr_t)kernel_write;
        if (!strcmp(symbol,"write") || !strcmp(symbol,"_write")) return (uintptr_t)posix_write;
        if (!strcmp(symbol,"sceKernelPwrite")) return (uintptr_t)saved_pwrite;
        if (!strcmp(symbol,"pwrite")) return (uintptr_t)posix_saved_pwrite;
        if (!strcmp(symbol,"sceKernelFtruncate")) return (uintptr_t)saved_ftruncate;
        if (!strcmp(symbol,"ftruncate")) return (uintptr_t)posix_saved_ftruncate;
        if (!strcmp(symbol,"sceKernelMkdir")) return (uintptr_t)saved_mkdir;
        if (!strcmp(symbol,"mkdir")) return (uintptr_t)posix_saved_mkdir;
        if (!strcmp(symbol,"sceKernelFsync")) return (uintptr_t)saved_fsync;
        if (!strcmp(symbol,"fsync")) return (uintptr_t)posix_saved_fsync;
    }
#endif
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
