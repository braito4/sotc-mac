/* Private adapter for the pinned bbcpu snapshot, not a public bbcpu ABI. */
#ifndef SOTC_BOOTSTRAP_CPU_CONTEXT_H
#define SOTC_BOOTSTRAP_CPU_CONTEXT_H
#include "cpu_internal.h"
void bootstrap_cpu_save(BbCpu *out);
void bootstrap_cpu_restore(const BbCpu *saved);
int bootstrap_cpu_suspend(BbCpu *out);
uint64_t bootstrap_cpu_start(uintptr_t entry, uint64_t first, uint64_t second,
                             void *stack, size_t size, int depth, int reset_fpu, uint64_t *stop);
void bootstrap_cpu_resume(const BbCpu *saved, uint64_t stop);
void bootstrap_cpu_set_fpu(uint32_t mxcsr, uint16_t control, uint16_t status);
void bootstrap_cpu_thread_finished(void);
#endif
