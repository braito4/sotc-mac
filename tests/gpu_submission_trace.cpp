// SPDX-License-Identifier: GPL-2.0-or-later
// CPU-only synthetic metadata: no Vulkan, Metal, game input or device access.
#include "gpu_submission_trace.h"
#include <unistd.h>
#include <cassert>
#include <string>
#include <thread>
#include <vector>
using namespace SotcGpuTrace;
extern "C" int fixture_control_start(void);
static std::string Report(Recorder& recorder, Key scheduler, U64 tick, U64 done = 0) {
    auto* file = std::tmpfile(); assert(file);
    recorder.Report(file, scheduler, tick, done);
    std::rewind(file);
    std::string result; char buffer[1024];
    while (const auto n = std::fread(buffer, 1, sizeof(buffer), file)) result.append(buffer, n);
    std::fclose(file); return result;
}
static Submission Batch(Key scheduler, U64 tick) {
    Submission s{}; s.scheduler = scheduler; s.tick = tick; s.timeline = 55;
    s.wait_count = 1; s.waits[0] = {66, 7};
    s.signal_count = 1; s.signals[0] = {55, tick}; return s;
}
int main(int argc, char** argv) {
    assert(argc == 2);
    if (std::string(argv[1]) == "supervised") {
        assert(Enabled()); assert(fixture_control_start() == 0);
        Recorder recorder;
        recorder.Begin(100); recorder.Note(100, "dispatch", 123, 0, 2, 3, 4);
        const Key buffers[] = {100}; recorder.Submit(Batch(10, 1), buffers);
        recorder.Returned(10, 1, 0);
        recorder.Report(stderr, 10, 1, 0);
        std::puts("GPU watchdog: synthetic pending submission"); std::fflush(stdout);
        for (;;) pause();
    }
    assert(Enabled() == (std::string(argv[1]) == "1"));
    if (std::string(argv[1]) != "1") return 0;
    Recorder recorder;
    const auto first = recorder.Begin(100);
    recorder.Note(100, "dispatch", 123, 0, 2, 3, 4);
    const auto chunk1 = recorder.CommitMetal(100, first);
    const Key buffers[] = {100};
    recorder.Submit(Batch(10, 1), buffers);
    auto prepared = Report(recorder, 10, 1);
    assert(prepared.find("AWAITED tick=1") != std::string::npos);
    assert(prepared.find("submit_returned=0") != std::string::npos);
    assert(prepared.find("wait semaphore=0x42 value=7") != std::string::npos);
    assert(prepared.find("count=2,3,4") != std::string::npos);
    recorder.Returned(10, 1, 0);
    recorder.MetalEvent(chunk1, 0); recorder.MetalEvent(chunk1, 1);
    recorder.MetalEvent(chunk1, 2, 4);
    auto complete = Report(recorder, 10, 1);
    assert(complete.find("submit_returned=1 result=0") != std::string::npos);
    assert(complete.find("scheduled_callback=1 completed_callback=1 status=4") != std::string::npos);

    // A recycled handle must not inherit any old command or Metal callback.
    const auto second = recorder.Begin(100); assert(second != first);
    recorder.Note(100, "draw", 456, 789, 3, 1);
    const auto chunk2 = recorder.CommitMetal(100, second);
    recorder.Submit(Batch(10, 2), buffers);
    recorder.MetalEvent(chunk2, 2, 5, 99);
    auto reused = Report(recorder, 10, 2);
    assert(reused.find("Metal chunk=1 ") == std::string::npos);
    assert(reused.find("Metal chunk=2 ") != std::string::npos);
    assert(reused.find("status=5 error=99") != std::string::npos);
    assert(reused.find(" dispatch ") == std::string::npos);
    assert(Report(recorder, 11, 2).find("not retained or submit not reached") != std::string::npos);
    assert(Report(recorder, 10, 2, 1).find("LAST_COMPLETED tick=1") != std::string::npos);

    // Every retained tail item keeps its original command ordinal.
    recorder.Begin(200);
    for (int i = 0; i < 200; ++i) recorder.Note(200, "copy");
    const Key other[] = {200}; recorder.Submit(Batch(10, 3), other);
    auto bounded = Report(recorder, 10, 3);
    assert(bounded.find("notes=200 retained=96") != std::string::npos);
    assert(bounded.find("cb_command=105 ") != std::string::npos);
    assert(bounded.find("cb_command=200 ") != std::string::npos);
    assert(bounded.find("cb_command=1 ") == std::string::npos);

    // Concurrent recorders/callbacks and a concurrent report use one coherent snapshot.
    std::vector<std::thread> workers;
    for (int t = 0; t < 12; ++t) workers.emplace_back([&,t] {
        for (int i = 0; i < 40; ++i) {
            const Key buffer = 1000 + t;
            const auto generation = recorder.Begin(buffer);
            recorder.Note(buffer, "draw", t + 1, 0, i + 1);
            const auto chunk = recorder.CommitMetal(buffer, generation);
            const Key commands[] = {buffer}; recorder.Submit(Batch(100 + t, i + 1), commands);
            recorder.Returned(100 + t, i + 1, 0); recorder.MetalEvent(chunk, 2, 4);
        }
    });
    for (int i = 0; i < 40; ++i) (void)Report(recorder, 100, 1);
    for (auto& worker : workers) worker.join();
    for (int t = 0; t < 12; ++t) {
        auto snapshot = Report(recorder, 100 + t, 40);
        assert(snapshot.find("AWAITED tick=40") != std::string::npos);
        assert(snapshot.find("completed_callback=1 status=4") != std::string::npos);
    }
    // Eviction and missing records stay explicit, never guessed from another tick.
    for (int i = 0; i < 530; ++i) {
        recorder.Begin(5000); const Key keys[] = {5000}; recorder.Submit(Batch(200, i), keys);
    }
    assert(Report(recorder, 200, 1).find("not retained or submit not reached") != std::string::npos);
    std::puts("PASS: bounded submission metadata, reuse, callbacks, dependencies and concurrent snapshots");
}
