// Original checked integration of real PS4 shader metadata with the native runtime.
// Invalid speculative work is retried on the owning GPU thread, never compiled as
// a substitute shader. A real missing program still stops with private evidence.
#pragma once
#include "gpu_shader_metadata.h"
#include "video_core/amdgpu/regs_shader.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <pthread.h>
#include <string_view>
#include <execinfo.h>

extern "C" std::uint64_t runtime_memory_clamp(std::uintptr_t, std::uint64_t);
extern "C" int runtime_memory_is_mapped(std::uintptr_t, std::uint64_t);
extern "C" void runtime_memory_read_backing(std::uintptr_t, void*, std::uint64_t);

namespace Sotc {
template<class Program>
std::optional<Shader::ShaderParams> TryShaderParams(const Program& program,
                                                   std::string_view role, bool required) {
    const auto address=reinterpret_cast<std::uintptr_t>(program.template Address<const u32*>());
    const auto mapped=std::size_t(runtime_memory_clamp(address,ShaderCodeLimit+sizeof(AmdGpu::BinaryInfo)));
    const auto result=FindShaderMetadata<AmdGpu::BinaryInfo>(mapped,[&](std::size_t offset,void* out,std::size_t bytes) {
        if(offset>mapped || bytes>mapped-offset || address>UINTPTR_MAX-offset) return false;
        if(!runtime_memory_is_mapped(address+offset,bytes)) return false;
        runtime_memory_read_backing(address+offset,out,bytes);
        return true;
    });
    if(result.found && runtime_memory_is_mapped(address,result.info.length)) {
        return Shader::ShaderParams{.user_data=program.user_data,
            .code=std::span{program.template Address<const u32*>(),result.info.length/sizeof(u32)},
            .hash=result.info.shader_hash};
    }
    if(!required) {
        static std::atomic<unsigned> reports{};
        if(reports.fetch_add(1,std::memory_order_relaxed)<8)
            std::fprintf(stderr,"GPU_SHADER_LOOKUP_RETRY: %.*s; speculative metadata unavailable; retry on GPU thread\n",int(role.size()),role.data());
        return std::nullopt;
    }
    char thread[64]{};
    pthread_getname_np(pthread_self(),thread,sizeof(thread));
    std::fprintf(stderr,"STOP_GPU_SHADER_METADATA: role=%.*s thread=%s address=%#llx mapped=%zu head=%08x,%08x hint=%llu scanned=%zu\n",
        int(role.size()),role.data(),thread,static_cast<unsigned long long>(address),mapped,
        result.head[0],result.head[1],static_cast<unsigned long long>(result.hint),result.scanned);
    // Evidence is private, bounded, and created only for an actual failed bind.
    // Lookup failure is preserved; no fake hash, length or pipeline is returned.
    if(const char* directory=std::getenv("BB_GPU_USER_DIR")) {
        std::error_code error;
        const auto folder=std::filesystem::path(directory)/"shader-metadata-failure";
        const bool created=std::filesystem::create_directory(folder,error);
        if(created && !error) {
            const auto bytes=std::min(mapped,ShaderMetadataScanBytes);
            std::vector<char> snapshot(bytes);
            if(bytes) runtime_memory_read_backing(address,snapshot.data(),bytes);
            std::ofstream data(folder/"code.bin",std::ios::binary);
            if(bytes) data.write(snapshot.data(),snapshot.size());
            std::ofstream meta(folder/"context.txt");
            meta<<"role="<<role<<"\nthread="<<thread<<"\naddress="<<address
                <<"\nmapped="<<mapped<<"\nhint="<<result.hint<<"\nscanned="<<result.scanned<<'\n';
            if(data && meta) std::fprintf(stderr,"GPU shader lookup evidence: %s\n",folder.c_str());
            else std::fprintf(stderr,"GPU shader lookup evidence write failed\n");
        }
    }
    void* frames[24];const auto count=backtrace(frames,24);backtrace_symbols_fd(frames,count,2);
    std::fflush(nullptr);std::_Exit(23);
}

template<class Program>
Shader::ShaderParams RequireShaderParams(const Program& program,std::string_view role) {
    return *TryShaderParams(program,role,true);
}
} // namespace Sotc
