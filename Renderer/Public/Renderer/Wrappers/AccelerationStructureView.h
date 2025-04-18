#pragma once

#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/Vulkan/ForwardDeclares.h>

#include <Common/Platform/CompilerWarnings.h>
#include <Common/Math/CoreNumericTypes.h>

namespace ngine::Rendering
{
	struct LogicalDevice;

	PUSH_CLANG_WARNINGS
	DISABLE_CLANG_WARNING("-Wunguarded-availability-new")

	struct AccelerationStructureView
	{
		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN
			return m_accelerationStructure != nullptr;
#elif RENDERER_METAL
			return m_accelerationStructure != nullptr;
#else
			return false;
#endif
		}

#if RENDERER_VULKAN
		[[nodiscard]] operator VkAccelerationStructureKHR() const
		{
			return m_accelerationStructure;
		}
#elif RENDERER_METAL
		[[nodiscard]] operator id<MTLAccelerationStructure>() const
		{
			return m_accelerationStructure;
		}
#endif

#if RENDERER_VULKAN
		using ResourceIdentifier = uint64;
#elif RENDERER_METAL
		using ResourceIdentifier = MTLResourceID;
#else
		using ResourceIdentifier = void*;
#endif
		[[nodiscard]] ResourceIdentifier GetResourceIdentifier(const LogicalDevice& logicalDevice) const;

		struct BuildSizesInfo
		{
			size accelerationStructureSize;
			size updateScratchSize;
			size buildScratchSize;
		};
	protected:
#if RENDERER_VULKAN
		VkAccelerationStructureKHR m_accelerationStructure{nullptr};
#elif RENDERER_METAL
		id<MTLAccelerationStructure> m_accelerationStructure;
#endif
	};

	struct PrimitiveAccelerationStructureView : public AccelerationStructureView
	{
	};

	struct InstanceAccelerationStructureView : public AccelerationStructureView
	{
	};

	POP_CLANG_WARNINGS
}
