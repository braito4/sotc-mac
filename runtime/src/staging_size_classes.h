// Original conservative adaptation of power-of-two staging size classes.
#pragma once
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
namespace SotcStagingClasses {
constexpr uint64_t MiB=1024*1024;
constexpr uint64_t PaddingBudget=64*MiB;
inline uint64_t Select(uint64_t request,uint64_t legacy,bool enabled,
                       std::atomic<uint64_t>& charged) {
    // No change for rings, giant transfers, invalid/overflow-prone input, or control.
    if(!enabled || request<=16*MiB || request>64*MiB || legacy<request)return legacy;
    const uint64_t rounded=std::bit_ceil(request);
    if(rounded<=legacy)return legacy;
    const uint64_t extra=rounded-legacy;
    auto used=charged.load(std::memory_order_relaxed);
    do {
        if(used>PaddingBudget || extra>PaddingBudget-used)return legacy;
    }while(!charged.compare_exchange_weak(used,used+extra,std::memory_order_relaxed));
    return rounded;
}
inline uint64_t AllocationSize(uint64_t request,uint64_t legacy) {
    static const bool enabled=[] {
        const char* value=std::getenv("SOTC_STAGING_SIZE_CLASSES");
        const bool on=value && value[0]=='1' && value[1]=='\0';
        if(on)std::fprintf(stderr,"Staging size classes: enabled; cumulative extra padding limited to 64 MiB; requests above 64 MiB unchanged\n");
        return on;
    }();
    // A lifetime charge, never refunded: bounds added padding even if trimming is enabled.
    // This is not a limit on total staging memory or allocation pressure versus another run.
    static std::atomic<uint64_t> charged{};
    const auto selected=Select(request,legacy,enabled,charged);
    if(selected!=legacy) {
        static std::atomic<unsigned> reports{};
        if(reports.fetch_add(1,std::memory_order_relaxed)<8)
            std::fprintf(stderr,"Staging size class applied: request=%llu legacy=%llu allocated=%llu padding_charged=%llu\n",
                (unsigned long long)request,(unsigned long long)legacy,(unsigned long long)selected,
                (unsigned long long)charged.load(std::memory_order_relaxed));
    }
    return selected;
}
}
