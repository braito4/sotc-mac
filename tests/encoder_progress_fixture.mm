#import <Foundation/Foundation.h>
#include <cassert>
@protocol MTLSharedEvent <NSObject>
@property(nonatomic) uint64_t signaledValue;
@end
@protocol MTLDevice <NSObject>
- (id<MTLSharedEvent>)newSharedEvent;
@end
@protocol MTLCommandBuffer <NSObject>
@property(nonatomic, strong) id<MTLDevice> device;
- (void)encodeSignalEvent:(id<MTLSharedEvent>)event value:(uint64_t)value;
@end
#include "gpu_encoder_progress.h"
@interface Event : NSObject <MTLSharedEvent>
@property(nonatomic) uint64_t signaledValue;
@end
@implementation Event
@end
@interface Device : NSObject <MTLDevice>
@end
@implementation Device
- (id<MTLSharedEvent>)newSharedEvent { return [Event new]; }
@end
@interface Command : NSObject <MTLCommandBuffer>
@property(nonatomic, strong) id<MTLDevice> device;
@property(nonatomic) uint64_t signals;
@property(nonatomic, strong) id<MTLSharedEvent> lastEvent;
@end
@implementation Command
- (void)encodeSignalEvent:(id<MTLSharedEvent>)event value:(uint64_t)value {
    assert(value==_signals+1); ++_signals; _lastEvent=event;
}
@end
struct Owner { id<MTLCommandBuffer> cmd; uintptr_t trace_buffer{123}; SotcEncoderProgress::State encoder_progress; };
int main() {
    @autoreleasepool {
        Command* command=[Command new];command.device=[Device new];
        Owner o{command};SotcGpuTrace::Get().Begin(123);
        auto base=[] { SotcGpuTrace::MetalObservation s{};s.available=true;return s; };
        for(int i=0;i<130;++i) SotcEncoderProgress::Mark(o,"compute");
        auto probe=SotcEncoderProgress::Observe(o,base);
        auto state=probe();
        if(!SotcEncoderProgress::Enabled()) { assert(command.signals==0 && !state.progress_enabled);return 0; }
        assert(command.signals==128 && state.progress_encoded==128 && state.progress_omitted==2);
        assert(state.progress_reached==0); // CPU encoding never masquerades as GPU progress.
        command.lastEvent.signaledValue=7;
        assert(probe().progress_reached==7);
        Command* second=[Command new];second.device=command.device;o.cmd=second;
        SotcEncoderProgress::Mark(o,"render");
        auto next=SotcEncoderProgress::Observe(o,base);
        assert(next().progress_encoded==1 && next().progress_reached==0 && next().progress_omitted==0);
        assert(probe().progress_reached==7); // a new command cannot overwrite the older event.
    }
}
