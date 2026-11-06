#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
namespace SotcPageHistoryBridge {
inline void Dump(unsigned report,unsigned slot,unsigned node,std::uint64_t physical,std::uint64_t bytes) {
    static const bool enabled=[] {const char* s=std::getenv("SOTC_PAGE_HISTORY");return s&&!std::strcmp(s,"1");}();
    if(!enabled)return;
    using Fn=void(*)(unsigned,unsigned,unsigned,std::uint64_t,std::uint64_t);
    static const auto fn=[] {
        auto value=reinterpret_cast<Fn>(dlsym(RTLD_DEFAULT,"sotc_buffer_page_history_dump_v1"));
        if(!value)std::fprintf(stderr,"Page history unavailable: host export missing; CPU attribution absent\n");
        return value;
    }();
    if(fn)fn(report,slot,node,physical,bytes);
}
}
