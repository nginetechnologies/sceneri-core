#pragma once

#if RENDERER_METAL
@protocol MTLDevice;
@protocol MTLCommandQueue;
@protocol MTLCommandBuffer;
@protocol MTLRenderCommandEncoder;
@protocol MTLParallelRenderCommandEncoder;
@protocol MTLComputeCommandEncoder;
@protocol MTLBlitCommandEncoder;
@protocol MTLAccelerationStructureCommandEncoder;
@protocol MTLSamplerState;
@protocol MTLDepthStencilState;
@protocol MTLHeap;
@protocol MTLBuffer;
@protocol MTLTexture;
@protocol MTLLibrary;
@protocol MTLFunction;
@protocol MTLRenderPipelineState;
@protocol MTLComputePipelineState;
@protocol MTLArgumentEncoder;
@protocol MTLEvent;
@protocol MTLFence;
@protocol MTLAccelerationStructure;

@class MTLAccelerationStructureTriangleGeometryDescriptor;
@class MTLPrimitiveAccelerationStructureDescriptor;
@class MTLIndirectInstanceAccelerationStructureDescriptor;

typedef struct MTLResourceID MTLResourceID;

@class CAMetalLayer;
@protocol CAMetalDrawable;
#endif
