// Original conservative bounds for read-only shader buffer bindings.
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace SotcBufferRead {

// Every load is guarded by the same unsigned index < flat[limit_dword].
// Its exclusive byte end is at most scale * index + end_add. The index cap
// also proves that intermediate U32 address arithmetic cannot overflow.
// scale == 0 means no proof. This record is part of shader-cache metadata.
struct Bound {
    std::uint32_t limit_dword{};
    std::uint32_t scale{};
    std::uint32_t end_add{};
    std::uint32_t safe_index_max{};
};

inline std::optional<std::uint64_t> ByteEnd(const Bound& bound,
                                          std::span<const std::uint32_t> flat) {
    if (!bound.scale || !bound.limit_dword || bound.limit_dword >= flat.size())
        return std::nullopt;
    const std::uint64_t limit = flat[bound.limit_dword];
    // No load executes for an empty iteration domain. Bind one byte rather
    // than inventing a null descriptor or allocating the declared huge range.
    if (!limit) return 1;
    if (limit - 1 > bound.safe_index_max) return std::nullopt;
    return (limit - 1) * bound.scale + bound.end_add;
}

inline std::uint64_t BindingSize(const Bound& bound, std::span<const std::uint32_t> flat,
                                 std::uint64_t declared, bool written, bool formatted) {
    // Only the large raw read-only path is changed. Unknown proofs retain the
    // original range and its existing validation/copy/error behavior.
    if (written || formatted || declared < (std::uint64_t{64} << 20)) return declared;
    const auto end = ByteEnd(bound, flat);
    return end ? std::min(declared, *end) : declared;
}

} // namespace SotcBufferRead
