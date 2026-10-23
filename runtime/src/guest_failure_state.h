/* Original numeric host-import snapshot. No pointer chasing or guest layout. */
#ifndef SOTC_GUEST_FAILURE_STATE_H
#define SOTC_GUEST_FAILURE_STATE_H
#include <stdint.h>
typedef struct {
    uint64_t available, rip, fs_base, stack_top, thread, regs[16];
} SotcGuestFailureState;
/* Ordinary terminating import only; never initialize a CPU or decode a caller. */
int bootstrap_cpu_failure_state(SotcGuestFailureState *out);
#endif
