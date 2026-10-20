/* Original synthetic backing/alias checks; no game content. */
#include "platform.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void) {
    assert(setenv("SOTC_MEMORY_BACKING", "invalid", 1) == 0);
    errno = 0; assert(bb_shared_memory("fixture", 16384) == -1 && errno == EINVAL);
    assert(setenv("SOTC_MEMORY_BACKING", "file", 1) == 0);
    errno = 0; assert(bb_shared_memory("fixture", UINT64_MAX) == -1 && errno == EINVAL);
    const size_t size = (size_t)sysconf(_SC_PAGESIZE) * 2;
    const int fd = bb_shared_memory("fixture", size); assert(fd >= 0);
    struct stat st; assert(fstat(fd, &st) == 0);
    assert(S_ISREG(st.st_mode) && (st.st_mode & 0777) == 0600 && st.st_nlink == 0 && st.st_size == (off_t)size);
    assert(fcntl(fd, F_GETFD) & FD_CLOEXEC);
    unsigned char *a = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    unsigned char *b = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(a != MAP_FAILED && b != MAP_FAILED && a != b);
    assert(close(fd) == 0); /* Views retain the unlinked object. */
    for (size_t i = 0; i < size; ++i) assert(a[i] == 0 && b[i] == 0);
    a[0] = 0x53; a[size - 1] = 0xa7; b[size / 2] = 0xc1;
    assert(b[0] == 0x53 && b[size - 1] == 0xa7 && a[size / 2] == 0xc1);
    bb_shared_memory_zero(-1, a, 0, size);
    for (size_t i = 0; i < size; ++i) assert(a[i] == 0 && b[i] == 0);
    assert(munmap(a, size) == 0 && munmap(b, size) == 0);
    assert(setenv("TMPDIR", "/no/such/sotc-fixture-directory", 1) == 0);
    errno = 0; assert(bb_shared_memory("fixture", size) == -1 && errno == ENOENT);
    puts("PASS: temporary file backing, mode/errors, unlinked private descriptor, close-on-exec, zero initialization and bidirectional shared aliases after close");
    return 0;
}
