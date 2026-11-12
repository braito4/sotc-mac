// SPDX-License-Identifier: GPL-2.0-or-later
// On-demand host imports, adapted from bloodborne-runtime b093fad8.
// The caller supplies deferred retirement and page-table rebinding. No eviction,
// guest copy, range truncation, permission change or synthetic completion here.
#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace SotcPoolRegions {
using Size = std::uint64_t;
inline constexpr Size Piece = 16ull << 20;
struct Span { Size start, end; };
inline std::optional<Span> Align(Size physical, Size bytes, Size pool) {
    if (!bytes || physical >= pool || bytes > pool - physical) return std::nullopt;
    const Size end = physical + bytes;
    const Size padding = (Piece - end % Piece) % Piece;
    return Span{physical - physical % Piece, end + std::min(padding, pool - end)};
}
template<class Buffer> struct State {
    struct Region { Size end; std::unique_ptr<Buffer> buffer; };
    std::recursive_mutex mutex;
    std::map<Size, Region> regions;
    std::map<Size, Size> block_base;
    Size pool_size{}, imported{}, merges{}, reported{};
    unsigned char* base{};

    // Caller holds mutex for combined table operations. Recursive for Acquire/At.
    std::pair<const Buffer*, Size> At(Size physical) {
        std::lock_guard lock(mutex);
        auto it = regions.upper_bound(physical);
        if (it == regions.begin()) return {nullptr, 0};
        --it;
        if (physical >= it->second.end) return {nullptr, 0};
        return {it->second.buffer.get(), it->first};
    }
    template<class Factory, class Retire, class Rewrite>
    std::pair<const Buffer*, Size> Acquire(Size physical, Size bytes,
                                          Factory factory, Retire retire, Rewrite rewrite) {
        std::lock_guard lock(mutex);
        const auto span = Align(physical, bytes, pool_size);
        if (!span) return {nullptr, 0};
        auto [start, end] = *span;
        auto it = regions.upper_bound(start);
        if (it != regions.begin()) {
            const auto previous = std::prev(it);
            if (previous->second.end >= end) return {previous->second.buffer.get(), previous->first};
            if (previous->second.end > start) it = previous;
        }
        std::vector<decltype(it)> replaced;
        for (; it != regions.end() && it->first < end; ++it) {
            start = std::min(start, it->first);
            end = std::max(end, it->second.end);
            replaced.push_back(it);
        }
        auto buffer = factory(start, end - start);
        if (!buffer) return {nullptr, 0}; // existing imports/table untouched
        const Buffer* result = buffer.get();
        Size old_bytes = 0;
        std::vector<std::unique_ptr<Buffer>> old;
        for (auto previous : replaced) {
            old_bytes += previous->second.end - previous->first;
            old.push_back(std::move(previous->second.buffer));
            regions.erase(previous);
        }
        regions.emplace(start, Region{end, std::move(buffer)});
        imported += (end - start) - old_bytes;
        if (!old.empty()) {
            ++merges;
            // Table aliases switch to the same physical bytes. Old objects remain
            // valid for prepared/queued commands through the caller's retire fence.
            rewrite(start, end, *result);
            for (auto& previous : old) retire(std::move(previous));
        }
        return {result, start};
    }
    void Forget(Size first_block, Size last_block) {
        std::lock_guard lock(mutex);
        block_base.erase(block_base.lower_bound(first_block), block_base.upper_bound(last_block));
    }
};
} // namespace SotcPoolRegions
