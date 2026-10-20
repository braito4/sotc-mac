#ifndef SOTC_BOOTSTRAP_CONTRACTS_H
#define SOTC_BOOTSTRAP_CONTRACTS_H
#include <stdint.h>
void bootstrap_initialize(uint64_t capabilities);
uintptr_t bootstrap_resolve(const char *name, int is_data);
uintptr_t bootstrap_clocks_resolve(const char *name);
uintptr_t bootstrap_fibers_resolve(const char *name);
void bootstrap_fibers_report(void);
uintptr_t bootstrap_modules_resolve(const char *name);
int bootstrap_mount_game_root(const char *root);
void bootstrap_unmount_game_root(void);
uintptr_t bootstrap_files_resolve(const char *canonical);
void bootstrap_files_report(void);
void bootstrap_finalize(void *dso);
void bootstrap_report(void);
void bootstrap_set_linked_libc(int enabled);
void bootstrap_set_procparam(void *param);
void bootstrap_set_module_tls(uint64_t module, const void *data, uint64_t filesz, uint64_t memsz);
#endif
