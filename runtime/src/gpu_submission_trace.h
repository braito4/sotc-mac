// SPDX-License-Identifier: GPL-2.0-or-later
// SotC diagnostic: bounded CPU records, not GPU execution breadcrumbs. No GPU reads.
#pragma once
#include "gpu_tiling_trace.h"
#include "gpu_render_trace.h"
#include "gpu_indirect_origin.h"
#include "gpu_buffer_chain.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <memory>
#include <span>
#include <unordered_map>
#include <utility>

namespace SotcGpuTrace {
using Key = std::uintptr_t;
using U64 = std::uint64_t;
inline bool Enabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("SOTC_GPU_SUBMIT_TRACE");
        return value && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}
struct Command {
    Key buffer{};
    U64 recording{}, ordinal{};
    std::array<char, 48> name{};
    U64 shader[2]{}, count[3]{};
};
enum class NoteKind : std::size_t {
    GuestPass, GuestDraw, GuestDispatch, NativeDraw, NativeDispatch, NativeCopy,
    NativeBlit, NativeClear, NativeBarrier, NativeState, NativeRenderScope, Other, Count
};
inline NoteKind ClassifyNote(const char* name) {
    const auto starts = [name](const char* prefix) {
        return std::strncmp(name, prefix, std::strlen(prefix)) == 0;
    };
    if (std::strcmp(name, "pass") == 0) return NoteKind::GuestPass;
    if (std::strcmp(name, "draw") == 0 || std::strcmp(name, "draw_indexed") == 0 ||
        std::strcmp(name, "draw_indirect") == 0) return NoteKind::GuestDraw;
    if (std::strcmp(name, "dispatch") == 0 || std::strcmp(name, "dispatch_indirect") == 0)
        return NoteKind::GuestDispatch;
    if (starts("vkCmdDraw")) return NoteKind::NativeDraw;
    if (starts("vkCmdDispatch")) return NoteKind::NativeDispatch;
    if (starts("vkCmdCopy") || starts("vkCmdResolveImage") || starts("vkCmdUpdateBuffer"))
        return NoteKind::NativeCopy;
    if (starts("vkCmdBlitImage")) return NoteKind::NativeBlit;
    if (starts("vkCmdClear") || starts("vkCmdFillBuffer")) return NoteKind::NativeClear;
    if (starts("vkCmdPipelineBarrier")) return NoteKind::NativeBarrier;
    if (starts("vkCmdBind") || starts("vkCmdSet") || starts("vkCmdPush"))
        return NoteKind::NativeState;
    if (starts("vkCmdBeginRendering") || starts("vkCmdEndRendering"))
        return NoteKind::NativeRenderScope;
    return NoteKind::Other;
}
// Native identities are process-local handles, not guest shader hashes or GPU progress.
struct NativeEvent {
    U64 source{},destination{},source_offset{},destination_offset{},bytes{},source_length{},destination_length{};
    Key buffer{}, encoder{}, pipeline{}, indirect_buffer{}, backend_pipeline{};
    U64 recording{}, ordinal{}, offset{}, checkpoint{}, progress_event{};
    std::array<char, 32> event{};
    std::array<char, 160> entry{};
    bool entry_truncated{};
    std::array<U64, 3> groups{}, threads{};
};
struct ComputeBinding {
    U64 recording{}, dispatch_event{}, pipeline{}, resource{}, offset{}, length{}, slot{}, inline_bytes{};
};
struct Commands {
    struct Checkpoint { NativeEvent end, operation; };
    std::deque<Checkpoint> checkpoints;
    U64 checkpoint_total{};
    std::shared_ptr<const NativeEvent> last_operation;
    void CaptureCheckpoint(const NativeEvent& event) {
        if (!SotcBufferChain::Enabled()) return;
        if (event.checkpoint) {
            ++checkpoint_total;
            if (checkpoints.size()<2048) checkpoints.push_back({event,last_operation ? *last_operation : NativeEvent{}});
        } else if (event.encoder && !std::strstr(event.event.data(),"end_request")) {
            last_operation=std::make_shared<NativeEvent>(event);
        }
    }
    U64 compute_binding_total{};
    std::deque<ComputeBinding> compute_bindings;
    U64 render_count{};
    std::deque<SotcRenderTrace::Scope> render_scopes;
    static constexpr std::size_t NativeLimit = 256;
    static constexpr std::size_t NativePrefix = 192; // first encoders plus final 64 events
    U64 native_total{};
    std::deque<NativeEvent> native_tail;
    void AddNative(NativeEvent event) {
        ++native_total;
        if (native_tail.size() == NativeLimit) native_tail.erase(native_tail.begin()+NativePrefix);
        native_tail.push_back(event);
    }
    static constexpr std::size_t Limit = 96;
    static constexpr std::size_t GuestLimit = 128;
    U64 recording{}, total{};
    std::deque<Command> tail;
    std::deque<Command> guest_tail;
    // Counts cover all observed notes, including those evicted from the tail.
    // Guest and native notes can describe the same work; never sum them as draws.
    std::array<U64, static_cast<std::size_t>(NoteKind::Count)> counts{};
    Command first_draw{}, last_draw{}, first_dispatch{}, last_dispatch{};
    void Add(const Command& command) {
        ++total;
        const auto kind = ClassifyNote(command.name.data());
        ++counts[static_cast<std::size_t>(kind)];
        if (kind == NoteKind::GuestDraw) Remember(first_draw, last_draw, command);
        if (kind == NoteKind::GuestDispatch) Remember(first_dispatch, last_dispatch, command);
        if (kind == NoteKind::GuestDraw || kind == NoteKind::GuestDispatch) AppendGuest(command);
        AppendTail(command);
    }
    void Merge(const Commands& other) {
        checkpoint_total += other.checkpoint_total;
        for (const auto& p:other.checkpoints) if(checkpoints.size()<2048) checkpoints.push_back(p);
        compute_binding_total += other.compute_binding_total;
        for(const auto& b:other.compute_bindings) if(compute_bindings.size()<128) compute_bindings.push_back(b);
        render_count += other.render_count;
        for(const auto& scope : other.render_scopes) if(render_scopes.size()<32) render_scopes.push_back(scope);
        const auto combined_native_total = native_total + other.native_total;
        for (const auto& event : other.native_tail) AddNative(event);
        native_total = combined_native_total;
        total += other.total;
        for (std::size_t i = 0; i < counts.size(); ++i) counts[i] += other.counts[i];
        MergeCandidates(first_draw, last_draw, other.first_draw, other.last_draw);
        MergeCandidates(first_dispatch, last_dispatch, other.first_dispatch, other.last_dispatch);
        for (const auto& command : other.tail) AppendTail(command);
        for (const auto& command : other.guest_tail) AppendGuest(command);
    }
private:
    static void Remember(Command& first, Command& last, const Command& command) {
        if (!first.ordinal) first = command;
        last = command;
    }
    static void MergeCandidates(Command& first, Command& last,
                                const Command& other_first, const Command& other_last) {
        if (!first.ordinal) first = other_first;
        if (other_last.ordinal) last = other_last;
    }
    void AppendTail(const Command& command) {
        if (tail.size() == Limit) tail.pop_front();
        tail.push_back(command);
    }
    void AppendGuest(const Command& command) {
        if (guest_tail.size()==GuestLimit) guest_tail.pop_front();
        guest_tail.push_back(command);
    }
};
struct Dependency { Key semaphore{}; U64 value{}; };
struct Submission {
    Key scheduler{}, timeline{};
    U64 tick{};
    int result{};
    bool returned{};
    U64 submitted_ns{}, returned_ns{};
    std::array<Dependency, 4> waits{}, signals{};
    std::uint32_t wait_count{}, signal_count{};
    std::array<Key, 32> buffers{};
    std::array<U64, 32> recordings{};
    std::uint32_t buffer_count{};
    U64 missing_buffers{};
    Commands commands;
};
struct MetalObservation {
    bool progress_enabled{}, progress_available{};
    U64 progress_encoded{}, progress_reached{}, progress_omitted{}, progress_event{};
    bool available{}, retained_references{}, memory_available{};
    int status{-1};
    std::int64_t error{};
    U64 allocated{}, recommended{};
};
using MetalProbe = std::function<MetalObservation()>;
struct MetalChunk {
    U64 id{};
    Key buffer{};
    U64 recording{};
    bool commit_returned{}, scheduled{}, callback{};
    int status{};
    std::int64_t error{};
    U64 commit_ns{}, returned_ns{}, scheduled_ns{}, completed_ns{};
    // The native bridge captures only a weak command-buffer reference.
    // Invoke outside the journal lock and only while reporting a failure/stall.
    MetalProbe probe;
};
inline U64 MonotonicNs() {
    return static_cast<U64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
class Recorder {
public:
    static constexpr std::size_t MaxOpen = 256, MaxSubmissions = 512, MaxMetal = 1024;
    explicit Recorder(U64 (*clock)() = MonotonicNs) : clock_ns(clock) {}
    U64 Begin(Key buffer) {
        std::lock_guard lock{mutex};
        if (!open.contains(buffer) && open.size() == MaxOpen) {
            open.erase(open.begin()); ++evictions;
        }
        open[buffer] = {};
        return open[buffer].recording = ++record_serial;
    }
    void Note(Key buffer, const char* name, U64 first = 0, U64 second = 0,
              U64 x = 0, U64 y = 0, U64 z = 0) {
        std::lock_guard lock{mutex};
        auto it = open.find(buffer);
        if (it == open.end()) { ++missing_notes; return; }
        Command command{};
        command.buffer = buffer;
        command.recording = it->second.recording;
        command.ordinal = it->second.total + 1;
        std::snprintf(command.name.data(), command.name.size(), "%s", name);
        command.shader[0] = first; command.shader[1] = second;
        command.count[0] = x; command.count[1] = y; command.count[2] = z;
        it->second.Add(command);
    }
    void ComputeBuffer(Key buffer, ComputeBinding binding) {
        std::lock_guard lock{mutex};auto it=open.find(buffer);
        if(it==open.end()) {++missing_notes;return;}
        auto& c=it->second;++c.compute_binding_total;
        if(c.compute_bindings.size()>=128) return;
        binding.recording=c.recording;binding.dispatch_event=c.native_total+1;
        c.compute_bindings.push_back(binding);
    }
    void RenderBegin(Key buffer, SotcRenderTrace::Scope scope) {
        std::lock_guard lock{mutex}; auto it=open.find(buffer);
        if(it==open.end()) { ++missing_notes;return; }
        ++it->second.render_count;
        if(it->second.render_scopes.size()>=32) return;
        scope.buffer=buffer;scope.recording=it->second.recording;
        scope.begin_event=it->second.native_total;
        it->second.render_scopes.push_back(std::move(scope));
    }
    void RenderDraw(Key buffer, SotcRenderTrace::Draw draw) {
        std::lock_guard lock{mutex};auto it=open.find(buffer);
        if(it==open.end()) { ++missing_notes;return; }
        if(it->second.render_count<=32 && !it->second.render_scopes.empty()) it->second.render_scopes.back().Add(draw);
    }
    void Native(Key buffer, NativeEvent event) {
        std::lock_guard lock{mutex};
        auto it = open.find(buffer);
        if (it == open.end()) { ++missing_notes; return; }
        event.buffer = buffer;
        event.recording = it->second.recording;
        event.ordinal = it->second.native_total + 1;
        it->second.CaptureCheckpoint(event);
        if(!it->second.render_scopes.empty() && it->second.render_count<=32 &&
           std::strcmp(event.event.data(), "render_checkpoint")==0) {
            auto& scope=it->second.render_scopes.back();
            scope.end_event=event.ordinal;scope.checkpoint=event.checkpoint;
            scope.progress_event=event.progress_event;
        }
        it->second.AddNative(event);
    }
    void Submit(Submission submission, std::span<const Key> buffers) {
        std::lock_guard lock{mutex};
        submission.submitted_ns = clock_ns();
        submission.buffer_count = static_cast<std::uint32_t>(buffers.size());
        std::size_t buffer_index = 0;
        for (Key buffer : buffers) {
            if (auto it = open.find(buffer); it != open.end()) {
                if (buffer_index < submission.recordings.size())
                    submission.recordings[buffer_index] = it->second.recording;
                submission.commands.Merge(it->second);
                open.erase(it);
            } else ++submission.missing_buffers;
            ++buffer_index;
        }
        for (std::size_t i = 0; i < buffers.size() && i < submission.buffers.size(); ++i)
            submission.buffers[i] = buffers[i];
        if (submissions.size() == MaxSubmissions) { submissions.pop_front(); ++evictions; }
        submissions.push_back(std::move(submission));
        // Heavy render details stay in the eight most recent submissions.
        // Counts remain so older reports explicitly disclose missing details.
        if(submissions.size()>8) {
            submissions[submissions.size()-9].commands.render_scopes.clear();
            submissions[submissions.size()-9].commands.checkpoints.clear();
        }
    }
    void Returned(Key scheduler, U64 tick, int result) {
        std::lock_guard lock{mutex};
        for (auto it = submissions.rbegin(); it != submissions.rend(); ++it)
            if (it->scheduler == scheduler && it->tick == tick) {
                if (!it->returned) it->returned_ns = clock_ns();
                it->returned = true; it->result = result; return;
            }
    }
    U64 CommitMetal(Key buffer, U64 recording, MetalProbe probe = {}) {
        std::lock_guard lock{mutex};
        if (metal.size() == MaxMetal) { metal.pop_front(); ++evictions; }
        const U64 id = ++metal_serial;
        metal.push_back({.id = id, .buffer = buffer, .recording = recording,
                         .commit_ns = clock_ns(), .probe = std::move(probe)});
        return id;
    }
    // 0: commit returned, 1: scheduled callback, 2: completion callback.
    void MetalEvent(U64 id, int event, int status = 0, std::int64_t error = 0) {
        std::lock_guard lock{mutex};
        for (auto it = metal.rbegin(); it != metal.rend(); ++it) if (it->id == id) {
            if (event == 0 && !it->commit_returned) {
                it->returned_ns = clock_ns(); it->commit_returned = true;
            }
            if (event == 1 && !it->scheduled) {
                it->scheduled_ns = clock_ns(); it->scheduled = true;
            }
            if (event == 2 && !it->callback) {
                it->completed_ns = clock_ns(); it->callback = true;
                it->status = status; it->error = error;
            }
            return;
        }
        ++missing_metal_events;
    }
    void ReportMetalFailure(std::FILE* output, U64 id) {
        MetalChunk failed{};
        Commands commands;
        Key scheduler{};
        U64 tick{};
        bool retained = false;
        bool submitted = false;
        U64 now{};
        const char* source = "not-retained";
        {
            std::lock_guard lock{mutex};
            now = clock_ns();
            for (const auto& chunk : metal) if (chunk.id == id) {
                failed = chunk; retained = true; break;
            }
            if (retained) {
                for (auto it = submissions.rbegin(); it != submissions.rend(); ++it) {
                    for (std::size_t i = 0; i < it->buffer_count && i < 32; ++i) {
                        if (it->buffers[i] == failed.buffer && it->recordings[i] == failed.recording) {
                            commands = it->commands; scheduler = it->scheduler; tick = it->tick;
                            submitted = true; source = "submission";
                            break;
                        }
                    }
                    if (submitted) break;
                }
                if (!submitted) {
                    if (auto it = open.find(failed.buffer); it != open.end() &&
                        it->second.recording == failed.recording) {
                        commands = it->second; source = "open-recording";
                    }
                }
            }
        }
        std::fprintf(output, "GPU submission trace: FAILED_METAL chunk=%llu retained=%d "
                     "buffer=%#llx recording=%llu scheduler=%#llx tick=%llu "
                     "status=%d error=%lld notes=%llu retained_notes=%zu source=%s; "
                     "CPU submission/recording candidates, not precise failing GPU instructions\n",
                     (unsigned long long)id, int(retained), (unsigned long long)failed.buffer,
                     (unsigned long long)failed.recording, (unsigned long long)scheduler,
                     (unsigned long long)tick, failed.status, (long long)failed.error,
                     (unsigned long long)commands.total, commands.tail.size(), source);
        if (retained) {
            PrintMetalTiming(output, failed, now);
            PrintMetalObservation(output, failed, now);
        }
        if(SotcIndirectOrigin::Enabled()) SotcIndirectOrigin::Get().Report(output);
        PrintCommands(output, commands);
        std::fflush(output);
    }
    void Report(std::FILE* output, Key scheduler, U64 tick, U64 completed) {
        if(SotcIndirectOrigin::Enabled()) SotcIndirectOrigin::Get().Report(output);
        if (SotcTilingTrace::Enabled()) SotcTilingTrace::Get().Report(output);
        // Copy under lock; printing must not block recording/completion callbacks.
        std::deque<Submission> selected;
        std::deque<MetalChunk> chunks;
        U64 lost{}, notes_lost{}, callbacks_lost{}, now{};
        {
            std::lock_guard lock{mutex};
            now = clock_ns();
            for (const auto& submission : submissions)
                if (submission.scheduler == scheduler &&
                    (submission.tick == tick || submission.tick == completed))
                    selected.push_back(submission);
            chunks = metal; lost = evictions; notes_lost = missing_notes;
            callbacks_lost = missing_metal_events;
        }
        std::fprintf(output, "GPU submission trace: CPU-recorded candidates, not per-command GPU progress; "
                     "scheduler=%#llx awaited=%llu completed=%llu evictions=%llu missing_notes=%llu "
                     "missing_metal_events=%llu\n", (unsigned long long)scheduler,
                     (unsigned long long)tick, (unsigned long long)completed,
                     (unsigned long long)lost, (unsigned long long)notes_lost,
                     (unsigned long long)callbacks_lost);
        bool found = false;
        for (const auto& submission : selected) {
            found |= submission.tick == tick;
            std::fprintf(output, "GPU submission trace: %s tick=%llu timeline=%#llx submit_returned=%d "
                         "result=%d command_buffers=%u missing_buffers=%llu notes=%llu retained=%zu\n",
                         submission.tick == tick ? "AWAITED" : "LAST_COMPLETED",
                         (unsigned long long)submission.tick, (unsigned long long)submission.timeline,
                         int(submission.returned), submission.result, submission.buffer_count,
                         (unsigned long long)submission.missing_buffers,
                         (unsigned long long)submission.commands.total, submission.commands.tail.size());
            std::fprintf(output, "GPU submission trace: submit_timing age_ms=%.3f return_after_ms=%.3f; "
                         "host observations, missing=-1\n", Milliseconds(now, submission.submitted_ns),
                         submission.returned ? Milliseconds(submission.returned_ns, submission.submitted_ns) : -1.0);
            for (std::uint32_t i = 0; i < submission.wait_count && i < 4; ++i)
                PrintDependency(output, "wait", submission.waits[i]);
            for (std::uint32_t i = 0; i < submission.signal_count && i < 4; ++i)
                PrintDependency(output, "signal", submission.signals[i]);
            for (std::uint32_t i = 0; i < submission.buffer_count && i < 32; ++i) {
                std::fprintf(output, "GPU submission trace: buffer[%u]=%#llx recording=%llu\n", i,
                             (unsigned long long)submission.buffers[i],
                             (unsigned long long)submission.recordings[i]);
                for (const auto& chunk : chunks) if (chunk.buffer == submission.buffers[i] &&
                                                     chunk.recording == submission.recordings[i]) {
                    std::fprintf(output, "GPU submission trace: Metal chunk=%llu commit_returned=%d "
                                 "scheduled_callback=%d completed_callback=%d status=%d error=%lld\n",
                                 (unsigned long long)chunk.id, int(chunk.commit_returned),
                                 int(chunk.scheduled), int(chunk.callback), chunk.status,
                                 (long long)chunk.error);
                    PrintMetalTiming(output, chunk, now);
                    PrintMetalObservation(output, chunk, now);
                }
            }
            PrintCommands(output, submission.commands);
        }
        if (!found) std::fputs("GPU submission trace: awaited submission not retained or submit not reached\n", output);
        std::fflush(output);
    }
private:
    static void PrintMetalObservation(std::FILE* output, const MetalChunk& chunk, U64 now) {
        MetalObservation state{};
        bool failed = false;
        try {
            if (chunk.probe) state = chunk.probe();
        } catch (...) {
            failed = true;
        }
        std::fprintf(output,"GPU submission trace: Metal progress chunk=%llu enabled=%d available=%d encoded=%llu reached=%llu omitted=%llu event=%#llx; GPU event prefix, diagnostic signals can alter timing\n",
            (unsigned long long)chunk.id,int(state.progress_enabled),int(state.progress_available),
            (unsigned long long)state.progress_encoded,(unsigned long long)state.progress_reached,(unsigned long long)state.progress_omitted,(unsigned long long)state.progress_event);
        std::fprintf(output, "GPU submission trace: Metal native chunk=%llu probe_registered=%d "
                     "available=%d probe_error=%d report_ns=%llu status=%d error=%lld "
                     "retained_references=%d memory_available=%d allocated_bytes=%llu recommended_bytes=%llu; "
                     "point-in-time CPU property observation, unavailable is not zero pressure\n",
                     (unsigned long long)chunk.id, int(bool(chunk.probe)), int(state.available),
                     int(failed), (unsigned long long)now,
                     state.available ? state.status : -1, (long long)state.error,
                     int(state.retained_references), int(state.memory_available), (unsigned long long)state.allocated,
                     (unsigned long long)state.recommended);
    }
    static double Milliseconds(U64 end, U64 start) {
        return end >= start ? double(end - start) / 1e6 : -1.0;
    }
    static void PrintMetalTiming(std::FILE* output, const MetalChunk& chunk, U64 now) {
        std::fprintf(output, "GPU submission trace: Metal timing chunk=%llu age_ms=%.3f "
                     "commit_return_after_ms=%.3f scheduled_after_ms=%.3f completed_after_ms=%.3f; "
                     "host observations from commit registration, missing=-1\n", (unsigned long long)chunk.id,
                     Milliseconds(now, chunk.commit_ns),
                     chunk.commit_returned ? Milliseconds(chunk.returned_ns, chunk.commit_ns) : -1.0,
                     chunk.scheduled ? Milliseconds(chunk.scheduled_ns, chunk.commit_ns) : -1.0,
                     chunk.callback ? Milliseconds(chunk.completed_ns, chunk.commit_ns) : -1.0);
    }
    static void PrintCommands(std::FILE* output, const Commands& commands) {
        if(commands.compute_binding_total) {
            std::fprintf(output,"GPU compute bindings: total=%llu retained=%zu omitted=%llu limit=128; CPU binding metadata, no access direction or GPU content read\n",
                (unsigned long long)commands.compute_binding_total,commands.compute_bindings.size(),
                (unsigned long long)(commands.compute_binding_total-commands.compute_bindings.size()));
            for(const auto& b:commands.compute_bindings) std::fprintf(output,
                "GPU compute binding: recording=%llu dispatch_event=%llu pipeline=%#llx resource=%#llx offset=%llu length=%llu slot=%llu inline_bytes=%llu\n",
                (unsigned long long)b.recording,(unsigned long long)b.dispatch_event,(unsigned long long)b.pipeline,
                (unsigned long long)b.resource,(unsigned long long)b.offset,(unsigned long long)b.length,
                (unsigned long long)b.slot,(unsigned long long)b.inline_bytes);
        }
        if(commands.render_count) std::fprintf(output,
            "GPU render coverage: total=%llu retained=%zu omitted=%llu limit=32; regular render scopes per submission, CPU encoding only\n",
            (unsigned long long)commands.render_count, commands.render_scopes.size(),
            (unsigned long long)(commands.render_count-commands.render_scopes.size()));
        for(const auto& scope:commands.render_scopes) scope.Report(output);
        if(commands.checkpoint_total) {
            std::fprintf(output,"GPU checkpoint map: total=%llu retained=%zu omitted=%llu; last observed operation before signal, not causal attribution\n",
                (unsigned long long)commands.checkpoint_total,commands.checkpoints.size(),
                (unsigned long long)(commands.checkpoint_total-commands.checkpoints.size()));
            for(const auto& p:commands.checkpoints) std::fprintf(output,
                "GPU checkpoint map: recording=%llu event=%#llx checkpoint=%llu kind=%s encoder=%#llx operation=%s operation_ordinal=%llu pipeline=%#llx entry=%s groups=%llu,%llu,%llu indirect=%#llx offset=%llu\n",
                (unsigned long long)p.end.recording,(unsigned long long)p.end.progress_event,
                (unsigned long long)p.end.checkpoint,p.end.event.data(),(unsigned long long)p.operation.encoder,
                p.operation.event.data(),(unsigned long long)p.operation.ordinal,(unsigned long long)p.operation.pipeline,
                p.operation.entry.data(),(unsigned long long)p.operation.groups[0],(unsigned long long)p.operation.groups[1],
                (unsigned long long)p.operation.groups[2],(unsigned long long)p.operation.indirect_buffer,
                (unsigned long long)p.operation.offset);
        }
        std::fprintf(output, "GPU native trace: retained=%zu total=%llu omitted=%llu limit=%zu prefix_limit=192 tail_limit=64; CPU encoding only, handles are process-local; indirect dimensions unread\n",
            commands.native_tail.size(), (unsigned long long)commands.native_total,
            (unsigned long long)(commands.native_total-commands.native_tail.size()), Commands::NativeLimit);
        for (const auto& e : commands.native_tail) {
            std::fprintf(output, "GPU native trace: recording=%llu ordinal=%llu buffer=%#llx event=%s encoder=%#llx pipeline=%#llx entry=%s truncated=%u groups=%llu,%llu,%llu threads=%llu,%llu,%llu indirect=%#llx offset=%llu backend_pipeline=%#llx checkpoint=%llu progress_event=%#llx source=%#llx destination=%#llx source_offset=%llu destination_offset=%llu bytes=%llu source_length=%llu destination_length=%llu\n",
                (unsigned long long)e.recording, (unsigned long long)e.ordinal,
                (unsigned long long)e.buffer, e.event.data(), (unsigned long long)e.encoder,
                (unsigned long long)e.pipeline, e.entry[0] ? e.entry.data() : "-", unsigned(e.entry_truncated),
                (unsigned long long)e.groups[0], (unsigned long long)e.groups[1], (unsigned long long)e.groups[2],
                (unsigned long long)e.threads[0], (unsigned long long)e.threads[1], (unsigned long long)e.threads[2],
                (unsigned long long)e.indirect_buffer, (unsigned long long)e.offset, (unsigned long long)e.backend_pipeline, (unsigned long long)e.checkpoint, (unsigned long long)e.progress_event,
                (unsigned long long)e.source,(unsigned long long)e.destination,
                (unsigned long long)e.source_offset,(unsigned long long)e.destination_offset,
                (unsigned long long)e.bytes,(unsigned long long)e.source_length,(unsigned long long)e.destination_length);
        }
        static constexpr const char* names[] = {
            "guest_pass", "guest_draw", "guest_dispatch", "native_draw", "native_dispatch",
            "native_copy", "native_blit", "native_clear", "native_barrier", "native_state",
            "native_render_scope", "other"
        };
        static_assert(std::size(names) == static_cast<std::size_t>(NoteKind::Count));
        std::fprintf(output, "GPU submission trace: note_summary total=%llu",
                     (unsigned long long)commands.total);
        for (std::size_t i = 0; i < commands.counts.size(); ++i)
            std::fprintf(output, " %s=%llu", names[i], (unsigned long long)commands.counts[i]);
        std::fputs("; all observed CPU notes, guest/native may describe the same work\n", output);
        PrintCandidate(output, "first_guest_draw", commands.first_draw);
        PrintCandidate(output, "last_guest_draw", commands.last_draw);
        PrintCandidate(output, "first_guest_dispatch", commands.first_dispatch);
        PrintCandidate(output, "last_guest_dispatch", commands.last_dispatch);
        const auto guest_total=commands.counts[static_cast<std::size_t>(NoteKind::GuestDraw)]+
                               commands.counts[static_cast<std::size_t>(NoteKind::GuestDispatch)];
        std::fprintf(output,"GPU submission trace: guest_candidates retained=%zu total=%llu omitted=%llu limit=%zu; CPU recording candidates, not GPU progress\n",
                     commands.guest_tail.size(),(unsigned long long)guest_total,
                     (unsigned long long)(guest_total-commands.guest_tail.size()),Commands::GuestLimit);
        for (const auto& command : commands.guest_tail)
            PrintCandidate(output,"retained_guest",command);
        for (const auto& command : commands.tail)
            std::fprintf(output, "GPU submission trace: cb_command=%llu recording=%llu buffer=%#llx %s "
                         "shader=%016llx,%016llx count=%llu,%llu,%llu\n",
                         (unsigned long long)command.ordinal, (unsigned long long)command.recording,
                         (unsigned long long)command.buffer,
                         command.name.data(), (unsigned long long)command.shader[0],
                         (unsigned long long)command.shader[1], (unsigned long long)command.count[0],
                         (unsigned long long)command.count[1], (unsigned long long)command.count[2]);
    }
    static void PrintCandidate(std::FILE* output, const char* label, const Command& command) {
        if (!command.ordinal) return;
        std::fprintf(output, "GPU submission trace: %s_candidate ordinal=%llu recording=%llu buffer=%#llx %s "
                     "shader=%016llx,%016llx count=%llu,%llu,%llu\n", label,
                     (unsigned long long)command.ordinal, (unsigned long long)command.recording,
                     (unsigned long long)command.buffer, command.name.data(),
                     (unsigned long long)command.shader[0], (unsigned long long)command.shader[1],
                     (unsigned long long)command.count[0], (unsigned long long)command.count[1],
                     (unsigned long long)command.count[2]);
    }
    static void PrintDependency(std::FILE* output, const char* kind, const Dependency& d) {
        std::fprintf(output, "GPU submission trace: %s semaphore=%#llx value=%llu\n", kind,
                     (unsigned long long)d.semaphore, (unsigned long long)d.value);
    }
    std::mutex mutex;
    std::unordered_map<Key, Commands> open;
    std::deque<Submission> submissions;
    std::deque<MetalChunk> metal;
    U64 (*clock_ns)();
    U64 record_serial{}, metal_serial{}, evictions{}, missing_notes{}, missing_metal_events{};
};
// Completion handlers may outlive ordinary C++ static destruction.
inline Recorder& Get() { static auto* recorder = new Recorder; return *recorder; }
inline void Shader(Key buffer, unsigned kind, U64 first, U64 second, U64 x, U64 y, U64 z) {
    static constexpr const char* kinds[] = {"pass", "draw", "draw_indexed", "draw_indirect",
                                           "dispatch", "dispatch_indirect"};
    Get().Note(buffer, kind < 6 ? kinds[kind] : "unknown", first, second, x, y, z);
}
} // namespace SotcGpuTrace
