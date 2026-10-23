// SPDX-License-Identifier: GPL-2.0-or-later
// Original import geometry. No guest reads, resource eviction or GPU dependency.
#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace SotcPoolWindows {
using Size = std::uint64_t;
inline constexpr Size GiB = Size{1} << 30;
inline constexpr Size Overlap = GiB / 2;
inline constexpr Size Step = 3 * GiB;
inline constexpr Size Maximum = Step + Overlap;
struct Window { Size start, size; };

inline std::vector<Window> Plan(Size bytes, bool split, bool legacy,
                                bool previous_balanced = false) {
    if (!bytes) return {};
    if (!split || bytes <= Maximum) return {{0, bytes}};
    std::vector<Window> result;
    Size start = 0;
    for (;;) {
        // Keep a 512 MiB overlap and make room for middle-pool allocations
        // starting below 2 GiB. The 1.75 GiB origin also keeps the prior
        // 3 GiB range above 2 GiB, which the rejected 1.5 GiB origin lost.
        const Size middle_start = previous_balanced ? 2 * GiB : 7 * GiB / 4;
        const Size length = !legacy && start == 0 ? middle_start + Overlap : Maximum;
        const Size size = std::min(length, bytes - start);
        result.push_back({start, size});
        if (size == bytes - start) break;
        // The loop continues only when bytes-start exceeds this increment.
        start += !legacy && start == 0 ? middle_start : Step;
    }
    return result;
}
} // namespace SotcPoolWindows
