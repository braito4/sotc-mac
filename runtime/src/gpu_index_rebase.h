#pragma once
#include <cstdint>

namespace SotcIndexRebase {
struct Plan {
    bool valid{}, copy{};
    std::uint64_t source{}, bytes{}, index_delta{};
};
// No CPU read of index data: copy the exact draw extent, padded for Metal blits.
constexpr Plan Make(std::uint64_t length, std::uint64_t base,
                    std::uint32_t first, std::uint32_t count, unsigned width) {
    if (width != 2 && width != 4) return {};
    const std::uint64_t advance = std::uint64_t(first) * width;
    const std::uint64_t bytes = std::uint64_t(count) * width;
    if (base > length || advance > length-base) return {};
    const auto at = base + advance;
    if (at % width || bytes > length-at) return {};
    constexpr std::uint64_t boundary = std::uint64_t{1} << 32;
    if (!count || (at < boundary && bytes <= boundary-at))
        return {true,false,at,bytes,0};
    const auto source = at & ~std::uint64_t{3};
    const auto delta = at-source;
    const auto padded = (delta+bytes+3) & ~std::uint64_t{3};
    if (padded >= boundary || padded > length-source) return {};
    return {true,true,source,padded,delta};
}
}

#ifdef __OBJC__
#import <Metal/Metal.h>
namespace SotcIndexRebase {
inline void Encode(id<MTLBlitCommandEncoder> encoder, id<MTLBuffer> source,
                   id<MTLBuffer> destination, std::uint64_t destination_offset,
                   const Plan& plan) {
    [encoder copyFromBuffer:source sourceOffset:plan.source
                  toBuffer:destination destinationOffset:destination_offset size:plan.bytes];
}
}
#endif
