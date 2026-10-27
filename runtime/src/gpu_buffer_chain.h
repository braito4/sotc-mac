#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace SotcBufferChain {
struct ReuseSummary { unsigned busy{}, oversize{}; std::uint64_t tick{}, bound{}, requested{}; };
inline thread_local ReuseSummary reuse;
inline void Reject(bool busy, std::uint64_t tick, std::uint64_t bound, std::uint64_t requested) {
    if (busy) ++reuse.busy; else ++reuse.oversize;
    if (reuse.busy + reuse.oversize == 1) { reuse.tick=tick; reuse.bound=bound; reuse.requested=requested; }
}
inline bool Enabled() {
    static const bool value = [] { const char* s=std::getenv("SOTC_BUFFER_CHAIN");
        return s && std::strcmp(s,"1")==0; }();
    return value;
}
// Host metadata only. IDs may be reused: preserve sequence and lifecycle events.
// One bounded stream shared by native allocation, registration and submission sites.
inline void Note(const char* event, std::uintptr_t handle, std::uintptr_t object,
                 std::uint64_t bytes, std::uint64_t context=0, const char* name="",
                 int name_length=0) {
    if (!Enabled()) return;
    static std::atomic<std::uint64_t> sequence{};
    auto n=sequence.fetch_add(1,std::memory_order_relaxed);
    if(n>=8192) {
        if(n==8192) std::fprintf(stderr,"Buffer chain: LIMIT=8192; subsequent lifecycle events omitted\n");
        return;
    }
    auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    std::fprintf(stderr,"Buffer chain: seq=%llu ns=%lld event=%s handle=%llx object=%llx bytes=%llu context=%llu name=%.*s\n",
        (unsigned long long)n,(long long)ns,event,(unsigned long long)handle,
        (unsigned long long)object,(unsigned long long)bytes,(unsigned long long)context,
        name_length>80?80:name_length,name);
}
}
