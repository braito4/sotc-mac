// Original bounded fallback-lookup evidence. Private CPU observations only.
#pragma once
#include "gpu_shader_provenance.h"
#include <filesystem>
#include <fstream>
#include <string_view>
#include <type_traits>

namespace Sotc {
struct ShaderLookupTraceKey {
    std::uint64_t address,hash,offset,length,head;
    bool operator==(const ShaderLookupTraceKey&) const = default;
};
inline std::mutex shader_lookup_trace_mutex;
inline std::array<ShaderLookupTraceKey,8> shader_lookup_trace_keys{};
inline std::size_t shader_lookup_trace_used{};
// First eight distinct observed candidates, not a cache or validated identities.
// Never re-read guest storage: code.bin is exactly the already captured scan.
template<class Program,class Result>
void TraceShaderLookup(const Program& program,const Result& result,
                       std::string_view role,bool required,std::uintptr_t address) noexcept {
    if(!SotcShaderProvenance::Enabled() || !result.candidate_found || !result.scanned) return;
    const char* directory=std::getenv("BB_GPU_USER_DIR");
    if(!directory || !*directory)return;
    static_assert(std::is_trivially_copyable_v<Program> && sizeof(Program)<=512);
    const ShaderLookupTraceKey key{address,result.info.shader_hash,result.offset,result.info.length,
        std::uint64_t(result.head[0])|(std::uint64_t(result.head[1])<<32)};
    auto& recorder=SotcShaderProvenance::Get();
    std::size_t slot{};
    {std::lock_guard lock(shader_lookup_trace_mutex);
     for(std::size_t i=0;i<shader_lookup_trace_used;++i)if(shader_lookup_trace_keys[i]==key) {recorder.LookupCaptureNote(11);return;}
     if(shader_lookup_trace_used==shader_lookup_trace_keys.size()) {recorder.LookupCaptureNote(12);return;}
     slot=shader_lookup_trace_used;shader_lookup_trace_keys[shader_lookup_trace_used++]=key;recorder.LookupCaptureNote(10);}
    try {
        constexpr std::size_t limit=65536+sizeof(result.info)-sizeof(std::uint32_t);
        if(result.scan_snapshot.empty() || result.scan_snapshot.size()>limit ||
           result.offset>result.scan_snapshot.size() || sizeof(result.info)>result.scan_snapshot.size()-result.offset) {
            recorder.LookupCaptureNote(13);return;
        }
        std::array<std::byte,sizeof(Program)> descriptor{};
        std::memcpy(descriptor.data(),&program,descriptor.size());
        std::uint64_t thread{};const auto self=pthread_self();
        static_assert(sizeof(self)<=sizeof(thread));std::memcpy(&thread,&self,sizeof(self));
        struct timespec now{};std::uint64_t capture_ns{};
        if(!clock_gettime(CLOCK_MONOTONIC,&now))capture_ns=std::uint64_t(now.tv_sec)*1000000000+now.tv_nsec;
        const auto root=std::filesystem::path(directory)/"shader-lookup-trace";
        std::error_code error;
        std::filesystem::create_directory(root,error);
        if(error || !std::filesystem::is_directory(std::filesystem::symlink_status(root,error)) || error) {
            recorder.LookupCaptureNote(13);return;
        }
        const auto folder=root/("case-"+std::to_string(slot));
        if(!std::filesystem::create_directory(folder,error) || error) {
            recorder.LookupCaptureNote(13);return;
        }
        std::ofstream data(folder/"code.bin",std::ios::binary);
        data.write(reinterpret_cast<const char*>(result.scan_snapshot.data()),result.scan_snapshot.size());
        data.close();
        std::ofstream raw(folder/"descriptor.bin",std::ios::binary);
        raw.write(reinterpret_cast<const char*>(descriptor.data()),descriptor.size());raw.close();
        std::ofstream metadata(folder/"metadata.bin",std::ios::binary);
        metadata.write(reinterpret_cast<const char*>(result.scan_snapshot.data()+result.offset),sizeof(result.info));metadata.close();
        std::array<std::uint32_t,2> scan_head{};
        const bool has_head=result.scan_snapshot.size()>=sizeof(scan_head);
        if(has_head)std::memcpy(scan_head.data(),result.scan_snapshot.data(),sizeof(scan_head));
        std::ofstream context(folder/"context.txt");
        context<<"schema=1\nproducer_pid="<<getpid()<<"\nthread="<<thread<<"\ncapture_ns="<<capture_ns
            <<"\nrole="<<role<<"\nrequired="<<required<<"\nprogram="
            <<reinterpret_cast<std::uintptr_t>(&program)<<"\norigin="<<address
            <<"\nshader="<<result.info.shader_hash<<"\nmetadata_offset="<<result.offset
            <<"\nhint="<<result.hint<<"\nscanned="<<result.scanned<<"\nlength="<<result.info.length
            <<"\nlookup_head0="<<result.head[0]<<"\nlookup_head1="<<result.head[1]
            <<"\nscan_head_available="<<has_head<<"\nscan_head_matches_lookup="
            <<(has_head ? int(scan_head==result.head) : -1)
            <<"\nscan_snapshot_exact=1\nmetadata_from_scan=1\nmetadata_bytes="<<sizeof(result.info)<<"\ndescriptor_bytes="<<descriptor.size()
            <<"\ndescriptor_snapshot_atomic=0\nassociation_validated=0\naccepted="<<result.found
            <<"\nassociation_status="<<unsigned(result.association)<<'\n';
        context.close();
        bool provenance_ok=false;
        if(auto* out=std::fopen((folder/"provenance.txt").c_str(),"w")) {
            recorder.Report(out,reinterpret_cast<std::uintptr_t>(&program));
            provenance_ok=std::fclose(out)==0;
        }
        if(data && raw && metadata && context && provenance_ok) {
            recorder.LookupCaptureNote(14);
            std::fprintf(stderr,"GPU shader lookup trace: case=%zu origin=%#llx metadata_offset=%zu length=%u; saved scanner input, association unvalidated\n",
                slot,(unsigned long long)address,result.offset,unsigned(result.info.length));
        } else recorder.LookupCaptureNote(13);
    } catch(...) {recorder.LookupCaptureNote(13);}
}
} // namespace Sotc
