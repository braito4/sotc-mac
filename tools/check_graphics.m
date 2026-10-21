/* Independent Metal prerequisite check. Synthetic pixels only; no game input.
 * A successful offscreen draw is not a SotC renderer or displayed game frame.
 * GPL-2.0-or-later. */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>
#include "../runtime/src/diagnostic_control.h"
static int unavailable(const char *stage, NSError *error) {
    fprintf(stderr, "STOP_HOST_GRAPHICS: %s%s%s\n", stage, error ? ": " : "",
            error ? error.localizedDescription.UTF8String : "");
    return 24;
}
int main(void) {
    if (sotc_diagnostic_control_start()) return 28;
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            fprintf(stderr, "STOP_HOST_GRAPHICS: Metal exposes no default device to this process (%lu enumerated).\n",
                    (unsigned long)MTLCopyAllDevices().count);
            fputs("Run this check in a native macOS Terminal session with GPU access. This result does not establish whether the physical Mac lacks a GPU.\n", stderr);
            return 24;
        }
        printf("Metal device: %s\n", device.name.UTF8String);
        NSError *error = nil;
        NSString *source = @"#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "vertex float4 vertex_main(uint id [[vertex_id]]) {\n"
            "  const float2 positions[3] = {float2(-1,-1),float2(3,-1),float2(-1,3)};\n"
            "  return float4(positions[id],0,1);\n"
            "}\n"
            "fragment float4 fragment_main() { return float4(0.25,0.5,0.75,1); }\n";
        id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
        if (!library) return unavailable("shader compilation failed", error);
        MTLRenderPipelineDescriptor *pipeline = [MTLRenderPipelineDescriptor new];
        pipeline.vertexFunction = [library newFunctionWithName:@"vertex_main"];
        pipeline.fragmentFunction = [library newFunctionWithName:@"fragment_main"];
        pipeline.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
        id<MTLRenderPipelineState> state = [device newRenderPipelineStateWithDescriptor:pipeline error:&error];
        if (!state) return unavailable("render pipeline creation failed", error);
        MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
            width:4 height:4 mipmapped:NO];
        descriptor.storageMode = MTLStorageModeShared;
        descriptor.usage = MTLTextureUsageRenderTarget;
        id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
        id<MTLCommandQueue> queue = [device newCommandQueue];
        if (!texture || !queue) return unavailable("texture/command queue allocation failed", nil);
        id<MTLCommandBuffer> command = [queue commandBuffer];
        if (!command) return unavailable("command buffer allocation failed", nil);
        MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(1, 0, 0, 0);
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
        if (!encoder) return unavailable("render encoder creation failed", nil);
        [encoder setRenderPipelineState:state];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding];
        [command commit];
        [command waitUntilCompleted];
        if (command.status != MTLCommandBufferStatusCompleted) return unavailable("GPU draw failed", command.error);
        unsigned char pixels[4 * 4 * 4] = {0};
        [texture getBytes:pixels bytesPerRow:16 fromRegion:MTLRegionMake2D(0, 0, 4, 4) mipmapLevel:0];
        const int expected[4] = {64, 128, 191, 255};
        for (unsigned i = 0; i < sizeof(pixels); ++i) {
            if (abs((int)pixels[i] - expected[i % 4]) > 1)
                return unavailable("GPU pixel readback does not match the synthetic shader", nil);
        }
        puts("PASS: Metal shader compilation, render pipeline, GPU triangle draw and pixel readback (synthetic, offscreen; no game frame).");
        return 0;
    }
}
