// Synthetic metadata against the actual upstream BinaryInfo declaration.
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "binary_info_fixture.h"
#include "gpu_shader_metadata.h"

int main() {
    using Info=BinaryInfo;
    std::vector<std::byte> memory;
    unsigned out_of_bounds=0,reads=0;
    const auto lookup=[&]() {
        return Sotc::FindShaderMetadata<Info>(memory.size(),[&](std::size_t off,void* out,std::size_t n) {
            ++reads;
            if(off>memory.size() || n>memory.size()-off) {++out_of_bounds;return false;}
            std::memcpy(out,memory.data()+off,n);return true;
        });
    };
    Info info{};info.signature=Info::signature_ref;info.length=16;info.shader_hash=0x12345678;
    const auto put=[&](std::size_t off) {assert(off+sizeof(info)<=memory.size());std::memcpy(memory.data()+off,&info,sizeof(info));};
    memory.resize(128);put(32);
    const std::uint32_t head[]={0xBEEB03FFu,3};std::memcpy(memory.data(),head,sizeof(head));
    auto result=lookup();assert(result.found && result.offset==32 && result.info.shader_hash==info.shader_hash && reads==2);
    // Unaligned scanner candidates remain readable evidence, never a binding.
    memory.assign(80,std::byte{});put(4);result=lookup();assert(!result.found && result.candidate_found && result.offset==4);
    // The largest literal must not wrap into the code head or trigger a far read.
    memory.assign(128,std::byte{});put(32);
    const std::uint32_t bad_hint[]={0xBEEB03FFu,0xffffffffu};std::memcpy(memory.data(),bad_hint,sizeof(bad_hint));
    result=lookup();assert(!result.found && result.candidate_found && result.offset==32 && result.hint==0x800000000ull);
    assert(result.association==Sotc::ShaderMetadataAssociation::InvalidHint);
    for(unsigned n=0;n<8;++n) {memory.assign(n,std::byte{});assert(!lookup().found);}
    for(unsigned n=1;n<sizeof(info);++n) {
        memory.assign(16+n,std::byte{});std::memcpy(memory.data()+16,&info,n);assert(!lookup().found);
    }
    for(unsigned length : {0u,3u,4u,36u,132u}) {
        memory.assign(128,std::byte{});std::memcpy(memory.data(),head,sizeof(head));
        info.length=length;put(32);assert(!lookup().found);
    }
    info.length=16;
    // Preserve the old scan's final candidate as evidence, with its full trailer.
    memory.assign(Sotc::ShaderMetadataScanBytes+sizeof(info)-4,std::byte{});
    put(Sotc::ShaderMetadataScanBytes-4);result=lookup();assert(!result.found && result.candidate_found && result.offset==Sotc::ShaderMetadataScanBytes-4);
    memory.assign(Sotc::ShaderMetadataScanBytes+sizeof(info),std::byte{});
    put(Sotc::ShaderMetadataScanBytes);assert(!lookup().found);
    // A real prologue hint can locate a distant trailer without scanning its body.
    memory.assign((1u<<20)+sizeof(info),std::byte{});put(1u<<20);
    const std::uint32_t far_hint[]={0xBEEB03FFu,(1u<<17)-1};std::memcpy(memory.data(),far_hint,sizeof(far_hint));
    reads=0;result=lookup();assert(result.found && result.offset==(1u<<20) && reads==2);
    const auto unavailable=Sotc::FindShaderMetadata<Info>(128,[](auto,auto,auto){return false;});
    assert(!unavailable.found && !out_of_bounds);
    std::puts("PASS: actual BinaryInfo layout; bounded/unaligned metadata, malformed/truncated ranges, overflow-safe and distant prologue hints; no out-of-range reads or fabricated metadata");
}
