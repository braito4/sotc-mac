// SPDX-License-Identifier: GPL-2.0-or-later
// Include after the Metal protocol declarations. CPU properties only; no waits.
#pragma once
#ifndef __OBJC__
#error "gpu_metal_probe.h requires Objective-C++"
#endif
#include "gpu_submission_trace.h"

namespace SotcGpuTrace {
inline MetalProbe ObserveMetalCommand(id<MTLCommandBuffer> command) {
    __weak id<MTLCommandBuffer> observed = command;
    return [observed]() -> MetalObservation {
        @autoreleasepool {
            // Promote weak to strong only for this property snapshot. The journal
            // does not extend command/resource ownership between reports.
            id<MTLCommandBuffer> current = observed;
            if (!current) return {};
            id<MTLDevice> device = current.device;
            NSError* error = current.error;
            return {.available = true,
                    .retained_references = bool(current.retainedReferences),
                    .memory_available = bool(device),
                    .status = int(current.status), .error = error ? error.code : 0,
                    .allocated = device.currentAllocatedSize,
                    .recommended = device.recommendedMaxWorkingSetSize};
        }
    };
}
} // namespace SotcGpuTrace
