/* Original synthetic x86/ARM64 fixture ABI; independent of game data. */
typedef unsigned long long TWord;
typedef struct {
    void *mutex, *condition;
    unsigned int ready, release;
} ThreadShared;
typedef struct {
    TWord tls_fn, self_fn, join_fn, exit_fn, errno_fn, lock_fn, unlock_fn, wait_fn, signal_fn;
    TWord open_fn, read_fn, pread_fn, close_fn, stat_fn;
    ThreadShared *shared;
    const char *path;
    TWord tag, explicit_exit, self, tcb, tls, dtv, initial_static, final_static, stack_pointer;
    TWord self_join_error, initial_module, initial_errno, reads, other_tls;
    unsigned int inherited_mxcsr, errors, dtv_valid;
} ThreadFixture;
