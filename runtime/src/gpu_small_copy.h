// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Small compute transfers, adapted from bloodborne-native/bloodborne-runtime
// a03929b5d0d09c070dea48e37c600e6cbf46ae5a. Conservative alias admission for SotC.
#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace SotcSmallCopy {
using U64 = std::uint64_t;
using U32 = std::uint32_t;
constexpr U64 MaxBytes = 65536;
constexpr U32 MaxRegions = 1024;
enum class Source { Fill, SameBuffer, FreshAllocation, UnknownAlias };
enum class Reason { Ready, Empty, TooMany, TooLarge, Bounds, Alias, Overlap };
inline const char* Name(Reason r) {
    switch (r) {
    case Reason::Ready: return "ready";
    case Reason::Empty: return "empty";
    case Reason::TooMany: return "regions";
    case Reason::TooLarge: return "bytes";
    case Reason::Bounds: return "bounds";
    case Reason::Alias: return "unknown_alias";
    case Reason::Overlap: return "overlap";
    }
    return "unknown";
}
struct Row { U64 src, dst; U32 end, width; };
static_assert(sizeof(Row) == 24);
struct Plan {
    Reason reason{Reason::Empty};
    U64 bytes{};
    U32 threads{};
    std::vector<Row> rows;
};
inline bool Fits(U64 base, U64 offset, U64 bytes, U64 length) {
    return base <= length && offset <= length - base && bytes <= length - base - offset;
}

// Native object inequality is NOT proof of disjoint backing: imported buffers can alias.
// FreshAllocation is reserved for UpdateBuffer's just-created owned staging buffer.
// Other cross-object copies retain the old blit, independent of diagnostic registries.
template<class Region>
Plan Prepare(Source source, U64 src_base, U64 src_length, U64 dst_base,
             U64 dst_length, U32 count, const Region* regions, U64 limit = MaxBytes) {
    Plan p;
    auto reject = [&](Reason r) { p.reason = r; p.rows.clear(); p.threads = 0; return p; };
    if (!count || !regions || !limit) return p;
    if (count > MaxRegions) return reject(Reason::TooMany);
    if (source == Source::UnknownAlias) return reject(Reason::Alias);
    limit = std::min(limit, MaxBytes);
    using Span = std::pair<U64, U64>;
    std::vector<Span> writes, reads;
    writes.reserve(count);
    if (source == Source::SameBuffer) reads.reserve(count);
    p.rows.reserve(count);
    for (U32 i = 0; i < count; ++i) {
        const auto& r = regions[i];
        if (!r.size) continue;
        if (r.size > limit - p.bytes) return reject(Reason::TooLarge);
        if (!Fits(dst_base, r.dstOffset, r.size, dst_length) ||
            (source != Source::Fill && !Fits(src_base, r.srcOffset, r.size, src_length)))
            return reject(Reason::Bounds);
        const U64 a = source == Source::Fill ? 0 : src_base + r.srcOffset;
        const U64 b = dst_base + r.dstOffset;
        writes.push_back({b, b + r.size});
        if (source == Source::SameBuffer) reads.push_back({a, a + r.size});
        const U64 alignment = a | b | r.size;
        const U32 width = !(alignment & 15) ? 16 : !(alignment & 3) ? 4 : 1;
        p.bytes += r.size;
        p.threads += r.size / width; // bounded by MaxBytes, including byte copies
        p.rows.push_back({a, b, p.threads, width});
    }
    if (!p.bytes) return p;
    std::sort(writes.begin(), writes.end());
    for (std::size_t i = 1; i < writes.size(); ++i)
        if (writes[i].first < writes[i - 1].second) return reject(Reason::Overlap);
    for (const auto& r : reads) {
        auto it = std::upper_bound(writes.begin(), writes.end(), Span{r.first, UINT64_MAX});
        if ((it != writes.begin() && std::prev(it)->second > r.first) ||
            (it != writes.end() && it->first < r.second)) return reject(Reason::Overlap);
    }
    p.reason = Reason::Ready;
    return p;
}
} // namespace SotcSmallCopy
