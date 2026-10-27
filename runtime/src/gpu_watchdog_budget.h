#pragma once
#include <cstdlib>
namespace SotcGpuWatchdog {
// Bounded diagnostic override; malformed/out-of-range values preserve the default.
inline unsigned Parse(const char* value){
    if(!value || !*value)return 10;
    unsigned n=0;
    for(const char* p=value;*p;++p){if(*p<'0'||*p>'9')return 10;n=n*10+unsigned(*p-'0');if(n>120)return 10;}
    return n>=10?n:10;
}
inline unsigned Seconds(){static const unsigned n=Parse(std::getenv("SOTC_GPU_WATCHDOG_SECONDS"));return n;}
}
