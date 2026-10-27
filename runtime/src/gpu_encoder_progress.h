// SPDX-License-Identifier: GPL-2.0-or-later
// Opt-in GPU signal checkpoints, outside encoders. No waits or extra submissions.
#pragma once
#include "gpu_submission_trace.h"
namespace SotcEncoderProgress {
inline bool Enabled() {
    static const bool enabled=[] { const char* p=std::getenv("SOTC_GPU_ENCODER_PROGRESS");
        return p && std::strcmp(p,"1")==0 && SotcGpuTrace::Enabled(); }();
    return enabled;
}
struct State {
    __weak id<MTLCommandBuffer> command;
    id<MTLSharedEvent> event;
    std::uint64_t encoded{}, omitted{};
};
template<class Owner>
void Mark(Owner& o, const char* kind) {
    if (!Enabled()) return;
    auto& s=o.encoder_progress;
    if (s.command != o.cmd) {
        s.command=o.cmd; s.event=[o.cmd.device newSharedEvent]; s.encoded=0; s.omitted=0;
    }
    // Focused chain profile covers long submissions; still bounded, no waits.
    static const uint64_t limit = [] { const char* p=std::getenv("SOTC_BUFFER_CHAIN");
        return p && std::strcmp(p,"1")==0 ? 2048ull : 128ull; }();
    if (!s.event || s.encoded==limit) { ++s.omitted; return; }
    const auto value=++s.encoded;
    [o.cmd encodeSignalEvent:s.event value:value];
    SotcGpuTrace::NativeEvent note{};
    std::snprintf(note.event.data(),note.event.size(),"%s_checkpoint",kind);
    note.checkpoint=value;
    note.progress_event=reinterpret_cast<std::uintptr_t>((__bridge void*)s.event);
    SotcGpuTrace::Get().Native(o.trace_buffer,note);
}
template<class Owner>
SotcGpuTrace::MetalProbe Observe(Owner& o, SotcGpuTrace::MetalProbe base) {
    // Retain only the diagnostic event. The base probe holds a weak command reference.
    id<MTLSharedEvent> event=o.encoder_progress.command==o.cmd ? o.encoder_progress.event : nil;
    const auto encoded=event ? o.encoder_progress.encoded : 0;
    const auto omitted=o.encoder_progress.command==o.cmd ? o.encoder_progress.omitted : 0;
    const bool enabled=Enabled();
    return [base,event,encoded,omitted,enabled]() {
        auto result=base();
        result.progress_enabled=enabled; result.progress_available=bool(event);
        result.progress_encoded=encoded; result.progress_omitted=omitted;
        result.progress_reached=event ? event.signaledValue : 0;
        result.progress_event=reinterpret_cast<std::uintptr_t>((__bridge void*)event);
        return result;
    };
}
}
