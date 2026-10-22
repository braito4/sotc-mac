// SPDX-License-Identifier: GPL-2.0-or-later
// One binding preparation only. Storage remains owned by the staging pool.
#pragma once
#include <cstdint>
#include <optional>
#include <utility>

namespace SotcBindingCopy {
using U64 = std::uint64_t;
struct Stamp {
    U64 generation{}, tick{}, physical{}, mapping_end{};
    bool operator==(const Stamp&) const = default;
};

template <class BufferPointer> class Scope {
public:
    using Result = std::pair<BufferPointer, U64>;
    // Called for every slot, including special/direct bindings that do not copy.
    void NextBinding(U64 slot) { current_slot = slot; }

    template <class Observe, class Copy>
    Result Obtain(U64 address, U64 size, bool written, bool formatted,
                  Observe&& observe, Copy&& copy, bool& shared) {
        shared = false;
        const auto before = !written && !formatted && address && size
                                ? observe() : std::optional<Stamp>{};
        if (before && valid && current_slot > last_slot && current_slot - last_slot == 1 &&
            address == last_address && size == last_size && *before == last_stamp) {
            last_slot = current_slot;
            shared = true;
            return last_result;
        }
        // A miss, write, unsupported source or exception must not revive old data.
        valid = false;
        const Result result = copy();
        if (before && result.first && before == observe()) {
            last_slot = current_slot;
            last_address = address;
            last_size = size;
            last_stamp = *before;
            last_result = result;
            valid = true;
        }
        return result;
    }
private:
    U64 current_slot{}, last_slot{}, last_address{}, last_size{};
    Stamp last_stamp{};
    Result last_result{};
    bool valid{};
};
} // namespace SotcBindingCopy
