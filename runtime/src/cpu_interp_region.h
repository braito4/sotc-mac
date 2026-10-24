/* Original immutable, image-relative interpreter comparison range.
 * Parsing never reads guest memory or changes instructions/protections. */
#ifndef SOTC_CPU_INTERP_REGION_H
#define SOTC_CPU_INTERP_REGION_H
#include <stdint.h>

typedef struct { uint64_t begin, end; unsigned enabled; } SotcCpuInterpRegion;

static inline int sotc_cpu_region_hex(const char **cursor,uint64_t *value) {
    const char *p=*cursor;
    if(p[0]!='0' || p[1]!='x')return -1;
    p+=2;unsigned digits=0;uint64_t n=0;
    for(;;++p) {
        unsigned d;
        if(*p>='0' && *p<='9')d=(unsigned)(*p-'0');
        else if(*p>='a' && *p<='f')d=(unsigned)(*p-'a')+10;
        else if(*p>='A' && *p<='F')d=(unsigned)(*p-'A')+10;
        else break;
        if(++digits>16)return -1;
        n=(n<<4)|d;
    }
    if(!digits)return -1;
    *value=n;*cursor=p;return 0;
}
static inline int sotc_cpu_region_parse(const char *text,SotcCpuInterpRegion *out) {
    *out=(SotcCpuInterpRegion){0};
    if(!text)return 0;
    const char *p=text;uint64_t begin,end;
    if(sotc_cpu_region_hex(&p,&begin) || *p++!=':' ||
       sotc_cpu_region_hex(&p,&end) || *p || begin>=end ||
       end>UINT64_C(512)*1024*1024 || end-begin>65536)return -1;
    *out=(SotcCpuInterpRegion){begin,end,1};return 0;
}
static inline int sotc_cpu_region_contains(const SotcCpuInterpRegion *range,
                                          uint64_t rip,uint64_t base) {
    return range->enabled && rip>=base && rip-base>=range->begin && rip-base<range->end;
}
#endif
