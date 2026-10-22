// Original bounded, private presentation evidence. CPU counters are asynchronous
// process totals; only a completed readback supplies pixel evidence.
#pragma once
#include "gpu_visual_pixels.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>

namespace SotcVisual {
inline bool Enabled() {
    static const bool enabled = [] { const char* p = std::getenv("SOTC_VISUAL_TRACE"); return p && std::strcmp(p, "1") == 0; }();
    return enabled;
}
inline std::uint64_t Now() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline std::string Quote(const char* value) {
    std::string out = "\"";
    if (value) for (std::size_t i = 0; value[i] && i < 2048; ++i) {
        const unsigned char c = value[i];
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 32) { char code[7]; std::snprintf(code, sizeof(code), "\\u%04x", c); out += code; }
        else out += char(c);
    }
    return out + '"';
}
enum Counter : unsigned { DrawRequests, MissingPipeline, EmptyDraws, ResourceBindFailed,
    BoundDraws, SceneDecisions, ReducedDecisions, PipelineCreateFailed, CounterCount };
inline constexpr const char* CounterNames[] = {"draw_requests", "missing_pipeline_draws", "empty_draws",
    "resource_bind_failures", "bound_draw_requests", "scene_decisions", "reduced_decisions", "pipeline_creation_failures"};
struct Frame {
    std::uint64_t id{}, ready_tick{};
    unsigned width{}, height{}, window_width{}, window_height{}, requested_width{}, requested_height{};
    bool reused{}, game{};
};
struct Ticket { Frame frame; std::uint64_t ordinal{}, requested_ns{}; };

