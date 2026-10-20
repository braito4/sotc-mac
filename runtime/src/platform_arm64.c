/* SotC platform adapter around unchanged pinned bloodborne-runtime sources.
 * Optional unlinked file backing preserves MAP_SHARED aliases when the host
 * disallows POSIX named shared-memory objects. No anonymous private-memory
 * substitution is made. Foundations: bloodborne-native, bbport and shadPS4.
 */
#define bb_shared_memory sotc_upstream_shared_memory
#include "../upstream/src/platform.c"
#undef bb_shared_memory
#include <limits.h>

int bb_shared_memory(const char *name, uint64_t size) {
    const char *mode = getenv("SOTC_MEMORY_BACKING");
    if (!mode || !strcmp(mode, "shm")) return sotc_upstream_shared_memory(name, size);
    if (strcmp(mode, "file") || size > INT64_MAX) { errno = EINVAL; return -1; }
    const char *directory = getenv("TMPDIR");
    if (!directory || !*directory) directory = "/tmp";
    char path[PATH_MAX];
    const int length = snprintf(path, sizeof(path), "%s/sotc-memory-XXXXXX", directory);
    if (length < 0 || (size_t)length >= sizeof(path)) { errno = ENAMETOOLONG; return -1; }
    const int fd = mkstemp(path);
    if (fd < 0) return -1;
    /* The descriptor alone owns the object. Never leave a named backing file
     * or expose it to children through exec. Preserve the actual error. */
    if (unlink(path)) {
        const int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) || ftruncate(fd, (off_t)size)) {
        const int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    return fd;
}
