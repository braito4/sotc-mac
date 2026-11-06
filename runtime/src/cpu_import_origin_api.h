#ifndef SOTC_CPU_IMPORT_ORIGIN_API_H
#define SOTC_CPU_IMPORT_ORIGIN_API_H
#include <stdint.h>
/* Diagnostic continuation, not an unwound caller or an inferred CALL instruction. */
typedef struct {
    uint64_t active,cpu_pc,rsp,target,guest_instruction,guest_return;
    unsigned return_status; /* 0 not applicable, 1 unreadable, 2 not guest code, 3 registered guest code */
} SotcImportOrigin;
__attribute__((weak)) int sotc_cpu_import_origin_v1(uint64_t cpu_pc,uint64_t rsp,SotcImportOrigin *out);
#endif