// Pure policy used by the production recorder and CPU fixtures.
struct CapturePolicy {
    std::uint64_t interval_ns{5'000'000'000ull}, next_ns{};
    unsigned attempts{}, limit{24};
    bool pending{};
    const char* Reserve(std::uint64_t now, unsigned width, unsigned height,
                        std::uint64_t allocated, std::uint64_t recommended, std::uint64_t sample_age) {
        if (pending) return "in_flight";
        if (attempts >= limit) return "capture_limit";
        if (now < next_ns) return "interval";
        next_ns = now + interval_ns;
        if (!width || !height || std::uint64_t(width) * height > MaxReadbackBytes / 4) return "size_limit";
        if (!recommended || sample_age > 2'000'000'000ull) return "native_memory_unknown_or_stale";
        // Recommendation is not a hard limit; retain diagnostic headroom before
        // adding a dedicated <=16 MiB readback to an already pressured process.
        if (allocated >= recommended || recommended - allocated < (256ull << 20)) return "memory_headroom";
        ++attempts; pending = true; return nullptr;
    }
};

class Recorder {
public:
    explicit Recorder(CapturePolicy config = {}) : policy{config} {}
    void Count(Counter counter) { if (Enabled()) counters[counter].fetch_add(1, std::memory_order_relaxed); }
    bool NeedNativeMemory() {
        if (!Enabled()) return false;
        const auto now = Now(); auto old = memory_poll.load(std::memory_order_relaxed);
        return now - old >= 1'000'000'000ull && memory_poll.compare_exchange_strong(old, now);
    }
    void NativeMemory(std::uint64_t allocated, std::uint64_t recommended) {
        native_allocated.store(allocated); native_recommended.store(recommended); native_sample_ns.store(Now());
    }
    void StagingMemory(const std::array<std::uint64_t, 9>& values) {
        if (Enabled()) for (unsigned i = 0; i < values.size(); ++i) staging[i].store(values[i], std::memory_order_relaxed);
    }
    Frame BeginFrame(unsigned width, unsigned height, unsigned ww, unsigned wh, unsigned rw,
                     unsigned rh, std::uint64_t ready_tick, bool reused, bool game,
                     std::uint64_t vk_bytes, std::uint64_t image_bytes) {
        Frame frame{++sequence, ready_tick, width, height, ww, wh, rw, rh, reused, game};
        if (Enabled()) {
            vulkan_bytes.store(vk_bytes); images.store(image_bytes);
            Event("present_recorded", frame);
        }
        return frame;
    }
    void Event(const char* event, const Frame& frame, const char* detail = nullptr, int result = 0) {
        if (!Enabled()) return;
        std::lock_guard lock{mutex};
        auto out = Start(event, frame); out << ",\"detail\":" << Quote(detail) << ",\"result\":" << result;
        Write(out.str() + '}');
    }
    std::optional<Ticket> ReserveCapture(const Frame& frame, bool supported) {
        if (!Enabled() || !frame.game) return {};
        const char* env = std::getenv("SOTC_VISUAL_CAPTURE");
        if (!env || std::strcmp(env, "1")) return {};
        std::lock_guard lock{mutex};
        if (!Open()) return {};
        const auto now = Now(), sample = native_sample_ns.load();
        if (!supported && now >= policy.next_ns && !policy.pending) {
            policy.next_ns = now + policy.interval_ns;
            Write(Start("capture_skipped", frame).str() + ",\"reason\":\"unsupported_pixel_format\"}");
            return {};
        }
        if (!supported) return {};
        const auto age = sample ? (now >= sample ? now - sample : 0) : UINT64_MAX;
        const char* reason = policy.Reserve(now, frame.width, frame.height, native_allocated.load(),
                                           native_recommended.load(), age);
        if (reason) {
            if (std::strcmp(reason, "interval") && std::strcmp(reason, "in_flight") && std::strcmp(reason, "capture_limit"))
                Write(Start("capture_skipped", frame).str() + ",\"reason\":" + Quote(reason) + '}');
            return {};
        }
        Ticket ticket{frame, policy.attempts, now};
        Write(Start("capture_requested", frame).str() + ",\"capture\":" + std::to_string(ticket.ordinal) + '}');
        return ticket;
    }
    void CaptureFailed(const Ticket& ticket, const char* why) {
        if (!Enabled()) return;
        std::lock_guard lock{mutex}; policy.pending = false;
        Write(Start("capture_failed", ticket.frame).str() + ",\"reason\":" + Quote(why) + '}');
    }
    void CompleteCapture(const Ticket& ticket, std::span<const std::uint8_t> data, bool bgra) {
        if (!Enabled()) return;
        std::lock_guard lock{mutex};
        policy.pending = false;
        auto pixels = AnalyzePixels(data, ticket.frame.width, ticket.frame.height, bgra, &previous);
        if (pixels.rgb.empty()) { Write(Start("capture_failed", ticket.frame).str() + ",\"reason\":\"invalid_readback\"}"); return; }
        char name[64]; std::snprintf(name, sizeof(name), "frame-%06llu.ppm", (unsigned long long)ticket.ordinal);
        FILE* image = std::fopen((directory / name).c_str(), "wbx");
        bool saved = false;
        if (image) {
            const bool header = std::fprintf(image, "P6\n%u %u\n255\n", pixels.width, pixels.height) > 0;
            saved = header && std::fwrite(pixels.rgb.data(), 1, pixels.rgb.size(), image) == pixels.rgb.size();
            if (std::fclose(image)) saved = false;
        }
        if (!saved) { Write(Start("capture_failed", ticket.frame).str() + ",\"reason\":\"image_write_failed\"}"); return; }
        auto out = Start("pixels", ticket.frame);
        out << ",\"capture\":" << ticket.ordinal << ",\"file\":" << Quote(name)
            << ",\"source\":\"game_frame_before_window_overlay\",\"preview_width\":" << pixels.width
            << ",\"preview_height\":" << pixels.height << ",\"hash\":\"" << std::hex << pixels.hash << std::dec
            << "\",\"mean_luma\":" << pixels.mean_luma << ",\"luma_deviation\":" << pixels.luma_deviation
            << ",\"black_fraction\":" << pixels.black_fraction << ",\"near_black_sample\":" << int(pixels.near_black)
            << ",\"compared\":" << int(pixels.compared) << ",\"identical_preview\":" << int(pixels.identical)
            << ",\"mean_change\":" << pixels.mean_change << ",\"changed_fraction\":" << pixels.changed_fraction
            << ",\"readback_latency_ms\":" << (Now() - ticket.requested_ns) / 1e6 << ",\"tile_luma\":[";
        for (unsigned i = 0; i < 32; ++i) out << (i ? "," : "") << pixels.tile_luma[i];
        Write(out.str() + "]}"); previous = std::move(pixels);
    }
    void Failure(int status, std::int64_t code, const char* domain, const char* description) {
        if (!Enabled()) return;
        std::lock_guard lock{mutex};
        auto out = Start("native_command_error", Frame{.id = sequence.load()});
        out << ",\"status\":" << status << ",\"code\":" << code << ",\"domain\":" << Quote(domain)
            << ",\"description\":" << Quote(description) << ",\"frame_association\":\"latest_cpu_sequence_only\"";
        Write(out.str() + '}');
    }
private:
    std::ostringstream Start(const char* event, const Frame& frame) {
        const auto sample = native_sample_ns.load();
        std::ostringstream out; out.imbue(std::locale::classic()); out << std::fixed << std::setprecision(4);
        out << "{\"schema\":1,\"event\":" << Quote(event) << ",\"t_s\":" << (Now() - start) / 1e9
            << ",\"frame\":" << frame.id << ",\"ready_tick\":" << frame.ready_tick
            << ",\"width\":" << frame.width << ",\"height\":" << frame.height
            << ",\"window_width\":" << frame.window_width << ",\"window_height\":" << frame.window_height
            << ",\"requested_scene_width\":" << frame.requested_width << ",\"requested_scene_height\":" << frame.requested_height
            << ",\"reused\":" << int(frame.reused) << ",\"game\":" << int(frame.game)
            << ",\"counter_scope\":\"asynchronous_process_totals\",\"counters\":{";
        for (unsigned i = 0; i < CounterCount; ++i) out << (i ? "," : "") << Quote(CounterNames[i]) << ':' << counters[i].load();
        const auto now = Now();
        out << "},\"native_allocated_bytes\":" << native_allocated.load()
            << ",\"native_recommended_bytes\":" << native_recommended.load()
            << ",\"native_sample_age_ms\":" << (sample ? (now >= sample ? now - sample : 0) / 1e6 : -1.0)
            << ",\"vulkan_tracked_bytes\":" << vulkan_bytes.load() << ",\"image_bytes\":" << images.load()
            << ",\"staging_bytes\":[";
        for (unsigned i = 0; i < staging.size(); ++i) out << (i ? "," : "") << staging[i].load();
        out << ']'; return out;
    }
    bool Open() {
        if (file) return true;
        if (attempted) return false;
        attempted = true;
        const char* root = std::getenv("BB_GPU_USER_DIR");
        if (!root || !*root || !std::filesystem::path(root).is_absolute()) return false;
        directory = std::filesystem::path(root) / "visual";
        std::error_code error;
        if (!std::filesystem::create_directory(directory, error) || error) return false;
        file = std::fopen((directory / "frames.jsonl").c_str(), "wx");
        if (file) std::fprintf(stderr, "VISUAL_TRACE: %s; private game-frame evidence, captures require SOTC_VISUAL_CAPTURE=1\n", directory.c_str());
        return file != nullptr;
    }
    void Write(const std::string& line) {
        if (!Open()) return;
        // Preserve the <=24 completed sample records even after timeline rotation.
        if (line.find("\"event\":\"pixels\"") != std::string::npos) {
            if (!captures) captures = std::fopen((directory / "captures.jsonl").c_str(), "wx");
            if (captures) {
                std::fwrite(line.data(), 1, line.size(), captures);
                std::fputc('\n', captures); std::fflush(captures);
            }
        }
        // Two <=4 MiB journals retain recent events without unbounded disk growth.
        if (written + line.size() + 1 > (4u << 20)) {
            std::fclose(file); file = nullptr;
            std::error_code error;
            std::filesystem::remove(directory / "frames.previous.jsonl", error); error.clear();
            std::filesystem::rename(directory / "frames.jsonl", directory / "frames.previous.jsonl", error);
            if (error) return;
            file = std::fopen((directory / "frames.jsonl").c_str(), "wx"); written = 0;
            if (!file) return;
        }
        const auto count = std::fwrite(line.data(), 1, line.size(), file);
        const bool ok = count == line.size() && std::fputc('\n', file) != EOF && std::fflush(file) == 0;
        if (!ok) { std::fclose(file); file = nullptr; return; }
        written += line.size() + 1;
    }
    std::mutex mutex;
    std::array<std::atomic<std::uint64_t>, CounterCount> counters{};
    std::array<std::atomic<std::uint64_t>, 9> staging{}; // ring[3], dedicated[3], held dedicated[3]
    std::atomic<std::uint64_t> sequence{}, memory_poll{}, native_allocated{}, native_recommended{}, native_sample_ns{};
    std::atomic<std::uint64_t> vulkan_bytes{}, images{};
    CapturePolicy policy;
    Pixels previous;
    std::filesystem::path directory;
    FILE* file{};
    FILE* captures{};
    bool attempted{};
    std::size_t written{};
    const std::uint64_t start{Now()};
};
inline Recorder& Get() { static auto* recorder = new Recorder; return *recorder; }
inline void Count(Counter counter) { if (Enabled()) Get().Count(counter); }
} // namespace SotcVisual
