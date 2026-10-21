// Original fatal command-buffer diagnostics. No GPU/device APIs or recovery stubs.
#pragma once
#include "gpu_submission_trace.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace Sotc {
struct MetalCommandFailure {
    bool failed{};
    int status{};
    std::int64_t code{};
    const char* domain{};
    const char* description{};
    const char* label{};
    std::uint64_t allocated{}, recommended{}, chunk{}, recording{};
    std::uintptr_t buffer{};
};

inline void CheckMetalCommandFailure(const MetalCommandFailure& failure) {
    if (!failure.failed) return;
    const auto text = [](const char* value) { return value ? value : "unavailable"; };
    std::fprintf(stderr, "STOP_GPU_METAL_COMMAND: status=%d domain=%.256s code=%lld "
                 "description=%.2048s label=%.256s allocated_bytes=%llu "
                 "recommended_working_set_bytes=%llu chunk=%llu buffer=%#llx recording=%llu\n",
                 failure.status, text(failure.domain), (long long)failure.code,
                 text(failure.description), text(failure.label),
                 (unsigned long long)failure.allocated, (unsigned long long)failure.recommended,
                 (unsigned long long)failure.chunk, (unsigned long long)failure.buffer,
                 (unsigned long long)failure.recording);
    if (failure.chunk && SotcGpuTrace::Enabled())
        SotcGpuTrace::Get().ReportMetalFailure(stderr, failure.chunk);
    // A failed native command is not successful GPU progress. Do not signal a
    // substitute fence, retry the workload or wait for the later stall timeout.
    std::fflush(stderr);
    std::_Exit(23);
}
} // namespace Sotc
