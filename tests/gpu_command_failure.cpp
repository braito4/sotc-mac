// CPU-only completion metadata. No GPU/device or graphics framework is loaded.
#include "gpu_command_failure.h"
#include <cassert>
#include <string>

int main(int argc, char** argv) {
    assert(argc == 2);
    const bool fail = std::string(argv[1]) != "success";
    const bool unknown = std::string(argv[1]) == "unknown";
    Sotc::MetalCommandFailure result{
        .failed = fail, .status = fail ? 5 : 4, .code = unknown ? 123 : 8,
        .domain = unknown ? nullptr : "MTLCommandBufferErrorDomain",
        .description = unknown ? nullptr : "Synthetic insufficient memory",
        .label = "synthetic command", .allocated = 123456, .recommended = 654321,
        .recording = 1, .buffer = 100,
    };
    if (SotcGpuTrace::Enabled()) {
        auto& recorder = SotcGpuTrace::Get();
        result.recording = recorder.Begin(result.buffer);
        recorder.Note(result.buffer, "dispatch", 456, 0, 2, 3, 4);
        result.chunk = recorder.CommitMetal(result.buffer, result.recording);
        recorder.MetalEvent(result.chunk, 2, result.status, result.code);
    }
    Sotc::CheckMetalCommandFailure(result);
    assert(!fail);
    std::puts("PASS: successful native completion preserves normal execution");
}
