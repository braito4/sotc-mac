// Original checked integration of real PS4 shader metadata with the native runtime.
// Invalid speculative work is retried on the owning GPU thread, never compiled as
// a substitute shader. A real missing program still stops with private evidence.
#pragma once
#include "gpu_shader_metadata.h"
#include "gpu_failure_history.h"
#include "gpu_shader_provenance.h"
#include "gpu_shader_lookup_trace.h"
#include "gpu_shader_prefix_bridge.h"
#include "gpu_shader_origin_cache.h"
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
    SotcShaderProvenance::Lookup(program,result,required,address);
    TraceShaderLookup(program,result,role,required,address);
    if(result.found && runtime_memory_is_mapped(address,result.info.length)) {
        return Shader::ShaderParams{.user_data=program.user_data,
            .code=std::span{reinterpret_cast<const u32*>(address),result.info.length/sizeof(u32)},
            .hash=result.info.shader_hash};
    }
    if(!required) {
        static std::atomic<unsigned> reports{};
        if(reports.fetch_add(1,std::memory_order_relaxed)<8)
            std::fprintf(stderr,"GPU_SHADER_LOOKUP_RETRY: %.*s; speculative metadata unavailable; retry on GPU thread\n",int(role.size()),role.data());
        return std::nullopt;
    }
    char thread[64]{};
    // Failure-only ordinary CPU-object snapshot, before any host-history hook.
    // This is separate from the earlier lookup-address observation, not atomic.
    static_assert(sizeof(Program)>=8 && sizeof(Program)<=512);
    std::array<std::byte,sizeof(Program)> descriptor{};
    std::memcpy(descriptor.data(),&program,descriptor.size());
    std::uint64_t thread_id{};const auto self=pthread_self();
    static_assert(sizeof(self)<=sizeof(thread_id));std::memcpy(&thread_id,&self,sizeof(self));
    pthread_getname_np(pthread_self(),thread,sizeof(thread));
    std::fprintf(stderr,"STOP_GPU_SHADER_METADATA: role=%.*s thread=%s address=%#llx mapped=%zu head=%08x,%08x hint=%llu scanned=%zu found=%d length=%u metadata_offset=%zu association=%s\n",
        int(role.size()),role.data(),thread,static_cast<unsigned long long>(address),mapped,
        result.head[0],result.head[1],static_cast<unsigned long long>(result.hint),result.scanned,
        int(result.found),unsigned(result.info.length),result.offset,ShaderMetadataAssociationName(result.association));
    PreserveCpuFailureHistory();
    if(SotcShaderProvenance::Enabled())
        SotcShaderProvenance::Get().Report(stderr,reinterpret_cast<std::uintptr_t>(&program));
    // Evidence is private, bounded, and created only for an actual failed bind.
    // Lookup failure is preserved; no fake hash, length or pipeline is returned.
    if(const char* directory=std::getenv("BB_GPU_USER_DIR")) {
        std::error_code error;
        const auto folder=std::filesystem::path(directory)/"shader-metadata-failure";
        const bool created=std::filesystem::create_directory(folder,error);
        if(created && !error) {
            const auto watch_snapshot=SotcShaderPrefixBridge::Snapshot(address);
            std::ofstream watch(folder/"prefix-watch.txt");
            watch<<"schema=1\naddress="<<address<<"\nstate="<<watch_snapshot.state<<'\n';
            static constexpr std::array<const char*,8> names={"watched_address","packet","capture_id","registered_ns",
                "last_readable_available","last_readable_value","last_readable_ns","stamp"};
            for(std::size_t i=0;i<names.size();++i)watch<<names[i]<<'='<<watch_snapshot.values[i]<<'\n';
            const auto submission_snapshot=SotcShaderOriginCache::Find(address);
            std::ofstream submission(folder/"submission-prefix.txt");
            submission<<"schema=1\naddress="<<address<<"\nstate="<<submission_snapshot.state
                <<"\ntotal="<<submission_snapshot.total<<"\nreplacements="<<submission_snapshot.replacements
                <<"\ncapacity="<<SotcShaderOriginCache::Slots<<"\nways="<<SotcShaderOriginCache::Ways
                <<"\nbucket_population="<<submission_snapshot.bucket_population
                <<"\nmatches="<<submission_snapshot.count<<'\n';
            for(std::size_t i=0;i<submission_snapshot.count;++i) {
                const auto& item=submission_snapshot.matches[i];const auto prefix="match"+std::to_string(i)+"_";
                submission<<prefix<<"address="<<item.address<<'\n'<<prefix<<"packet="<<item.packet
                    <<'\n'<<prefix<<"raw="<<item.raw<<'\n'<<prefix<<"head="<<item.head
                    <<'\n'<<prefix<<"flags="<<item.flags<<'\n'<<prefix<<"capture_id="<<item.capture
                    <<'\n'<<prefix<<"sampled_ns="<<item.sampled_ns<<'\n'<<prefix<<"context="<<item.context
                    <<'\n'<<prefix<<"physical="<<item.physical<<'\n'<<prefix<<"stamp="<<item.stamp<<'\n';
            }
            const auto& snapshot=result.scan_snapshot;
            const auto bytes=snapshot.size();
            std::ofstream data(folder/"code.bin",std::ios::binary);
            if(bytes) data.write(reinterpret_cast<const char*>(snapshot.data()),snapshot.size());
            std::ofstream raw(folder/"descriptor.bin",std::ios::binary);
            raw.write(reinterpret_cast<const char*>(descriptor.data()),descriptor.size());raw.close();
            std::ofstream meta(folder/"context.txt");
            meta<<"role="<<role<<"\nthread="<<thread<<"\naddress="<<address
                <<"\nmapped="<<mapped<<"\nhint="<<result.hint<<"\nscanned="<<result.scanned
                <<"\nassociation="<<ShaderMetadataAssociationName(result.association)<<"\ncandidate_found="<<result.candidate_found
                <<"\nproducer_pid="<<getpid()<<"\ndescriptor_bytes="<<descriptor.size()
                <<"\nprogram="<<reinterpret_cast<std::uintptr_t>(&program)<<"\nthread_id="<<thread_id
                <<"\ndescriptor_layout="<<(SotcShaderProvenance::AddressOffset<Program>() ? "compute_address40_at48_shift8" : "address40_shift8")
                <<"\ndescriptor_address_offset="<<SotcShaderProvenance::AddressOffset<Program>()
                <<"\ndescriptor_snapshot_atomic=0"
                <<"\nsnapshot_from_scan=1\nscan_head_matches_initial="
                <<(bytes>=sizeof(result.head) ? int(!std::memcmp(snapshot.data(),result.head.data(),sizeof(result.head))) : -1)<<'\n';
            if(SotcShaderProvenance::Enabled()) {
                if(auto* report=std::fopen((folder/"provenance.txt").c_str(),"w")) {
                    SotcShaderProvenance::Get().Report(report,reinterpret_cast<std::uintptr_t>(&program));
                    std::fclose(report);
                }
            }
            if(data && raw && meta && watch && submission) std::fprintf(stderr,"GPU shader lookup evidence: %s\n",folder.c_str());
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
