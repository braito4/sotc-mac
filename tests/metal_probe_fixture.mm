// SPDX-License-Identifier: GPL-2.0-or-later
// Real ARC weak-capture contract with CPU objects, no Metal framework/device.
#import <Foundation/Foundation.h>
#include <cassert>
#include <cstdio>

@protocol MTLDevice <NSObject>
@property(readonly) NSUInteger currentAllocatedSize;
@property(readonly) NSUInteger recommendedMaxWorkingSetSize;
@end
@protocol MTLCommandBuffer <NSObject>
@property(readonly) id<MTLDevice> device;
@property(readonly) NSError* error;
@property(readonly) BOOL retainedReferences;
@property(readonly) NSInteger status;
@end
#include "gpu_metal_probe.h"

static unsigned destroyed = 0;
@interface ProbeDevice : NSObject <MTLDevice>
@property NSUInteger currentAllocatedSize;
@property NSUInteger recommendedMaxWorkingSetSize;
@end
@implementation ProbeDevice
@end
@interface ProbeCommand : NSObject <MTLCommandBuffer>
@property(strong) id<MTLDevice> device;
@property(strong) NSError* error;
@property BOOL retainedReferences;
@property NSInteger status;
@property(strong) NSObject* resource;
@end
@implementation ProbeCommand
- (void)dealloc { ++destroyed; }
@end

int main() {
    using namespace SotcGpuTrace;
    MetalProbe probe;
    __weak ProbeCommand* weak_command;
    __weak NSObject* weak_resource;
    @autoreleasepool {
        ProbeCommand* command = [ProbeCommand new];
        weak_command = command;
        command.status = 2; command.retainedReferences = YES;
        command.resource = [NSObject new]; weak_resource = command.resource;
        ProbeDevice* device = [ProbeDevice new];
        command.device = device;
        device.currentAllocatedSize = 12345; device.recommendedMaxWorkingSetSize = 54321;
        probe = ObserveMetalCommand(command);
        auto state = probe();
        assert(state.available && state.status == 2 && state.error == 0);
        assert(state.retained_references && state.allocated == 12345 && state.recommended == 54321);
        assert(state.memory_available);
        command.status = 3; device.currentAllocatedSize = 23456;
        assert(probe().status == 3 && probe().allocated == 23456);
        command.status = 5;
        command.error = [NSError errorWithDomain:@"synthetic" code:8 userInfo:nil];
        assert(probe().error == 8 && probe().status == 5);
        command.retainedReferences = NO;
        assert(!probe().retained_references);
        command.device = nil;
        assert(probe().available && !probe().memory_available);
    }
    // Journal/probe copies must not retain either the command or its resources.
    MetalProbe copied = probe;
    assert(destroyed == 1 && weak_command == nil && weak_resource == nil);
    assert(!probe().available && !copied().available);
    assert(!ObserveMetalCommand(nil)().available);
    std::puts("PASS: production weak Metal property bridge; fresh status/error/memory, expired/nil object and no ownership extension; CPU only");
}
