// SPDX-License-Identifier: GPL-2.0-or-later
// CPU-only synthetic metadata: no Vulkan, Metal, game input or device access.
#include "gpu_submission_trace.h"
#include <unistd.h>
#include <cassert>
#include <string>
#include <thread>
#include <vector>
using namespace SotcGpuTrace;
static U64 fixture_now;
static U64 FixtureClock() { return fixture_now; }
extern "C" int fixture_control_start(void);
static std::string Report(Recorder& recorder, Key scheduler, U64 tick, U64 done = 0) {
    auto* file = std::tmpfile(); assert(file);
    recorder.Report(file, scheduler, tick, done);
    std::rewind(file);
    std::string result; char buffer[1024];
    while (const auto n = std::fread(buffer, 1, sizeof(buffer), file)) result.append(buffer, n);
    std::fclose(file); return result;
}
static std::string FailureReport(Recorder& recorder, U64 chunk) {
    auto* file = std::tmpfile(); assert(file);
    recorder.ReportMetalFailure(file, chunk);
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
    auto failure = FailureReport(recorder, chunk2);
    assert(failure.find("FAILED_METAL chunk=2 retained=1") != std::string::npos);
    assert(failure.find("tick=2 status=5 error=99") != std::string::npos);
    assert(failure.find(" draw ") != std::string::npos && failure.find(" dispatch ") == std::string::npos);
    // Completion can fail before queue submission is registered. Retain its
    // open recording, and do not borrow notes from a reused command buffer.
    const auto early = recorder.Begin(300);
    recorder.Note(300, "early_dispatch", 321, 0, 7, 8, 9);
    const auto early_chunk = recorder.CommitMetal(300, early);
    recorder.MetalEvent(early_chunk, 2, 5, 8);
    failure = FailureReport(recorder, early_chunk);
    assert(failure.find("tick=0 status=5 error=8") != std::string::npos);
    assert(failure.find("early_dispatch") != std::string::npos);
    recorder.Begin(300); recorder.Note(300, "unrelated_reuse");
    failure = FailureReport(recorder, early_chunk);
    assert(failure.find("unrelated_reuse") == std::string::npos);
    assert(FailureReport(recorder, 999).find("retained=0") != std::string::npos);
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

    // A large tail must not hide early compute/draw candidates. Aggregate two
    // buffers without counting their retained tails twice, including an empty
    // buffer and an unknown buffer whose missing evidence stays explicit.
    Recorder summaries;
    summaries.Begin(600);
    summaries.Note(600, "dispatch", 101, 0, 2, 3, 4);
    summaries.Note(600, "draw_indexed", 202, 303, 6, 2);
    for (int i = 0; i < 26762; ++i) summaries.Note(600, "vkCmdBindPipeline");
    summaries.Begin(601);
    summaries.Note(601, "draw_indirect", 404, 505, 8, 3);
    summaries.Note(601, "dispatch_indirect", 606, 0, 9, 10, 11);
    summaries.Note(601, "vkCmdDrawIndexedIndirect");
    summaries.Note(601, "vkCmdDispatchIndirect");
    summaries.Note(601, "vkCmdCopyBufferToImage");
    summaries.Note(601, "vkCmdBlitImage");
    summaries.Note(601, "vkCmdFillBuffer");
    summaries.Note(601, "vkCmdPipelineBarrier2KHR");
    summaries.Note(601, "vkCmdBeginRenderingKHR");
    summaries.Note(601, "future_unknown_command");
    summaries.Note(601, "pass");
    summaries.Begin(602);
    const Key many[] = {600, 601, 602, 603};
    summaries.Submit(Batch(20, 1), many);
    auto all = Report(summaries, 20, 1);
    assert(all.find("missing_buffers=1 notes=26775 retained=96") != std::string::npos);
    assert(all.find("note_summary total=26775 guest_pass=1 guest_draw=2 guest_dispatch=2 "
                    "native_draw=1 native_dispatch=1 native_copy=1 native_blit=1 "
                    "native_clear=1 native_barrier=1 native_state=26762 "
                    "native_render_scope=1 other=1") != std::string::npos);
    assert(all.find("first_guest_dispatch_candidate ordinal=1 recording=1 buffer=0x258 dispatch "
                    "shader=0000000000000065,0000000000000000 count=2,3,4") != std::string::npos);
    assert(all.find("last_guest_dispatch_candidate ordinal=2 recording=2 buffer=0x259 dispatch_indirect ") != std::string::npos);
    assert(all.find("first_guest_draw_candidate ordinal=2 recording=1 buffer=0x258 draw_indexed ") != std::string::npos);
    assert(all.find("last_guest_draw_candidate ordinal=1 recording=2 buffer=0x259 draw_indirect ") != std::string::npos);
    assert(all.find("cb_command=1 recording=1") == std::string::npos);
    // Summary storage is fixed, below 1 KiB per recording/submission, plus the
    // original bounded 96-note tail. It cannot grow with batch length.
    static_assert(sizeof(Commands) <= 1024);
    summaries.Begin(600); summaries.Note(600, "vkCmdCopyImage");
    const Key recycled[] = {600}; summaries.Submit(Batch(20, 2), recycled);
    auto reset = Report(summaries, 20, 2);
    assert(reset.find("guest_draw=0 guest_dispatch=0") != std::string::npos);
    assert(reset.find("_candidate ordinal=") == std::string::npos);

    // Host timestamps use one monotonic clock, with absent observations -1.
    // Metal callbacks can run before commit returns: do not infer GPU order
    // from CPU call return order or overwrite the first observed timestamp.
    Recorder timed(FixtureClock);
    fixture_now = 1000000;
    const auto timed_generation = timed.Begin(700);
    const Key timed_buffers[] = {700}; timed.Submit(Batch(30, 1), timed_buffers);
    fixture_now = 2000000;
    const auto timed_chunk = timed.CommitMetal(700, timed_generation);
    fixture_now = 3000000; timed.MetalEvent(timed_chunk, 1);
    fixture_now = 4000000; timed.MetalEvent(timed_chunk, 2, 4);
    fixture_now = 5000000; timed.MetalEvent(timed_chunk, 0);
    fixture_now = 6000000; timed.Returned(30, 1, 0);
    fixture_now = 10000000;
    timed.MetalEvent(timed_chunk, 1); timed.MetalEvent(timed_chunk, 2, 5, 8);
    auto timing = Report(timed, 30, 1);
    assert(timing.find("submit_timing age_ms=9.000 return_after_ms=5.000") != std::string::npos);
    assert(timing.find("age_ms=8.000 commit_return_after_ms=3.000 "
                       "scheduled_after_ms=1.000 completed_after_ms=2.000") != std::string::npos);
    assert(timing.find("status=4 error=0") != std::string::npos);
    const auto pending_generation = timed.Begin(700);
    timed.Submit(Batch(30, 2), timed_buffers);
    const auto pending_chunk = timed.CommitMetal(700, pending_generation);
    fixture_now = 20000000;
    auto pending = Report(timed, 30, 2);
    assert(pending.find("submit_timing age_ms=10.000 return_after_ms=-1.000") != std::string::npos);
    assert(pending.find("age_ms=10.000 commit_return_after_ms=-1.000 "
                        "scheduled_after_ms=-1.000 completed_after_ms=-1.000") != std::string::npos);
    assert(pending.find("Metal chunk=1 ") == std::string::npos);
    assert(FailureReport(timed, pending_chunk).find("completed_after_ms=-1.000") != std::string::npos);

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
    std::puts("PASS: bounded submission metadata, full-batch summaries, host timings, reuse, callbacks, dependencies and concurrent snapshots");
}
