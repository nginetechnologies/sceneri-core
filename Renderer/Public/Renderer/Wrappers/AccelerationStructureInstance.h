#pragma once

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>

#include <Common/EnumFlagOperators.h>

namespace ngine::Rendering
{
	PUSH_CLANG_WARNINGS
	DISABLE_CLANG_WARNING("-Wunguarded-availability-new")

	struct AccelerationStructureInstance
	{
		enum class Flags : uint8
		{
			DisableTriangleFaceCulling = 1 << 0
		};

		AccelerationStructureInstance(
			const Math::Matrix3x4f transform,
			const uint32 instanceCustomIndex,
			const uint8 mask,
			const EnumFlags<Flags> flags,
			const AccelerationStructureView::ResourceIdentifier primitiveAccelerationStructureResourceIdentifier
		);
	private:
#if RENDERER_VULKAN
		VkAccelerationStructureInstanceKHR m_instance;
#elif RENDERER_METAL
		MTLIndirectAccelerationStructureInstanceDescriptor m_instance;
#endif
	};

	POP_CLANG_WARNINGS

	ENUM_FLAG_OPERATORS(AccelerationStructureInstance::Flags);
}
