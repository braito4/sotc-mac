/* Optional SotC save adapter. All file hooks run under bootstrap_files_lock. */
#ifndef SOTC_SAVEDATA_TRIAL_H
#define SOTC_SAVEDATA_TRIAL_H
#include <stdint.h>
#include <stddef.h>
uintptr_t sotc_savedata_trial_resolve(const char *name);
int sotc_savedata_trial_enabled(void);
void bootstrap_files_lock(void);
void bootstrap_files_unlock(void);
int sotc_save_path_check(const char *path, int writing);
int sotc_save_fd_writable(int fd);
void sotc_save_fd_opened(int fd, const char *path, int flags);
void sotc_save_fd_closed(int fd);
#endif
