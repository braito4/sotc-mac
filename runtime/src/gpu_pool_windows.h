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

inline std::vector<Window> Plan(Size bytes, bool split, bool legacy) {
    if (!bytes) return {};
    if (!split || bytes <= Maximum) return {{0, bytes}};
    std::vector<Window> result;
    Size start = 0;
    for (;;) {
        // A shorter prefix leaves a full 3.5 GiB window starting at 2 GiB.
        // This covers long contiguous bindings in the middle of the pool.
        const Size length = !legacy && start == 0 ? 2 * GiB + Overlap : Maximum;
        const Size size = std::min(length, bytes - start);
        result.push_back({start, size});
        if (size == bytes - start) break;
        // The loop continues only when bytes-start exceeds this increment.
        start += !legacy && start == 0 ? 2 * GiB : Step;
    }
    return result;
}
} // namespace SotcPoolWindows
