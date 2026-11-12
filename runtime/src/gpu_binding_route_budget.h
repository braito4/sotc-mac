// SPDX-License-Identifier: GPL-2.0-or-later
// Bounded diagnostic admission only. No GPU work or guest-memory mutation.
#pragma once
#include <cstdint>
#include <mutex>
namespace SotcBindingRouteBudget {
using U64=std::uint64_t;
enum class Decision { Admit, Waiting, Exhausted };
struct Budget {
    std::mutex mutex;
    U64 warmup{},focused{},legacy{},waiting{};
    bool triggered{};
    Decision Take(U64 shader,U64 trigger) {
        std::lock_guard lock(mutex);
        if(!trigger) return legacy++<128 ? Decision::Admit : Decision::Exhausted;
        if(shader==trigger)triggered=true;
        if(triggered)return focused++<120 ? Decision::Admit : Decision::Exhausted;
        if(warmup<8){++warmup;return Decision::Admit;}
        ++waiting;return Decision::Waiting;
    }
};
} // namespace SotcBindingRouteBudget
