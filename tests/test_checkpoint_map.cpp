#include "gpu_submission_trace.h"
#include <cassert>
int main() {
    using namespace SotcGpuTrace;
    Commands c;
    for (unsigned i=1;i<=940;++i) {
        NativeEvent op{};op.encoder=100+i;op.recording=7;op.ordinal=i*3;
        op.pipeline=900+i;std::snprintf(op.event.data(),op.event.size(),"dispatch_direct");
        c.CaptureCheckpoint(op);c.AddNative(op);
        NativeEvent end{};end.recording=7;end.checkpoint=i;end.progress_event=42;
        c.CaptureCheckpoint(end);c.AddNative(end);
    }
    if (!SotcBufferChain::Enabled()) { assert(c.checkpoints.empty()); return 0; }
    assert(c.native_tail.size()==256);
    assert(c.checkpoints.size()==940);
    assert(c.checkpoints[215].end.checkpoint==216);
    assert(c.checkpoints[215].operation.encoder==316);
    assert(c.checkpoints[215].operation.pipeline==1116);
    Commands merged;merged.Merge(c);
    assert(merged.checkpoints.size()==940 && merged.checkpoint_total==940);
    for(unsigned i=0;i<3000;++i) { NativeEvent e{};e.checkpoint=941+i;c.CaptureCheckpoint(e); }
    assert(c.checkpoints.size()==2048 && c.checkpoint_total==3940);
    SotcBufferChain::reuse={};
    SotcBufferChain::Reject(true,17,2048,4096);
    SotcBufferChain::Reject(false,0,16,32);
    assert(SotcBufferChain::reuse.busy==1 && SotcBufferChain::reuse.oversize==1);
    assert(SotcBufferChain::reuse.tick==17 && SotcBufferChain::reuse.requested==4096);
}
