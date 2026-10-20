#ifndef SOTC_BOOTSTRAP_CONTRACTS_H
#define SOTC_BOOTSTRAP_CONTRACTS_H
#include <stdint.h>
#include <stddef.h>
void bootstrap_initialize(uint64_t capabilities);
uintptr_t bootstrap_resolve(const char *name, int is_data);
uintptr_t bootstrap_clocks_resolve(const char *name);
uintptr_t bootstrap_fibers_resolve(const char *name);
void bootstrap_fibers_report(void);
int bootstrap_fibers_active(void);
uintptr_t bootstrap_modules_resolve(const char *name);
uintptr_t bootstrap_discmap_resolve(const char *name);
uintptr_t bootstrap_dialogs_resolve(const char *name);
void bootstrap_dialogs_report(void);
uintptr_t bootstrap_event_flags_resolve(const char *name);
void bootstrap_event_flags_report(void);
unsigned bootstrap_event_flag_waiters(uintptr_t handle);
size_t bootstrap_event_flag_objects(void);
int bootstrap_mount_game_root(const char *root);
void bootstrap_unmount_game_root(void);
uintptr_t bootstrap_files_resolve(const char *canonical);
void bootstrap_files_report(void);
int bootstrap_files_mounted(void);
void bootstrap_finalize(void *dso);
void bootstrap_report(void);
void bootstrap_set_linked_libc(int enabled);
void bootstrap_set_procparam(void *param);
void bootstrap_set_module_tls(uint64_t module, const void *data, uint64_t filesz, uint64_t memsz);
void *bootstrap_tls_get_addr(const uint64_t *index);
void bootstrap_tls_attach(uint64_t *tcb, uint64_t static_size);
void bootstrap_tls_freeze(void);
void bootstrap_tls_cleanup(void);
size_t bootstrap_tls_live_blocks(void);
size_t bootstrap_threads_live(void);
#endif
