// Original bounded shader-metadata lookup. No guest code or GPU is embedded.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace Sotc {
inline constexpr std::size_t ShaderMetadataScanBytes = 0x4000 * sizeof(std::uint32_t);
inline constexpr std::size_t ShaderCodeLimit = (1u << 24) - 1;

template<class Info> struct ShaderMetadataResult {
    bool found{};
    Info info{};
    std::size_t offset{};
    std::uint64_t hint{};
    std::array<std::uint32_t, 2> head{};
    std::size_t scanned{};
};

// read(offset, destination, bytes) must copy only the supplied mapped range.
// Metadata is copied, never accessed through an unaligned struct pointer.
template<class Info, class Reader>
ShaderMetadataResult<Info> FindShaderMetadata(std::size_t mapped, Reader&& read) {
    ShaderMetadataResult<Info> result;
    if(mapped < sizeof(result.head) || !read(0, result.head.data(), sizeof(result.head))) return result;
    const auto candidate = [&](std::size_t offset, const Info& info) {
        // GCN instructions occupy whole dwords. Do not invent a code length.
        if(!info.Valid() || !info.length || (info.length & 3) || info.length > mapped) return false;
        result.found=true; result.info=info; result.offset=offset;
        return true;
    };
    if(result.head[0] == 0xBEEB03FFu) {
        // Promote before addition/multiplication: the old u32 expression wrapped.
        result.hint=(std::uint64_t(result.head[1])+1)*8;
        if(result.hint <= mapped && sizeof(Info) <= mapped-result.hint) {
            Info info{};
            if(read(std::size_t(result.hint), &info, sizeof(info)) && candidate(std::size_t(result.hint), info)) return result;
        }
    }
    const auto bytes=std::min(mapped, ShaderMetadataScanBytes+sizeof(Info)-sizeof(std::uint32_t));
    std::vector<std::byte> snapshot(bytes);
    if(!read(0, snapshot.data(), bytes)) return result;
    result.scanned=bytes;
    for(std::size_t offset=0;offset<ShaderMetadataScanBytes && offset+sizeof(Info)<=bytes;offset+=sizeof(std::uint32_t)) {
        Info info{};
        std::memcpy(&info,snapshot.data()+offset,sizeof(info));
        if(candidate(offset,info)) return result;
    }
    return result;
}
} // namespace Sotc
