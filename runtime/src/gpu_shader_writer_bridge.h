// Original optional GPU write-intent bridge; no allocator assumptions or writes.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <dlfcn.h>
namespace SotcShaderWriterBridge {
inline void Binding(std::uint64_t address,std::uint64_t bytes,std::uint64_t shader,
        std::uint64_t slot,std::uint64_t declared,bool paged=false) {
    static const bool enabled=[]{const auto* p=std::getenv("SOTC_SHADER_COPY_TRACE");return p&&!std::strcmp(p,"1");}();
    if(!enabled)return;
    const int saved=errno;
    using Hook=void(*)(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
    static const auto classic_hook=reinterpret_cast<Hook>(dlsym(RTLD_DEFAULT,"sotc_shader_writable_binding"));
    static const auto paged_hook=reinterpret_cast<Hook>(dlsym(RTLD_DEFAULT,"sotc_shader_paged_writable_binding"));
    const auto hook=paged?paged_hook:classic_hook;
    if(hook)hook(address,bytes,shader,slot,declared);
    errno=saved;
}
inline void ImageBinding(std::uint64_t address,std::uint64_t bytes,std::uint64_t shader,
        std::uint64_t slot,std::uint64_t declared) {
    static const bool enabled=[]{const auto* p=std::getenv("SOTC_SHADER_COPY_TRACE");return p&&!std::strcmp(p,"1");}();
    if(!enabled)return;
    const int saved=errno;
    using Hook=void(*)(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
    static const auto hook=reinterpret_cast<Hook>(dlsym(RTLD_DEFAULT,"sotc_shader_image_writable_binding"));
    if(hook)hook(address,bytes,shader,slot,declared);
    errno=saved;
}
inline void ResolvedImageBinding(std::uint64_t address,std::uint64_t bytes,std::uint64_t shader,
        std::uint64_t slot,std::uint64_t declared) {
    static const bool enabled=[]{const auto* p=std::getenv("SOTC_SHADER_COPY_TRACE");return p&&!std::strcmp(p,"1");}();
    if(!enabled)return;
    const int saved=errno;
    using Hook=void(*)(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
    static const auto hook=reinterpret_cast<Hook>(dlsym(RTLD_DEFAULT,"sotc_shader_resolved_image_writable_binding"));
    if(hook)hook(address,bytes,shader,slot,declared);
    errno=saved;
}
inline void TargetBinding(std::uint64_t address,std::uint64_t bytes,std::uint64_t shader,
        std::uint64_t slot,std::uint64_t declared) {
    static const bool enabled=[]{const auto* p=std::getenv("SOTC_SHADER_COPY_TRACE");return p&&!std::strcmp(p,"1");}();
    if(!enabled)return;
    const int saved=errno;
    using Hook=void(*)(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
    static const auto hook=reinterpret_cast<Hook>(dlsym(RTLD_DEFAULT,"sotc_shader_target_writable_binding"));
    if(hook)hook(address,bytes,shader,slot,declared);
    errno=saved;
}
inline void Observe(std::uint64_t address,std::uint64_t size,const void* data,
        unsigned source,std::uint64_t sequence,std::uintptr_t caller) {
    static const bool enabled=[]{const auto* p=std::getenv("SOTC_SHADER_COPY_TRACE");return p&&!std::strcmp(p,"1");}();
    if(!enabled)return;
    const int saved=errno;
    using Hook=void(*)(std::uint64_t,const void*,std::uint64_t,unsigned,std::uint64_t,std::uintptr_t);
    static const auto hook=reinterpret_cast<Hook>(dlsym(RTLD_DEFAULT,"sotc_shader_gpu_write"));
    if(hook)hook(address,data,size,source,sequence,caller);
    errno=saved;
}
}
