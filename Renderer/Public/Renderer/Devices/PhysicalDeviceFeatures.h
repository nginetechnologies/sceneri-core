#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Rendering
{
	enum class PhysicalDeviceFeatures : uint32
	{
		GeometryShader = 1 << 0,
		TesselationShader = 1 << 1,
		TextureCompressionBC = 1 << 2,
		TextureCompressionETC2 = 1 << 3,
		TextureCompressionASTC_LDR = 1 << 4,
		DepthClamp = 1 << 5,
		DepthBounds = 1 << 6,
		DepthBiasClamp = 1 << 7,
		CubemapArrays = 1 << 8,
		CubemapReadWrite = 1 << 9,
		LayeredRendering = 1 << 10,
		NonSolidFillMode = 1 << 11,
		ShaderInt16 = 1 << 12,
		ShaderFloat16 = 1 << 13,
		BufferDeviceAddress = 1 << 14,
		RayTracingPipeline = 1 << 15,
		AccelerationStructure = 1 << 16,
		AccelerationStructureHostCommands = 1 << 17,
		RayQuery = 1 << 18,
		DescriptorIndexing = 1 << 19,
		ExtendedDynamicState = 1 << 20,
		FragmentStoresAndAtomics = 1 << 21,
		VertexPipelineStoresAndAtomics = 1 << 22,
		PartiallyBoundDescriptorBindings = 1 << 23,
		RuntimeDescriptorArrays = 1 << 24,
		UpdateDescriptorSampleImageAfterBind = 1 << 25,
		NonUniformImageArrayIndexing = 1 << 26,
		ShaderInt64 = 1 << 27,
		SeparateDepthStencilLayout = 1 << 28,
		ReadWriteBuffers = 1 << 29,
		ReadWriteTextures = 1 << 30
	};

	ENUM_FLAG_OPERATORS(PhysicalDeviceFeatures);
}
