// Opt-in timings of owned Metal command buffers; no waits, fences or GPU synchronization.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace SotcMetalTiming {
inline bool Enabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("SOTC_METAL_TIMING");
        return value && value[0] == '1';
    }();
    return enabled;
}
struct Stats {
    uint64_t valid{}, invalid{}, commands{};
    double gpu{}, gpu_max{}, queue{}, queue_max{}, admission{}, callback{};
    void Add(double submit, double start, double end, double completed,
             double admission_seconds, uint64_t count) {
        if (!std::isfinite(submit) || !std::isfinite(start) || !std::isfinite(end) ||
            !std::isfinite(completed) || !std::isfinite(admission_seconds) ||
            submit <= 0 || start < submit || end < start || completed < end ||
            admission_seconds < 0) {
            ++invalid;
            return;
        }
        ++valid; commands += count;
        gpu += end - start; gpu_max = std::max(gpu_max, end - start);
        queue += start - submit; queue_max = std::max(queue_max, start - submit);
        admission += admission_seconds; callback += completed - end;
    }
};
inline void Complete(double submit, double start, double end, double completed,
                     double admission, uint64_t commands) {
    static std::mutex mutex;
    static Stats stats;
    static double last = 0;
    Stats snapshot;
    double window = 0;
    {
        std::lock_guard lock(mutex);
        if (!last) last = completed;
        stats.Add(submit, start, end, completed, admission, commands);
        if (completed - last < 5) return;
        window = completed - last;
        last = completed;
        snapshot = stats;
        stats = {};
    }
    const double n = snapshot.valid ? double(snapshot.valid) : 1;
    std::printf("Metal timing: window_s=%.3f valid=%llu invalid=%llu commands_per_buffer=%.1f "
                "gpu_avg_ms=%.3f gpu_max_ms=%.3f precommit_to_gpu_avg_ms=%.3f "
                "precommit_to_gpu_max_ms=%.3f admission_avg_ms=%.3f callback_lag_avg_ms=%.3f; "
                "scope=owned_completed_buffers not_frame_time_or_utilization\n",
                window, (unsigned long long)snapshot.valid, (unsigned long long)snapshot.invalid,
                double(snapshot.commands)/n, snapshot.gpu*1000/n, snapshot.gpu_max*1000,
                snapshot.queue*1000/n, snapshot.queue_max*1000, snapshot.admission*1000/n,
                snapshot.callback*1000/n);
}
} // namespace SotcMetalTiming
