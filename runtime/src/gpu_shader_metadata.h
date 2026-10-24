// Original bounded shader-metadata lookup. No guest code or GPU is embedded.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>
#include <utility>

namespace Sotc {
inline constexpr std::size_t ShaderMetadataScanBytes = 0x4000 * sizeof(std::uint32_t);
inline constexpr std::size_t ShaderCodeLimit = (1u << 24) - 1;
enum class ShaderMetadataAssociation : unsigned {
    Unavailable=0, Bound=1, MissingPrologue=2, InvalidHint=3, InvalidMetadata=4
};
inline const char* ShaderMetadataAssociationName(ShaderMetadataAssociation value) {
    switch(value) {
    case ShaderMetadataAssociation::Bound:return "bound_to_prologue";
    case ShaderMetadataAssociation::MissingPrologue:return "unbound_missing_prologue";
    case ShaderMetadataAssociation::InvalidHint:return "unbound_invalid_hint";
    case ShaderMetadataAssociation::InvalidMetadata:return "unbound_hint_metadata";
    default:return "unavailable";
    }
}

template<class Info> struct ShaderMetadataResult {
    bool found{};
    bool candidate_found{};
    ShaderMetadataAssociation association{ShaderMetadataAssociation::Unavailable};
    Info info{};
    std::size_t offset{};
    std::uint64_t hint{};
    std::array<std::uint32_t, 2> head{};
    std::size_t scanned{};
    std::vector<std::byte> scan_snapshot;
};

// read(offset, destination, bytes) must copy only the supplied mapped range.
// Metadata is copied, never accessed through an unaligned struct pointer.
template<class Info, class Reader>
ShaderMetadataResult<Info> FindShaderMetadata(std::size_t mapped, Reader&& read) {
    ShaderMetadataResult<Info> result;
    if(mapped < sizeof(result.head) || !read(0, result.head.data(), sizeof(result.head))) return result;
    // Only a self-describing prologue can bind a main-program origin to this
    // metadata layout. A signature elsewhere in the mapping is evidence only.
    result.association=ShaderMetadataAssociation::MissingPrologue;
    if(result.head[0] == 0xBEEB03FFu) {
        // Promote before addition/multiplication: the old u32 expression wrapped.
        result.hint=(std::uint64_t(result.head[1])+1)*8;
        result.association=ShaderMetadataAssociation::InvalidHint;
        if(result.hint <= mapped && sizeof(Info) <= mapped-result.hint) {
            result.association=ShaderMetadataAssociation::InvalidMetadata;
            Info info{};
            if(read(std::size_t(result.hint), &info, sizeof(info)) && info.Valid()) {
                result.candidate_found=true;result.info=info;result.offset=std::size_t(result.hint);
                // Code includes the eight-byte prologue and cannot overlap its
                // own metadata. Do not infer a new origin from a later signature.
                if(info.length>=sizeof(result.head) && !(info.length&3) &&
                   info.length<=mapped && info.length<=result.hint) {
                    result.found=true;result.association=ShaderMetadataAssociation::Bound;
                    return result;
                }
            }
        }
    }
    const auto bytes=std::min(mapped, ShaderMetadataScanBytes+sizeof(Info)-sizeof(std::uint32_t));
    std::vector<std::byte> snapshot(bytes);
    if(!read(0, snapshot.data(), bytes)) return result;
    result.scan_snapshot=std::move(snapshot);
    result.scanned=bytes;
    for(std::size_t offset=0;offset<ShaderMetadataScanBytes && offset+sizeof(Info)<=bytes;offset+=sizeof(std::uint32_t)) {
        Info info{};
        std::memcpy(&info,result.scan_snapshot.data()+offset,sizeof(info));
        if(!result.candidate_found && info.Valid()) {
            result.candidate_found=true;result.info=info;result.offset=offset;
        }
    }
    return result;
}
} // namespace Sotc
