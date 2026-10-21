// SPDX-License-Identifier: GPL-2.0-or-later
// SotC diagnostic: bounded CPU records, not GPU execution breadcrumbs. No GPU reads.
#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
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
struct Commands {
    static constexpr std::size_t Limit = 96;
    U64 recording{}, total{};
    std::deque<Command> tail;
    void Add(const Command& command) {
        ++total;
        if (tail.size() == Limit) tail.pop_front();
        tail.push_back(command);
    }
};
struct Dependency { Key semaphore{}; U64 value{}; };
struct Submission {
    Key scheduler{}, timeline{};
    U64 tick{};
    int result{};
    bool returned{};
    std::array<Dependency, 4> waits{}, signals{};
    std::uint32_t wait_count{}, signal_count{};
    std::array<Key, 32> buffers{};
    std::array<U64, 32> recordings{};
    std::uint32_t buffer_count{};
    U64 missing_buffers{};
    Commands commands;
};
struct MetalChunk {
    U64 id{};
    Key buffer{};
    U64 recording{};
    bool commit_returned{}, scheduled{}, callback{};
    int status{};
    std::int64_t error{};
};
class Recorder {
public:
    static constexpr std::size_t MaxOpen = 256, MaxSubmissions = 512, MaxMetal = 1024;
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
    void Submit(Submission submission, std::span<const Key> buffers) {
        std::lock_guard lock{mutex};
        submission.buffer_count = static_cast<std::uint32_t>(buffers.size());
        std::size_t buffer_index = 0;
        for (Key buffer : buffers) {
            if (auto it = open.find(buffer); it != open.end()) {
                if (buffer_index < submission.recordings.size())
                    submission.recordings[buffer_index] = it->second.recording;
                const U64 omitted = it->second.total - it->second.tail.size();
                submission.commands.total += omitted;
                for (const auto& command : it->second.tail) submission.commands.Add(command);
                open.erase(it);
            } else ++submission.missing_buffers;
            ++buffer_index;
        }
        for (std::size_t i = 0; i < buffers.size() && i < submission.buffers.size(); ++i)
            submission.buffers[i] = buffers[i];
        if (submissions.size() == MaxSubmissions) { submissions.pop_front(); ++evictions; }
        submissions.push_back(std::move(submission));
    }
    void Returned(Key scheduler, U64 tick, int result) {
        std::lock_guard lock{mutex};
        for (auto it = submissions.rbegin(); it != submissions.rend(); ++it)
            if (it->scheduler == scheduler && it->tick == tick) {
                it->returned = true; it->result = result; return;
            }
    }
    U64 CommitMetal(Key buffer, U64 recording) {
        std::lock_guard lock{mutex};
        if (metal.size() == MaxMetal) { metal.pop_front(); ++evictions; }
        const U64 id = ++metal_serial;
        metal.push_back({.id = id, .buffer = buffer, .recording = recording});
        return id;
    }
    // 0: commit returned, 1: scheduled callback, 2: completion callback.
    void MetalEvent(U64 id, int event, int status = 0, std::int64_t error = 0) {
        std::lock_guard lock{mutex};
        for (auto it = metal.rbegin(); it != metal.rend(); ++it) if (it->id == id) {
            if (event == 0) it->commit_returned = true;
            if (event == 1) it->scheduled = true;
            if (event == 2) { it->callback = true; it->status = status; it->error = error; }
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
        const char* source = "not-retained";
        {
            std::lock_guard lock{mutex};
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
        PrintCommands(output, commands);
        std::fflush(output);
    }
    void Report(std::FILE* output, Key scheduler, U64 tick, U64 completed) {
        // Copy under lock; printing must not block recording/completion callbacks.
        std::deque<Submission> selected;
        std::deque<MetalChunk> chunks;
        U64 lost{}, notes_lost{}, callbacks_lost{};
        {
            std::lock_guard lock{mutex};
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
            for (std::uint32_t i = 0; i < submission.wait_count && i < 4; ++i)
                PrintDependency(output, "wait", submission.waits[i]);
            for (std::uint32_t i = 0; i < submission.signal_count && i < 4; ++i)
                PrintDependency(output, "signal", submission.signals[i]);
            for (std::uint32_t i = 0; i < submission.buffer_count && i < 32; ++i) {
                std::fprintf(output, "GPU submission trace: buffer[%u]=%#llx recording=%llu\n", i,
                             (unsigned long long)submission.buffers[i],
                             (unsigned long long)submission.recordings[i]);
                for (const auto& chunk : chunks) if (chunk.buffer == submission.buffers[i] &&
                                                     chunk.recording == submission.recordings[i])
                    std::fprintf(output, "GPU submission trace: Metal chunk=%llu commit_returned=%d "
                                 "scheduled_callback=%d completed_callback=%d status=%d error=%lld\n",
                                 (unsigned long long)chunk.id, int(chunk.commit_returned),
                                 int(chunk.scheduled), int(chunk.callback), chunk.status,
                                 (long long)chunk.error);
            }
            PrintCommands(output, submission.commands);
        }
        if (!found) std::fputs("GPU submission trace: awaited submission not retained or submit not reached\n", output);
        std::fflush(output);
    }
private:
    static void PrintCommands(std::FILE* output, const Commands& commands) {
        for (const auto& command : commands.tail)
            std::fprintf(output, "GPU submission trace: cb_command=%llu recording=%llu buffer=%#llx %s "
                         "shader=%016llx,%016llx count=%llu,%llu,%llu\n",
                         (unsigned long long)command.ordinal, (unsigned long long)command.recording,
                         (unsigned long long)command.buffer,
                         command.name.data(), (unsigned long long)command.shader[0],
                         (unsigned long long)command.shader[1], (unsigned long long)command.count[0],
                         (unsigned long long)command.count[1], (unsigned long long)command.count[2]);
    }
    static void PrintDependency(std::FILE* output, const char* kind, const Dependency& d) {
        std::fprintf(output, "GPU submission trace: %s semaphore=%#llx value=%llu\n", kind,
                     (unsigned long long)d.semaphore, (unsigned long long)d.value);
    }
    std::mutex mutex;
    std::unordered_map<Key, Commands> open;
    std::deque<Submission> submissions;
    std::deque<MetalChunk> metal;
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
