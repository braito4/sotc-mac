// Original optional registration bridge. No allocation or guest reads here.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <array>
#include <dlfcn.h>
namespace SotcShaderPrefixBridge {
// Pool offset at this observation, not an allocation identity or lifetime token.
inline std::uint64_t Physical(std::uint64_t address) {
    const int saved=errno;
    using Hook=int(*)(std::uintptr_t,std::uint64_t*,std::uintptr_t*);
    static const auto hook=reinterpret_cast<Hook>(dlsym(RTLD_DEFAULT,"runtime_memory_pool_phys"));
    std::uint64_t physical=UINT64_MAX;std::uintptr_t end{};
    if(hook&&address<=UINTPTR_MAX-8) {
        std::uint64_t value{};
        if(hook(address,&value,&end)&&end>=address+8)physical=value;
    }
    errno=saved;return physical;
}
struct WatchSnapshot {int state{-1};std::array<std::uint64_t,8> values{};};
inline WatchSnapshot Snapshot(std::uint64_t address) {
    const int saved=errno;
    using Hook=int(*)(std::uint64_t,std::uint64_t*);
    static const auto hook=reinterpret_cast<Hook>(dlsym(RTLD_DEFAULT,"sotc_shader_prefix_watch_snapshot"));
    WatchSnapshot result;if(hook)result.state=hook(address,result.values.data());errno=saved;return result;
}
inline void Register(std::uint64_t address,std::uint64_t packet,std::uint64_t capture) {
    static const bool enabled=[]{const char *p=std::getenv("SOTC_SHADER_COPY_TRACE");return p&&!std::strcmp(p,"1");}();
    if(!enabled)return;
    const int saved=errno;
    using Hook=void(*)(std::uint64_t,std::uint64_t,std::uint64_t);
    static const auto hook=reinterpret_cast<Hook>(dlsym(RTLD_DEFAULT,"sotc_shader_prefix_watch"));
    if(hook)hook(address,packet,capture);
    errno=saved;
}
}
