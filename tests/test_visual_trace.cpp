// Original synthetic pixels and policy; no renderer, game input or GPU.
#include "gpu_visual_trace.h"
#include <cassert>
#include <thread>
using namespace SotcVisual;
int main() {
    assert(AnalyzePixels({}, 0, 1, false).rgb.empty());
    assert(AnalyzePixels({}, UINT32_MAX, UINT32_MAX, false).rgb.empty());
    std::vector<std::uint8_t> data(640 * 360 * 4, 0);
    auto black = AnalyzePixels(data, 640, 360, false);
    assert(black.width == 320 && black.height == 180 && black.near_black && !black.compared);
    auto same = AnalyzePixels(data, 640, 360, false, &black);
    assert(same.compared && same.identical && same.changed_fraction == 0);
    for (std::size_t i = 0; i < data.size(); i += 4) data[i] = 255;
    auto red = AnalyzePixels(data, 640, 360, false, &black);
    assert(!red.near_black && !red.identical && red.changed_fraction == 1 && red.mean_luma == 54 * 255.0 / 256);
    auto blue = AnalyzePixels(data, 640, 360, true);
    assert(blue.rgb[0] == 0 && blue.rgb[2] == 255 && blue.mean_luma == 19 * 255.0 / 256);
    const std::array<std::uint8_t, 16> small{0,0,0,255, 255,255,255,255, 255,255,255,255, 0,0,0,255};
    assert(AnalyzePixels(small, 2, 2, false).luma_deviation > 120);
    CapturePolicy policy{.interval_ns = 10, .limit = 2};
    assert(std::strcmp(policy.Reserve(0, 640, 360, 0, 0, 0), "native_memory_unknown_or_stale") == 0);
    assert(std::strcmp(policy.Reserve(1, 640, 360, 0, 1ull<<30, 0), "interval") == 0);
    assert(std::strcmp(policy.Reserve(10, 640, 360, 0, 1ull<<30, 3'000'000'000ull), "native_memory_unknown_or_stale") == 0);
    assert(std::strcmp(policy.Reserve(20, 640, 360, 900ull<<20, 1ull<<30, 0), "memory_headroom") == 0);
    assert(std::strcmp(policy.Reserve(30, UINT32_MAX, UINT32_MAX, 0, 1ull<<30, 0), "size_limit") == 0);
    assert(policy.Reserve(40, 640, 360, 0, 1ull<<30, 0) == nullptr);
    assert(std::strcmp(policy.Reserve(100, 640, 360, 0, 1ull<<30, 0), "in_flight") == 0);
    policy.pending = false;
    assert(policy.Reserve(100, 640, 360, 0, 1ull<<30, 0) == nullptr);
    policy.pending = false;
    assert(std::strcmp(policy.Reserve(110, 640, 360, 0, 1ull<<30, 0), "capture_limit") == 0);
    Recorder recorder{CapturePolicy{.interval_ns = 0, .limit = 3}};
    recorder.NativeMemory(1024, 2ull << 30);
    recorder.StagingMemory({1,2,3,4,5,6,7,8,9}, {1,2,3,4,5,6}, {10,11,12});
    auto frame = recorder.BeginFrame(640, 360, 1920, 1080, 640, 360, 42, false, true, 2048, 1024);
    recorder.Event("present_result", frame, "quoted\"\\\n<script>", 1);
    if (!Enabled()) return 0;
    std::thread worker{[&] { for (unsigned i = 0; i < 100; ++i) recorder.Count(DrawRequests); }};
    for (unsigned i = 0; i < 100; ++i) recorder.Count(MissingPipeline);
    worker.join();
    auto ticket = recorder.ReserveCapture(frame, true); assert(ticket);
    recorder.CompleteCapture(*ticket, data, false);
    ticket = recorder.ReserveCapture(frame, true); assert(ticket);
    recorder.CompleteCapture(*ticket, data, false);
    ticket = recorder.ReserveCapture(frame, true); assert(ticket);
    recorder.CaptureFailed(*ticket, "synthetic_failure");
    assert(!recorder.ReserveCapture(frame, true));
    for (unsigned i = 0; i < 10000; ++i) recorder.Event("present_result", frame, "rotation_fixture", 1);
    recorder.Failure(5, 8, "synthetic", "failure\n<script>");
    std::puts("PASS visual pixels, policy, completion records, rotation and concurrent counters");
}
