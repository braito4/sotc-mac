/* Temporary synthetic content only; never reads the user's game or saves. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "platform.h"
#include "runtime.h"
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

void bootstrap_test_files(void) {
    const char *open_name = "1G3lF1Gg1k8#0#F";
    assert(!bootstrap_resolve(open_name, 0));
    char root[] = "/tmp/sotc-readonly-XXXXXX", path[PATH_MAX];
    assert(mkdtemp(root));
    assert(snprintf(path, sizeof(path), "%s/MixedCase.bin", root) < (int)sizeof(path));
    int host = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    const unsigned char content[] = {'a', 'b', 'c', 0, 'd', 'e'};
    assert(host >= 0 && write(host, content, sizeof(content)) == (ssize_t)sizeof(content) && close(host) == 0);
    assert(bootstrap_mount_game_root(root) == 0);
    int64_t (*open_guest)(const char *, int, int) = (void *)bootstrap_resolve(open_name, 0);
    int64_t (*read_guest)(int, void *, uint64_t) = (void *)bootstrap_resolve("Cg4srZ6TKbU#0#F", 0);
    int64_t (*pread_guest)(int, void *, uint64_t, int64_t) = (void *)bootstrap_resolve("+r3rMFwItV4#0#F", 0);
    int64_t (*seek_guest)(int, int64_t, int) = (void *)bootstrap_resolve("oib76F-12fk#0#F", 0);
    int64_t (*fstat_guest)(int, void *) = (void *)bootstrap_resolve("kBwCPsYX-m4#0#F", 0);
    int64_t (*stat_guest)(const char *, void *) = (void *)bootstrap_resolve("eV9wAD2riIA#0#F", 0);
    int64_t (*stat_posix)(const char *, void *) = (void *)bootstrap_resolve("E6ao34wPw+U#p#F", 0);
    int64_t (*close_guest)(int) = (void *)bootstrap_resolve("UK2Tl2DWUns#0#F", 0);
    assert(open_guest && read_guest && pread_guest && seek_guest && fstat_guest && stat_guest && stat_posix && close_guest);
    assert(!bootstrap_resolve("1G3lF1Gg1k8#1#u", 0) && !bootstrap_resolve(open_name, 1));
    assert(!bootstrap_files_resolve("FxVZqBAA7ks#p#J"));
    const int flags[] = {1, 2, 0x8, 0x200, 0x400, 0x800};
    for (size_t i = 0; i < sizeof(flags) / sizeof(*flags); ++i)
        assert((uint32_t)open_guest("/app0/MixedCase.bin", flags[i], 0600) == UINT32_C(0x8002001e));
    assert((uint32_t)open_guest("/app0/../MixedCase.bin", 0, 0) == UINT32_C(0x8002000d));
    struct { uint64_t head; unsigned char data[120]; uint64_t tail; } info = {0xa123, {0}, 0xb456};
    int64_t length;
    assert(stat_guest("/hostapp/mixedcase.bin", info.data) == 0);
    memcpy(&length, info.data + 72, sizeof(length));
    assert(length == (int64_t)sizeof(content) && info.head == 0xa123 && info.tail == 0xb456);
    assert(stat_posix("/app0/missing.bin", info.data) == -1 && *runtime_errno() == 2);
    int fd = (int)open_guest("mixedcase.bin", 0, 0);
    assert(fd >= 3 && fstat_guest(fd, info.data) == 0);
    memcpy(&length, info.data + 72, sizeof(length));
    assert(length == (int64_t)sizeof(content));
    unsigned char buffer[8] = {0};
    unsigned char *bridge = runtime_low_map(16384, PROT_READ | PROT_WRITE);
    assert(bridge);
    bridge[0] = 0x48; bridge[1] = 0xb8;
    const uintptr_t target = (uintptr_t)read_guest;
    memcpy(bridge + 2, &target, sizeof(target)); bridge[10] = 0xff; bridge[11] = 0xe0;
    bbcpu_add_guest_code((uintptr_t)bridge, 16384);
    const uint64_t args[] = {(uint64_t)fd, (uintptr_t)buffer, 3};
    assert(bbcpu_call((uintptr_t)bridge, 3, args) == 3 && !memcmp(buffer, content, 3));
    assert(pread_guest(fd, buffer, 2, 3) == 2 && !memcmp(buffer, content + 3, 2));
    assert(seek_guest(fd, 0, SEEK_CUR) == 3);
    assert(seek_guest(fd, 0, SEEK_END) == (int64_t)sizeof(content) && read_guest(fd, buffer, 1) == 0);
    assert((uint32_t)read_guest(0, buffer, 1) == UINT32_C(0x80020009));
    assert((uint32_t)read_guest(fd, NULL, 1) == UINT32_C(0x8002000e));
    assert((uint32_t)read_guest(fd, buffer, UINT64_MAX) == UINT32_C(0x80020016));
    assert((uint32_t)pread_guest(fd, buffer, 1, -1) == UINT32_C(0x80020016));
    assert(close_guest(fd) == 0 && (uint32_t)read_guest(fd, buffer, 1) == UINT32_C(0x80020009));
    bootstrap_unmount_game_root();
    assert(!bootstrap_resolve(open_name, 0));
    host = open(path, O_RDONLY);
    assert(host >= 0 && read(host, buffer, sizeof(buffer)) == (ssize_t)sizeof(content));
    assert(!memcmp(buffer, content, sizeof(content)) && close(host) == 0);
    assert(unlink(path) == 0 && rmdir(root) == 0);
    puts("PASS: read-only content mount, x86-to-ARM64 reads, pread/seek/EOF, stat layout, guest errors and write rejection");
}
