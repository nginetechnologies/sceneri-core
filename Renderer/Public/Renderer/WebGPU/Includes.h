#pragma once

#if RENDERER_WEBGPU
#if PLATFORM_EMSCRIPTEN
#include <emscripten/html5_webgpu.h>
#elif RENDERER_WEBGPU_DAWN
#include <dawn/webgpu.h>
#elif RENDERER_WEBGPU_WGPU_NATIVE
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>
#endif
#endif


#if RENDERER_WEBGPU && !RENDERER_WEBGPU_DAWN
using WGPUUncapturedErrorCallback = WGPUErrorCallback;

inline static constexpr WGPUBufferBindingType WGPUBufferBindingType_BindingNotUsed = WGPUBufferBindingType_Undefined;
inline static constexpr WGPUTextureSampleType WGPUTextureSampleType_BindingNotUsed = WGPUTextureSampleType_Undefined;
inline static constexpr WGPUStorageTextureAccess WGPUStorageTextureAccess_BindingNotUsed = WGPUStorageTextureAccess_Undefined;
inline static constexpr WGPUSamplerBindingType WGPUSamplerBindingType_BindingNotUsed = WGPUSamplerBindingType_Undefined;
#endif
