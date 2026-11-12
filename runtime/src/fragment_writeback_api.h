#ifndef SOTC_FRAGMENT_WRITEBACK_API_H
#define SOTC_FRAGMENT_WRITEBACK_API_H
#include <stdint.h>
/* Versioned host/renderer ABI. A group contains at most two independent outputs. */
typedef struct SotcFragmentTransfer {
    uintptr_t address;
    void *data;
    uint64_t size;
    uint64_t generation;
} SotcFragmentTransfer;
#if defined(__cplusplus)
static_assert(sizeof(SotcFragmentTransfer)==32,"fragment transfer ABI requires 64-bit host");
#else
_Static_assert(sizeof(SotcFragmentTransfer)==32,"fragment transfer ABI requires 64-bit host");
#endif
#endif
