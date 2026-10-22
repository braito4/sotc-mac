// Original native command admission control. No GPU calls or fence substitution.
#pragma once
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>

namespace SotcMetalInflight {
struct Stats {
    unsigned limit{}, active{}, peak{};
    std::uint64_t acquired{}, completed{}, abandoned{}, waits{}, wait_ns{};
};
class Gate {
    struct State {
        std::mutex mutex;
        std::condition_variable changed;
        Stats stats;
        explicit State(unsigned limit) { stats.limit = limit; }
    };
    std::shared_ptr<State> state;
public:
    class Ticket {
        friend class Gate;
        std::shared_ptr<State> state;
        std::atomic<bool> finished{true};
        void Finish(bool completed) noexcept {
            if (finished.exchange(true)) return;
            {
                std::lock_guard lock(state->mutex);
                --state->stats.active;
                if (completed) ++state->stats.completed;
                else ++state->stats.abandoned;
            }
            state->changed.notify_one();
        }
    public:
        ~Ticket() { Finish(false); }
        void Complete() noexcept { Finish(true); }
    };
    explicit Gate(unsigned limit) : state(std::make_shared<State>(limit)) {}
    std::shared_ptr<Ticket> Acquire() {
        // Allocate before reserving a slot, so allocation failure cannot leak capacity.
        auto ticket = std::make_shared<Ticket>();
        ticket->state = state;
        std::unique_lock lock(state->mutex);
        const auto start = std::chrono::steady_clock::now();
        const bool waited = state->stats.limit && state->stats.active >= state->stats.limit;
        if (waited) {
            ++state->stats.waits;
            state->changed.wait(lock, [&] { return state->stats.active < state->stats.limit; });
            state->stats.wait_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count();
        }
        ticket->finished.store(false);
        ++state->stats.active;
        ++state->stats.acquired;
        if (state->stats.active > state->stats.peak) state->stats.peak = state->stats.active;
        return ticket;
    }
    Stats Snapshot() const {
        std::lock_guard lock(state->mutex);
        return state->stats;
    }
};
inline unsigned ConfiguredLimit() {
    const char* value = std::getenv("SOTC_METAL_INFLIGHT");
    if (!value) return 8;
    unsigned parsed{};
    const auto end = value + std::strlen(value);
    const auto result = std::from_chars(value, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed > 64) {
        std::fputs("STOP_METAL_INFLIGHT_CONFIG: expected an integer from 0 to 64\n", stderr);
        std::fflush(stderr);
        std::_Exit(24);
    }
    return parsed;
}
inline Gate& Get() {
    static Gate gate(ConfiguredLimit());
    static const bool reported = [] {
        std::fprintf(stderr, "Metal inflight: limit=%u (0 disables admission limit); "
                     "slots release on native completion, not Vulkan timeline\n",
                     gate.Snapshot().limit);
        return true;
    }();
    (void)reported;
    return gate;
}
inline void Report(const char* reason) {
    const auto s = Get().Snapshot();
    std::fprintf(stderr, "Metal inflight: reason=%s limit=%u reserved_or_inflight=%u peak=%u "
                 "acquired=%llu completed=%llu abandoned=%llu waits=%llu wait_ms=%.3f\n",
                 reason, s.limit, s.active, s.peak,
                 (unsigned long long)s.acquired, (unsigned long long)s.completed,
                 (unsigned long long)s.abandoned, (unsigned long long)s.waits, s.wait_ns / 1e6);
}
inline std::shared_ptr<Gate::Ticket> Acquire() {
    auto ticket = Get().Acquire();
    const auto s = Get().Snapshot();
    if (s.waits == 1 || (s.waits && s.waits % 1024 == 0)) {
        // Report only once per selected wait count, even with concurrent producers.
        static std::atomic<std::uint64_t> last{};
        auto seen = last.load();
        if (seen != s.waits && last.compare_exchange_strong(seen, s.waits)) Report("admission");
    }
    return ticket;
}
} // namespace SotcMetalInflight
