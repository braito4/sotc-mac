#ifndef SOTC_BUFFER_PAGE_HISTORY_API_H
#define SOTC_BUFFER_PAGE_HISTORY_API_H
#include <stdint.h>
/* Optional diagnostic ABI. Content sampling, when enabled, uses only the backing
 * view under the VMA lock. No guest hooks or allocator substitutions. */
enum { SOTC_PAGE_MAP=1, SOTC_PAGE_UNMAP=2, SOTC_PAGE_MEMCPY=3,
       SOTC_PAGE_MEMSET=4, SOTC_PAGE_FILE_READ=5, SOTC_PAGE_FILE_PREAD=6,
       SOTC_PAGE_BACKING_WRITE=7, SOTC_PAGE_CPU_WRITE_NOTICE=8, SOTC_PAGE_ZERO=9 };
__attribute__((weak)) void sotc_buffer_page_cpu_v1(unsigned kind,uintptr_t guest,uint64_t bytes,
                                                  uintptr_t source,uint64_t aux);
__attribute__((weak)) void sotc_buffer_page_mapping_v1(unsigned kind,uintptr_t guest,
                                                      uint64_t physical,uint64_t bytes);
/* Used with the runtime VMA lock already held; translation is exact for this observation. */
__attribute__((weak)) void sotc_buffer_page_backing_v1(uintptr_t guest,uint64_t physical,
                                                      uint64_t bytes,uintptr_t source,uintptr_t caller);
#endif
