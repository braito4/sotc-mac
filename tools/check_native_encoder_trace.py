"""Exercise the installed Metal trace wrapper with metadata doubles; no GPU device."""
import os
import subprocess
from build_gpu import ROOT, source_path
from check_buffer_copy_chunks import function


def main():
    source = source_path(ROOT)
    frame = (source/'gpu/shadps4/video_core/renderer_metal/mtl_frame.mm').read_text()
    start = frame.index('void TraceNative(')
    body = frame.index(') {', start) + 2
    wrapper = frame[start:body] + function(frame[body:], '{')
    work = ROOT/'work/native-encoder-trace-fixture'
    work.mkdir(exist_ok=True)
    cpp = work/'fixture.mm'
    cpp.write_text(r'''
#import <Foundation/Foundation.h>
#include "gpu_submission_trace.h"
#include <cassert>
#include <string>
using u64 = std::uint64_t;
namespace Metal {
struct Shader { std::string entry; };
struct ComputePipeline { void* state; std::array<unsigned, 3> threads_per_group; Shader* shader; };
}
struct Owned { SotcGpuTrace::Key trace_buffer; };
''' + wrapper + r'''
int main() {
    @autoreleasepool {
        Owned o{123};
        SotcGpuTrace::Get().Begin(o.trace_buffer);
        Metal::Shader shader{std::string(200, 'x')};
        int marker = 1;
        Metal::ComputePipeline pipeline{&marker, {32, 2, 1}, &shader};
        NSObject* encoder = [NSObject new];
        TraceNative(o, "compute_begin", encoder);
        TraceNative(o, "dispatch_direct", encoder, &pipeline, {7, 8, 9});
        TraceNative(o, "dispatch_indirect", encoder, &pipeline, {}, 456, 48);
        TraceNative(o, "compute_end_request", encoder);
        SotcGpuTrace::Submission batch{}; batch.scheduler = 5; batch.tick = 1;
        const SotcGpuTrace::Key buffers[] = {123};
        SotcGpuTrace::Get().Submit(batch, buffers);
        auto* out = std::tmpfile(); assert(out);
        SotcGpuTrace::Get().Report(out, 5, 1, 0); std::rewind(out);
        std::string log; char bytes[1024];
        while (auto n = std::fread(bytes, 1, sizeof(bytes), out)) log.append(bytes, n);
        std::fclose(out);
        if (SotcGpuTrace::Enabled()) {
            assert(log.find("retained=4 total=4 omitted=0") != std::string::npos);
            assert(log.find("truncated=1 groups=7,8,9 threads=32,2,1") != std::string::npos);
            assert(log.find("groups=0,0,0 threads=32,2,1 indirect=0x1c8 offset=48") != std::string::npos);
            assert(log.find("event=compute_begin") < log.find("event=compute_end_request"));
        } else {
            assert(log.find("retained=0 total=0 omitted=0") != std::string::npos);
            assert(log.find("event=") == std::string::npos);
        }
    }
}
''')
    binary = work/'fixture'
    subprocess.run(['clang++', '-std=c++20', '-fobjc-arc', '-framework', 'Foundation',
                    '-I'+str(ROOT/'runtime/src'), str(cpp), '-o', str(binary)], check=True)
    for enabled in ('0', '1'):
        subprocess.run([str(binary)], env=dict(os.environ, SOTC_GPU_SUBMIT_TRACE=enabled),
                       timeout=10, check=True)
    print('PASS: installed native trace wrapper opt-in, direct/indirect metadata, entry truncation and encoder order; Foundation doubles only, no GPU execution.')


if __name__ == '__main__':
    main()
