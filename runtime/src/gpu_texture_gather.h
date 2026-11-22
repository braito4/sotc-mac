// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace SotcTextureGather {
using Bytes = std::uint64_t;

template<class Buffer> struct View {
    const Buffer* buffer;
    Bytes offset;
    Bytes available;
};

template<class Buffer> struct Part {
    const Buffer* buffer;
    Bytes source, destination, size;
};

// Resolve the complete upload before recording any commands. A backing run can
// cross several imports and an import can contain several disjoint backing runs.
template<class Buffer, class Mapping, class Import>
std::vector<Part<Buffer>> Plan(Bytes address, Bytes size, Mapping mapping, Import imported) {
    if (!size || ((address | size) & 3) || address > UINT64_MAX - size) return {};
    std::vector<Part<Buffer>> parts;
    for (Bytes done = 0; done < size;) {
        Bytes physical = 0;
        std::uintptr_t end = 0;
        const Bytes at = address + done;
        if (!mapping(at, &physical, &end) || end <= at) return {};
        Bytes remaining = std::min<Bytes>(end - at, size - done);
        if (physical > UINT64_MAX - remaining) return {};
        while (remaining) {
            const auto view = imported(physical);
            if (!view || !view->buffer) return {};
            const Bytes count = std::min(remaining, view->available);
            if (!count || ((view->offset | count) & 3) || view->offset > UINT64_MAX - count)
                return {};
            if (!parts.empty() && parts.back().buffer == view->buffer &&
                parts.back().source + parts.back().size == view->offset) {
                parts.back().size += count;
            } else {
                parts.push_back({view->buffer, view->offset, done, count});
            }
            remaining -= count;
            done += count;
            physical += count;
        }
    }
    return parts;
}
} // namespace SotcTextureGather
